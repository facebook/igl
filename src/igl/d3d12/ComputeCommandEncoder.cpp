/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/ComputeCommandEncoder.h>
#include <igl/d3d12/CommandBuffer.h>
#include <igl/d3d12/ComputePipelineState.h>
#include <igl/d3d12/Buffer.h>
#include <igl/d3d12/Texture.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/DescriptorHeapManager.h>
#include <igl/d3d12/SamplerState.h>

namespace igl::d3d12 {

ComputeCommandEncoder::ComputeCommandEncoder(CommandBuffer& commandBuffer) :
  commandBuffer_(commandBuffer), isEncoding_(true) {
  IGL_LOG_INFO("ComputeCommandEncoder created\n");

  // Set descriptor heaps for this command list
  auto& context = commandBuffer_.getContext();
  DescriptorHeapManager* heapMgr = context.getDescriptorHeapManager();
  ID3D12DescriptorHeap* cbvSrvUavHeap = nullptr;
  ID3D12DescriptorHeap* samplerHeap = nullptr;

  if (heapMgr) {
    cbvSrvUavHeap = heapMgr->getCbvSrvUavHeap();
    samplerHeap = heapMgr->getSamplerHeap();
  }

  // Fallback if manager present but heaps not available
  if (!cbvSrvUavHeap || !samplerHeap) {
    cbvSrvUavHeap = context.getCbvSrvUavHeap();
    samplerHeap = context.getSamplerHeap();
  }

  if (cbvSrvUavHeap && samplerHeap) {
    auto* commandList = commandBuffer_.getCommandList();
    if (commandList) {
      ID3D12DescriptorHeap* heaps[] = {cbvSrvUavHeap, samplerHeap};
      commandList->SetDescriptorHeaps(2, heaps);
      IGL_LOG_INFO("ComputeCommandEncoder: Descriptor heaps set\n");
    }
  }
}

void ComputeCommandEncoder::endEncoding() {
  if (!isEncoding_) {
    return;
  }

  IGL_LOG_INFO("ComputeCommandEncoder::endEncoding()\n");
  isEncoding_ = false;
}

void ComputeCommandEncoder::bindComputePipelineState(
    const std::shared_ptr<IComputePipelineState>& pipelineState) {
  if (!pipelineState) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindComputePipelineState - null pipeline state\n");
    return;
  }

  currentPipeline_ = static_cast<const ComputePipelineState*>(pipelineState.get());

  auto* commandList = commandBuffer_.getCommandList();
  if (!commandList) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindComputePipelineState - null command list\n");
    return;
  }

  // Set compute root signature and pipeline state
  commandList->SetComputeRootSignature(currentPipeline_->getRootSignature());
  commandList->SetPipelineState(currentPipeline_->getPipelineState());

  IGL_LOG_INFO("ComputeCommandEncoder::bindComputePipelineState - PSO and root signature set\n");
}

void ComputeCommandEncoder::dispatchThreadGroups(const Dimensions& threadgroupCount,
                                                 const Dimensions& /*threadgroupSize*/,
                                                 const Dependencies& dependencies) {
  if (!currentPipeline_) {
    IGL_LOG_ERROR("ComputeCommandEncoder::dispatchThreadGroups - no pipeline state bound\n");
    return;
  }

  auto* commandList = commandBuffer_.getCommandList();
  if (!commandList) {
    IGL_LOG_ERROR("ComputeCommandEncoder::dispatchThreadGroups - null command list\n");
    return;
  }

  IGL_LOG_INFO("ComputeCommandEncoder::dispatchThreadGroups(%u, %u, %u)\n",
               threadgroupCount.width, threadgroupCount.height, threadgroupCount.depth);

  // Process dependencies - insert barriers for buffers and textures
  const Dependencies* deps = &dependencies;
  std::vector<ID3D12Resource*> uavResources;

  while (deps) {
    // Handle buffer dependencies
    for (IBuffer* buf : deps->buffers) {
      if (!buf) {
        break;
      }
      auto* d3dBuffer = static_cast<Buffer*>(buf);
      uavResources.push_back(d3dBuffer->getResource());
    }

    // Handle texture dependencies
    for (ITexture* tex : deps->textures) {
      if (!tex) {
        break;
      }
      auto* d3dTexture = static_cast<Texture*>(tex);
      // Ensure texture is in proper state for compute access
      d3dTexture->transitionAll(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      uavResources.push_back(d3dTexture->getResource());
    }

    deps = deps->next;
  }

  // Insert UAV barriers for dependent resources before dispatch
  if (!uavResources.empty()) {
    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    barriers.reserve(uavResources.size());

    for (ID3D12Resource* resource : uavResources) {
      D3D12_RESOURCE_BARRIER barrier = {};
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
      barrier.UAV.pResource = resource;
      barriers.push_back(barrier);
    }

    commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    IGL_LOG_INFO("ComputeCommandEncoder: Inserted %zu UAV barriers before dispatch\n", barriers.size());
  }

  // Bind all cached resources to root parameters
  // Root signature layout (from Device::createComputePipeline):
  // - Parameter 0: Root Constants (b0) - 16 DWORDs
  // - Parameter 1: UAV table (u0-uN)
  // - Parameter 2: SRV table (t0-tN)
  // - Parameter 3: CBV table (b1-bN)
  // - Parameter 4: Sampler table (s0-sN)

  // Bind UAVs (Parameter 1)
  if (boundUavCount_ > 0) {
    commandList->SetComputeRootDescriptorTable(1, cachedUavHandles_[0]);
    IGL_LOG_INFO("ComputeCommandEncoder: Bound %zu UAVs\n", boundUavCount_);
  }

  // Bind SRVs (Parameter 2)
  if (boundSrvCount_ > 0) {
    commandList->SetComputeRootDescriptorTable(2, cachedSrvHandles_[0]);
    IGL_LOG_INFO("ComputeCommandEncoder: Bound %zu SRVs\n", boundSrvCount_);
  }

  // Bind CBVs (Parameter 3)
  if (boundCbvCount_ > 0) {
    // For CBVs, we use root descriptor table, but we cached GPU addresses
    // We need to create a descriptor table for CBVs if we have any
    // For simplicity, if CBV count is 1, we could use SetComputeRootConstantBufferView on a different parameter
    // But to match our root signature, we should use descriptor table
    // For now, log that CBV binding via table is not yet fully implemented
    IGL_LOG_INFO("ComputeCommandEncoder: CBV table binding (%zu CBVs) - using direct root CBV for now\n", boundCbvCount_);
    // Note: This is a simplification. Full implementation would require creating CBV descriptors in a table
  }

  // Bind Samplers (Parameter 4)
  if (boundSamplerCount_ > 0) {
    commandList->SetComputeRootDescriptorTable(4, cachedSamplerHandles_[0]);
    IGL_LOG_INFO("ComputeCommandEncoder: Bound %zu samplers\n", boundSamplerCount_);
  }

  // Dispatch compute work
  // Note: threadgroupSize is embedded in the compute shader ([numthreads(...)])
  commandList->Dispatch(threadgroupCount.width, threadgroupCount.height, threadgroupCount.depth);

  // Insert global UAV barrier after dispatch to ensure compute writes are visible
  D3D12_RESOURCE_BARRIER globalBarrier = {};
  globalBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  globalBarrier.UAV.pResource = nullptr;  // Global UAV barrier
  commandList->ResourceBarrier(1, &globalBarrier);

  IGL_LOG_INFO("ComputeCommandEncoder::dispatchThreadGroups - dispatch complete, global UAV barrier inserted\n");
}

void ComputeCommandEncoder::bindPushConstants(const void* /*data*/,
                                              size_t /*length*/,
                                              size_t /*offset*/) {
  // Push constants not yet implemented for D3D12
  // Requires proper root signature design and backward compatibility considerations
  IGL_LOG_INFO("ComputeCommandEncoder::bindPushConstants - not yet implemented\n");
}

void ComputeCommandEncoder::bindTexture(uint32_t index, ITexture* texture) {
  if (!texture) {
    IGL_LOG_INFO("ComputeCommandEncoder::bindTexture: null texture\n");
    return;
  }

  if (index >= kMaxComputeTextures) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindTexture: index %u exceeds max %zu\n",
                  index, kMaxComputeTextures);
    return;
  }

  auto& context = commandBuffer_.getContext();
  auto* device = context.getDevice();
  auto* heapMgr = context.getDescriptorHeapManager();
  auto* d3dTexture = static_cast<Texture*>(texture);

  if (!heapMgr || !device || !d3dTexture->getResource()) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindTexture: missing resources\n");
    return;
  }

  // Transition texture to shader resource state for compute shader access
  auto* commandList = commandBuffer_.getCommandList();
  if (commandList) {
    d3dTexture->transitionAll(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  }

  // Allocate descriptor and create SRV
  const uint32_t descriptorIndex = nextCbvSrvUavDescriptor_++;
  D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = heapMgr->getCbvSrvUavCpuHandle(descriptorIndex);

  // Create SRV descriptor
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format = textureFormatToDXGIFormat(d3dTexture->getFormat());
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

  auto resourceDesc = d3dTexture->getResource()->GetDesc();
  if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MipLevels = d3dTexture->getNumMipLevels();
    srvDesc.Texture3D.MostDetailedMip = 0;
  } else if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
    if (resourceDesc.DepthOrArraySize > 1) {
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
      srvDesc.Texture2DArray.MipLevels = d3dTexture->getNumMipLevels();
      srvDesc.Texture2DArray.MostDetailedMip = 0;
      srvDesc.Texture2DArray.FirstArraySlice = 0;
      srvDesc.Texture2DArray.ArraySize = resourceDesc.DepthOrArraySize;
    } else {
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      srvDesc.Texture2D.MipLevels = d3dTexture->getNumMipLevels();
      srvDesc.Texture2D.MostDetailedMip = 0;
    }
  } else {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindTexture: unsupported dimension\n");
    return;
  }

  device->CreateShaderResourceView(d3dTexture->getResource(), &srvDesc, cpuHandle);

  cachedSrvHandles_[index] = heapMgr->getCbvSrvUavGpuHandle(descriptorIndex);
  boundSrvCount_ = std::max(boundSrvCount_, static_cast<size_t>(index + 1));

  IGL_LOG_INFO("ComputeCommandEncoder::bindTexture: Created SRV at index %u, descriptor slot %u\n",
               index, descriptorIndex);
}

void ComputeCommandEncoder::bindBuffer(uint32_t index, IBuffer* buffer, size_t offset, size_t /*bufferSize*/) {
  if (!buffer) {
    IGL_LOG_INFO("ComputeCommandEncoder::bindBuffer: null buffer\n");
    return;
  }

  auto* d3dBuffer = static_cast<Buffer*>(buffer);
  auto& context = commandBuffer_.getContext();
  auto* device = context.getDevice();
  auto* heapMgr = context.getDescriptorHeapManager();

  if (!heapMgr || !device) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: no descriptor heap manager or device\n");
    return;
  }

  // Determine buffer type
  const auto bufferType = d3dBuffer->getBufferType();
  const bool isUniformBuffer = (bufferType & BufferDesc::BufferTypeBits::Uniform) != 0;
  const bool isStorageBuffer = (bufferType & BufferDesc::BufferTypeBits::Storage) != 0;

  IGL_LOG_INFO("ComputeCommandEncoder::bindBuffer(%u): isUniform=%d, isStorage=%d\n",
               index, isUniformBuffer, isStorageBuffer);

  if (isStorageBuffer) {
    // Storage buffer - bind as UAV (unordered access view) for read/write
    if (index >= kMaxComputeBuffers) {
      IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: UAV index %u exceeds max %zu\n",
                    index, kMaxComputeBuffers);
      return;
    }

    const uint32_t descriptorIndex = nextCbvSrvUavDescriptor_++;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = heapMgr->getCbvSrvUavCpuHandle(descriptorIndex);

    // Create UAV descriptor for RWByteAddressBuffer (raw buffer)
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R32_TYPELESS; // Required for raw buffers
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = offset / 4; // Element offset (4 bytes per element)
    uavDesc.Buffer.NumElements = static_cast<UINT>((d3dBuffer->getSizeInBytes() - offset) / 4);
    uavDesc.Buffer.StructureByteStride = 0; // 0 for raw buffers
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW; // RAW flag required for ByteAddressBuffer

    device->CreateUnorderedAccessView(d3dBuffer->getResource(), nullptr, &uavDesc, cpuHandle);

    cachedUavHandles_[index] = heapMgr->getCbvSrvUavGpuHandle(descriptorIndex);
    boundUavCount_ = std::max(boundUavCount_, static_cast<size_t>(index + 1));

    IGL_LOG_INFO("ComputeCommandEncoder::bindBuffer: Created UAV at index %u, descriptor slot %u\n",
                 index, descriptorIndex);
  } else if (isUniformBuffer) {
    // Uniform buffer - bind as CBV (constant buffer view)
    if (index >= kMaxComputeBuffers) {
      IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: CBV index %u exceeds max %zu\n",
                    index, kMaxComputeBuffers);
      return;
    }

    // Align offset to 256 bytes (D3D12 requirement for CBV)
    const size_t alignedOffset = (offset + 255) & ~255;
    if (offset != alignedOffset) {
      IGL_LOG_INFO("ComputeCommandEncoder::bindBuffer: Aligned offset %zu -> %zu for CBV\n",
                   offset, alignedOffset);
    }

    cachedCbvAddresses_[index] = d3dBuffer->gpuAddress(alignedOffset);
    boundCbvCount_ = std::max(boundCbvCount_, static_cast<size_t>(index + 1));

    IGL_LOG_INFO("ComputeCommandEncoder::bindBuffer: Cached CBV at index %u, address 0x%llx\n",
                 index, cachedCbvAddresses_[index]);
  } else {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: Buffer must be Uniform or Storage type\n");
  }
}

void ComputeCommandEncoder::bindUniform(const UniformDesc& /*uniformDesc*/, const void* /*data*/) {
  // Single uniform binding not supported in D3D12
  // Use uniform buffers (CBVs) instead
  IGL_LOG_INFO("ComputeCommandEncoder::bindUniform - not supported, use uniform buffers\n");
}

void ComputeCommandEncoder::bindBytes(uint32_t /*index*/, const void* /*data*/, size_t /*length*/) {
  // bindBytes not yet implemented
  // Would need to create a temporary upload buffer and copy data
  IGL_LOG_INFO("ComputeCommandEncoder::bindBytes - not yet implemented\n");
}

void ComputeCommandEncoder::bindImageTexture(uint32_t index, ITexture* texture, TextureFormat /*format*/) {
  if (!texture) {
    IGL_LOG_INFO("ComputeCommandEncoder::bindImageTexture: null texture\n");
    return;
  }

  if (index >= kMaxComputeBuffers) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindImageTexture: index %u exceeds max %zu\n",
                  index, kMaxComputeBuffers);
    return;
  }

  auto& context = commandBuffer_.getContext();
  auto* device = context.getDevice();
  auto* heapMgr = context.getDescriptorHeapManager();
  auto* d3dTexture = static_cast<Texture*>(texture);

  if (!heapMgr || !device || !d3dTexture->getResource()) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindImageTexture: missing resources\n");
    return;
  }

  // Transition texture to UAV state for compute shader read/write access
  auto* commandList = commandBuffer_.getCommandList();
  if (commandList) {
    d3dTexture->transitionAll(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  }

  // Allocate descriptor and create UAV
  const uint32_t descriptorIndex = nextCbvSrvUavDescriptor_++;
  D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = heapMgr->getCbvSrvUavCpuHandle(descriptorIndex);

  // Create UAV descriptor
  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
  uavDesc.Format = textureFormatToDXGIFormat(d3dTexture->getFormat());

  auto resourceDesc = d3dTexture->getResource()->GetDesc();
  if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
    uavDesc.Texture3D.MipSlice = 0;
    uavDesc.Texture3D.FirstWSlice = 0;
    uavDesc.Texture3D.WSize = resourceDesc.DepthOrArraySize;
  } else if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
    if (resourceDesc.DepthOrArraySize > 1) {
      uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
      uavDesc.Texture2DArray.MipSlice = 0;
      uavDesc.Texture2DArray.FirstArraySlice = 0;
      uavDesc.Texture2DArray.ArraySize = resourceDesc.DepthOrArraySize;
      uavDesc.Texture2DArray.PlaneSlice = 0;
    } else {
      uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
      uavDesc.Texture2D.MipSlice = 0;
      uavDesc.Texture2D.PlaneSlice = 0;
    }
  } else {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindImageTexture: unsupported dimension\n");
    return;
  }

  device->CreateUnorderedAccessView(d3dTexture->getResource(), nullptr, &uavDesc, cpuHandle);

  cachedUavHandles_[index] = heapMgr->getCbvSrvUavGpuHandle(descriptorIndex);
  boundUavCount_ = std::max(boundUavCount_, static_cast<size_t>(index + 1));

  IGL_LOG_INFO("ComputeCommandEncoder::bindImageTexture: Created UAV at index %u, descriptor slot %u\n",
               index, descriptorIndex);
}

void ComputeCommandEncoder::bindSamplerState(uint32_t index, ISamplerState* samplerState) {
  if (!samplerState) {
    IGL_LOG_INFO("ComputeCommandEncoder::bindSamplerState: null sampler\n");
    return;
  }

  if (index >= kMaxComputeSamplers) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindSamplerState: index %u exceeds max %zu\n",
                  index, kMaxComputeSamplers);
    return;
  }

  auto& context = commandBuffer_.getContext();
  auto* device = context.getDevice();
  auto* heapMgr = context.getDescriptorHeapManager();
  auto* d3dSampler = static_cast<SamplerState*>(samplerState);

  if (!heapMgr || !device) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindSamplerState: missing resources\n");
    return;
  }

  // Allocate descriptor and create sampler
  const uint32_t descriptorIndex = heapMgr->allocateSampler();
  D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = heapMgr->getSamplerCpuHandle(descriptorIndex);

  // Get sampler desc from IGL sampler state and create D3D12 sampler
  const D3D12_SAMPLER_DESC& samplerDesc = d3dSampler->getDesc();
  device->CreateSampler(&samplerDesc, cpuHandle);

  cachedSamplerHandles_[index] = heapMgr->getSamplerGpuHandle(descriptorIndex);
  boundSamplerCount_ = std::max(boundSamplerCount_, static_cast<size_t>(index + 1));

  IGL_LOG_INFO("ComputeCommandEncoder::bindSamplerState: Created sampler at index %u, descriptor slot %u\n",
               index, descriptorIndex);
}

void ComputeCommandEncoder::pushDebugGroupLabel(const char* label, const Color& /*color*/) const {
  auto* commandList = commandBuffer_.getCommandList();
  if (commandList) {
    // PIX debug markers
    const size_t len = strlen(label);
    std::wstring wlabel(len, L' ');
    std::mbstowcs(&wlabel[0], label, len);
    commandList->BeginEvent(0, wlabel.c_str(), static_cast<UINT>((wlabel.length() + 1) * sizeof(wchar_t)));
  }
}

void ComputeCommandEncoder::insertDebugEventLabel(const char* label, const Color& /*color*/) const {
  auto* commandList = commandBuffer_.getCommandList();
  if (commandList) {
    const size_t len = strlen(label);
    std::wstring wlabel(len, L' ');
    std::mbstowcs(&wlabel[0], label, len);
    commandList->SetMarker(0, wlabel.c_str(), static_cast<UINT>((wlabel.length() + 1) * sizeof(wchar_t)));
  }
}

void ComputeCommandEncoder::popDebugGroupLabel() const {
  auto* commandList = commandBuffer_.getCommandList();
  if (commandList) {
    commandList->EndEvent();
  }
}

} // namespace igl::d3d12
