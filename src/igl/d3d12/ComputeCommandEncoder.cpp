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
  // DX12-NEW-01: Use active heap from frame context, not legacy accessor
  auto& context = commandBuffer_.getContext();
  auto& frameCtx = context.getFrameContexts()[context.getCurrentFrameIndex()];

  ID3D12DescriptorHeap* cbvSrvUavHeap = frameCtx.activeCbvSrvUavHeap.Get();
  ID3D12DescriptorHeap* samplerHeap = frameCtx.samplerHeap.Get();

  // Legacy fallback: if the context does not provide per-frame heaps, try the manager once
  if ((!cbvSrvUavHeap || !samplerHeap) && context.getDescriptorHeapManager()) {
    auto* heapMgr = context.getDescriptorHeapManager();
    if (!cbvSrvUavHeap) {
      cbvSrvUavHeap = heapMgr->getCbvSrvUavHeap();
    }
    if (!samplerHeap) {
      samplerHeap = heapMgr->getSamplerHeap();
    }
  }

  if (cbvSrvUavHeap && samplerHeap) {
    auto* commandList = commandBuffer_.getCommandList();
    if (commandList) {
      ID3D12DescriptorHeap* heaps[] = {cbvSrvUavHeap, samplerHeap};
      commandList->SetDescriptorHeaps(2, heaps);
      IGL_LOG_INFO("ComputeCommandEncoder: Descriptor heaps set (active heap from FrameContext)\n");
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
  // P1_DX12-FIND-04: Create CBV descriptors and bind via descriptor table
  if (boundCbvCount_ > 0) {
    auto& context = commandBuffer_.getContext();
    auto* device = context.getDevice();

    // Get starting descriptor index for our CBV table
    // C-001: Allocate descriptors individually (may span multiple pages)
    std::vector<uint32_t> cbvIndices;
    for (size_t i = 0; i < boundCbvCount_; ++i) {
      uint32_t descriptorIndex = 0;
      Result allocResult = commandBuffer_.getNextCbvSrvUavDescriptor(&descriptorIndex);
      if (!allocResult.isOk()) {
        IGL_LOG_ERROR("ComputeCommandEncoder: Failed to allocate CBV descriptor %zu: %s\n", i, allocResult.message.c_str());
        return;
      }
      cbvIndices.push_back(descriptorIndex);
    }

    // Create CBV descriptors for all bound constant buffers
    for (size_t i = 0; i < boundCbvCount_; ++i) {
      if (cachedCbvAddresses_[i] != 0 && cachedCbvSizes_[i] > 0) {
        const uint32_t descriptorIndex = cbvIndices[i];
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);

        // Align size to 256-byte boundary (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
        const size_t alignedSize = (cachedCbvSizes_[i] + 255) & ~255;

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = cachedCbvAddresses_[i];
        cbvDesc.SizeInBytes = static_cast<UINT>(alignedSize);

        device->CreateConstantBufferView(&cbvDesc, cpuHandle);
      }
    }

    // Bind the CBV descriptor table to root parameter 3
    // C-001: Use first allocated descriptor index
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(cbvIndices[0]);
    commandList->SetComputeRootDescriptorTable(3, gpuHandle);

    IGL_LOG_INFO("ComputeCommandEncoder: Bound %zu CBVs via descriptor table (descriptors %u-%u)\n",
                 boundCbvCount_, cbvIndices[0], cbvIndices[boundCbvCount_ - 1]);
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

void ComputeCommandEncoder::bindPushConstants(const void* data,
                                              size_t length,
                                              size_t offset) {
  auto* commandList = commandBuffer_.getCommandList();
  if (!commandList || !data || length == 0) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindPushConstants: Invalid parameters (list=%p, data=%p, len=%zu)\n",
                  commandList, data, length);
    return;
  }

  // Compute root signature parameter 0 is declared as D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS (b0)
  // with 16 DWORDs (64 bytes) capacity
  constexpr size_t kMaxPushConstantBytes = 64;

  if (length + offset > kMaxPushConstantBytes) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindPushConstants: size %zu + offset %zu exceeds maximum %zu bytes\n",
                  length, offset, kMaxPushConstantBytes);
    return;
  }

  // Calculate number of 32-bit values and offset in DWORDs
  const uint32_t num32BitValues = static_cast<uint32_t>((length + 3) / 4);  // Round up to DWORDs
  const uint32_t destOffsetIn32BitValues = static_cast<uint32_t>(offset / 4);

  // Use SetComputeRoot32BitConstants to directly write data to root constants
  // Root parameter 0 = b0 (Push Constants), as declared in compute root signature
  commandList->SetComputeRoot32BitConstants(
      0,                          // Root parameter index (push constants at parameter 0)
      num32BitValues,             // Number of 32-bit values to set
      data,                       // Source data
      destOffsetIn32BitValues);   // Destination offset in 32-bit values

  IGL_LOG_INFO("ComputeCommandEncoder::bindPushConstants: Set %u DWORDs (%zu bytes) at offset %zu to root parameter 0 (b0)\n",
               num32BitValues, length, offset);
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
  auto* d3dTexture = static_cast<Texture*>(texture);

  if (!device || !d3dTexture->getResource() || context.getCbvSrvUavHeap() == nullptr) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindTexture: missing device, resource, or per-frame heap\n");
    return;
  }

  // Transition texture to shader resource state for compute shader access
  auto* commandList = commandBuffer_.getCommandList();
  if (commandList) {
    d3dTexture->transitionAll(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  }

  // Allocate descriptor and create SRV
  // C-001: Now uses Result-based allocation with dynamic heap growth
  uint32_t descriptorIndex = 0;
  Result allocResult = commandBuffer_.getNextCbvSrvUavDescriptor(&descriptorIndex);
  if (!allocResult.isOk()) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindTexture: Failed to allocate descriptor: %s\n", allocResult.message.c_str());
    return;
  }
  D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);
  D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(descriptorIndex);

  // Create SRV descriptor
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format =
      textureFormatToDXGIShaderResourceViewFormat(d3dTexture->getFormat());
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

  // Pre-creation validation (TASK_P0_DX12-004)
  IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateShaderResourceView");
  IGL_DEBUG_ASSERT(d3dTexture->getResource() != nullptr, "Texture resource is null");
  IGL_DEBUG_ASSERT(cpuHandle.ptr != 0, "SRV descriptor handle is invalid");

  device->CreateShaderResourceView(d3dTexture->getResource(), &srvDesc, cpuHandle);

  cachedSrvHandles_[index] = gpuHandle;
  for (size_t i = index + 1; i < kMaxComputeTextures; ++i) {
    cachedSrvHandles_[i] = {};
  }
  boundSrvCount_ = static_cast<size_t>(index + 1);

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

  if (!device || context.getCbvSrvUavHeap() == nullptr) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: missing device or per-frame descriptor heap\n");
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

    // C-001: Now uses Result-based allocation with dynamic heap growth
    uint32_t descriptorIndex = 0;
    Result allocResult = commandBuffer_.getNextCbvSrvUavDescriptor(&descriptorIndex);
    if (!allocResult.isOk()) {
      IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: Failed to allocate UAV descriptor: %s\n", allocResult.message.c_str());
      return;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(descriptorIndex);

    // Create UAV descriptor for RWStructuredBuffer (structured buffer)
    // D3D12 compute shaders expect structured buffers, not raw buffers
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN; // Required for structured buffers
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = offset / 4; // Element offset (4 bytes per element)
    uavDesc.Buffer.NumElements = static_cast<UINT>((d3dBuffer->getSizeInBytes() - offset) / 4);
    uavDesc.Buffer.StructureByteStride = 4; // 4 bytes per element (float)
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE; // No flags for structured buffers

    // Pre-creation validation (TASK_P0_DX12-004)
    IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateUnorderedAccessView");
    IGL_DEBUG_ASSERT(d3dBuffer->getResource() != nullptr, "Buffer resource is null");
    IGL_DEBUG_ASSERT(cpuHandle.ptr != 0, "UAV descriptor handle is invalid");

    device->CreateUnorderedAccessView(d3dBuffer->getResource(), nullptr, &uavDesc, cpuHandle);

    cachedUavHandles_[index] = gpuHandle;
    for (size_t i = index + 1; i < kMaxComputeBuffers; ++i) {
      cachedUavHandles_[i] = {};
    }
    boundUavCount_ = static_cast<size_t>(index + 1);

    IGL_LOG_INFO("ComputeCommandEncoder::bindBuffer: Created UAV at index %u, descriptor slot %u\n",
                 index, descriptorIndex);

    commandBuffer_.trackTransientResource(d3dBuffer->getResource());
  } else if (isUniformBuffer) {
    // Uniform buffer - bind as CBV (constant buffer view)
    if (index >= kMaxComputeBuffers) {
      IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: CBV index %u exceeds max %zu\n",
                    index, kMaxComputeBuffers);
      return;
    }

    // D3D12 requires constant buffer addresses to be 256-byte aligned
    // (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
    if ((offset & 255) != 0) {
      IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: ERROR - CBV offset %zu is not 256-byte aligned "
                    "(required by D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT). "
                    "Constant buffers must be created at aligned offsets. Ignoring bind request.\n", offset);
      return;
    }

    cachedCbvAddresses_[index] = d3dBuffer->gpuAddress(offset);
    // P1_DX12-FIND-04: Store buffer size for CBV descriptor creation in dispatchThreadGroups()
    cachedCbvSizes_[index] = d3dBuffer->getSizeInBytes() - offset;
    for (size_t i = index + 1; i < kMaxComputeBuffers; ++i) {
      cachedCbvAddresses_[i] = 0;
      cachedCbvSizes_[i] = 0;
    }
    boundCbvCount_ = static_cast<size_t>(index + 1);

    IGL_LOG_INFO("ComputeCommandEncoder::bindBuffer: Cached CBV at index %u, address 0x%llx, size %zu\n",
                 index, cachedCbvAddresses_[index], cachedCbvSizes_[index]);

    commandBuffer_.trackTransientResource(d3dBuffer->getResource());
  } else {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: Buffer must be Uniform or Storage type\n");
  }
}

void ComputeCommandEncoder::bindUniform(const UniformDesc& /*uniformDesc*/, const void* /*data*/) {
  // Single uniform binding not supported in D3D12
  // Use uniform buffers (CBVs) instead
  IGL_LOG_INFO("ComputeCommandEncoder::bindUniform - not supported, use uniform buffers\n");
}

void ComputeCommandEncoder::bindBytes(uint32_t index, const void* data, size_t length) {
  auto* commandList = commandBuffer_.getCommandList();
  if (!commandList || !data || length == 0) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindBytes: Invalid parameters (list=%p, data=%p, len=%zu)\n",
                  commandList, data, length);
    return;
  }

  // bindBytes binds small constant data as a constant buffer
  // We use a dynamic CBV approach: allocate temp buffer from upload heap and cache its GPU address

  // Validate index range - we support binding to b1-b7 (b0 is reserved for push constants)
  if (index >= kMaxComputeBuffers) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindBytes: index %u exceeds supported range [0,%zu)\n",
                  index, kMaxComputeBuffers);
    return;
  }

  // D3D12 requires constant buffer addresses to be 256-byte aligned
  constexpr size_t kCBVAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT; // 256 bytes
  const size_t alignedSize = (length + kCBVAlignment - 1) & ~(kCBVAlignment - 1);

  // Create transient upload buffer for this data
  auto& context = commandBuffer_.getContext();
  auto* device = context.getDevice();
  if (!device) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindBytes: D3D12 device is null\n");
    return;
  }

  // Create upload heap buffer
  D3D12_HEAP_PROPERTIES heapProps = {};
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

  D3D12_RESOURCE_DESC bufferDesc = {};
  bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufferDesc.Alignment = 0;
  bufferDesc.Width = alignedSize;
  bufferDesc.Height = 1;
  bufferDesc.DepthOrArraySize = 1;
  bufferDesc.MipLevels = 1;
  bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
  bufferDesc.SampleDesc.Count = 1;
  bufferDesc.SampleDesc.Quality = 0;
  bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
  HRESULT hr = device->CreateCommittedResource(
      &heapProps,
      D3D12_HEAP_FLAG_NONE,
      &bufferDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(uploadBuffer.GetAddressOf())
  );

  if (FAILED(hr)) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindBytes: Failed to create upload buffer: HRESULT = 0x%08X\n",
                  static_cast<unsigned>(hr));
    return;
  }

  // Map and copy data
  void* mappedPtr = nullptr;
  hr = uploadBuffer->Map(0, nullptr, &mappedPtr);
  if (FAILED(hr) || !mappedPtr) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindBytes: Failed to map upload buffer: HRESULT = 0x%08X\n",
                  static_cast<unsigned>(hr));
    return;
  }

  memcpy(mappedPtr, data, length);
  uploadBuffer->Unmap(0, nullptr);

  // Get GPU virtual address and cache it (matching bindBuffer behavior for uniform buffers)
  D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = uploadBuffer->GetGPUVirtualAddress();
  cachedCbvAddresses_[index] = gpuAddress;
  for (size_t i = index + 1; i < kMaxComputeBuffers; ++i) {
    cachedCbvAddresses_[i] = 0;
  }
  boundCbvCount_ = static_cast<size_t>(index + 1);

  // Track transient resource to keep it alive until GPU finishes
  commandBuffer_.trackTransientResource(uploadBuffer.Get());

  IGL_LOG_INFO("ComputeCommandEncoder::bindBytes: Bound %zu bytes (aligned to %zu) to index %u (GPU address 0x%llx)\n",
               length, alignedSize, index, gpuAddress);
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
  auto* d3dTexture = static_cast<Texture*>(texture);

  if (!device || !d3dTexture->getResource() || context.getCbvSrvUavHeap() == nullptr) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindImageTexture: missing device, resource, or per-frame heap\n");
    return;
  }

  // Transition texture to UAV state for compute shader read/write access
  auto* commandList = commandBuffer_.getCommandList();
  if (commandList) {
    d3dTexture->transitionAll(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  }

  // Allocate descriptor and create UAV
  // C-001: Now uses Result-based allocation with dynamic heap growth
  uint32_t descriptorIndex = 0;
  Result allocResult = commandBuffer_.getNextCbvSrvUavDescriptor(&descriptorIndex);
  if (!allocResult.isOk()) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindImageTexture: Failed to allocate UAV descriptor: %s\n", allocResult.message.c_str());
    return;
  }
  D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);
  D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(descriptorIndex);

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

  // Pre-creation validation (TASK_P0_DX12-004)
  IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateUnorderedAccessView");
  IGL_DEBUG_ASSERT(d3dTexture->getResource() != nullptr, "Texture resource is null");
  IGL_DEBUG_ASSERT(cpuHandle.ptr != 0, "UAV descriptor handle is invalid");

  device->CreateUnorderedAccessView(d3dTexture->getResource(), nullptr, &uavDesc, cpuHandle);

  cachedUavHandles_[index] = gpuHandle;
  for (size_t i = index + 1; i < kMaxComputeBuffers; ++i) {
    cachedUavHandles_[i] = {};
  }
  boundUavCount_ = static_cast<size_t>(index + 1);

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
  auto* d3dSampler = static_cast<SamplerState*>(samplerState);

  if (!device || !d3dSampler || context.getSamplerHeap() == nullptr) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindSamplerState: missing device, sampler, or per-frame sampler heap\n");
    return;
  }

  // Allocate descriptor and create sampler
  const uint32_t descriptorIndex = commandBuffer_.getNextSamplerDescriptor()++;
  D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getSamplerCpuHandle(descriptorIndex);
  D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getSamplerGpuHandle(descriptorIndex);

  // Get sampler desc from IGL sampler state and create D3D12 sampler
  const D3D12_SAMPLER_DESC& samplerDesc = d3dSampler->getDesc();
  // Pre-creation validation (TASK_P0_DX12-004)
  IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateSampler");
  IGL_DEBUG_ASSERT(cpuHandle.ptr != 0, "Sampler descriptor handle is invalid");

  device->CreateSampler(&samplerDesc, cpuHandle);

  cachedSamplerHandles_[index] = gpuHandle;
  for (size_t i = index + 1; i < kMaxComputeSamplers; ++i) {
    cachedSamplerHandles_[i] = {};
  }
  boundSamplerCount_ = static_cast<size_t>(index + 1);

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
