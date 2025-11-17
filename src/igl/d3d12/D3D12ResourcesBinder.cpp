/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/D3D12ResourcesBinder.h>

#include <igl/d3d12/Buffer.h>
#include <igl/d3d12/CommandBuffer.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12Context.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/SamplerState.h>
#include <igl/d3d12/Texture.h>

namespace igl::d3d12 {

namespace {
// Root signature layout constants
// Graphics pipeline root signature (from Device::createRenderPipeline):
// - Parameter 0: Root Constants b2 (push constants)
// - Parameter 1: Root CBV b0 (legacy)
// - Parameter 2: Root CBV b1 (legacy)
// - Parameter 3: CBV descriptor table (b2-b15)
// - Parameter 4: SRV descriptor table (textures t0-t15)
// - Parameter 5: Sampler descriptor table (s0-s15)
// - Parameter 6: UAV descriptor table (storage buffers u0-uN)
constexpr uint32_t kGraphicsRootParam_PushConstants = 0;
constexpr uint32_t kGraphicsRootParam_RootCBV0 = 1;
constexpr uint32_t kGraphicsRootParam_RootCBV1 = 2;
constexpr uint32_t kGraphicsRootParam_CBVTable = 3;
constexpr uint32_t kGraphicsRootParam_SRVTable = 4;
constexpr uint32_t kGraphicsRootParam_SamplerTable = 5;
constexpr uint32_t kGraphicsRootParam_UAVTable = 6;

// Compute pipeline root signature (from Device::createComputePipeline):
// - Parameter 0: Root Constants b0 (push constants)
// - Parameter 1: UAV table (u0-uN storage buffers)
// - Parameter 2: SRV table (t0-tN textures)
// - Parameter 3: CBV table (b1-bN uniform buffers)
// - Parameter 4: Sampler table (s0-sN)
constexpr uint32_t kComputeRootParam_PushConstants = 0;
constexpr uint32_t kComputeRootParam_UAVTable = 1;
constexpr uint32_t kComputeRootParam_SRVTable = 2;
constexpr uint32_t kComputeRootParam_CBVTable = 3;
constexpr uint32_t kComputeRootParam_SamplerTable = 4;

// D3D12 alignment requirement for constant buffer views
constexpr size_t kConstantBufferAlignment = 256;
constexpr size_t kMaxCBVSize = 65536; // 64 KB (D3D12 spec limit)

} // namespace

D3D12ResourcesBinder::D3D12ResourcesBinder(CommandBuffer& commandBuffer, bool isCompute)
    : commandBuffer_(commandBuffer), isCompute_(isCompute) {}

void D3D12ResourcesBinder::bindTexture(uint32_t index, ITexture* texture) {
  if (index >= IGL_TEXTURE_SAMPLERS_MAX) {
    IGL_LOG_ERROR("D3D12ResourcesBinder::bindTexture: index %u exceeds maximum %u\n",
                  index,
                  IGL_TEXTURE_SAMPLERS_MAX);
    return;
  }

  if (!texture) {
    // Unbind texture at this slot
    if (index < bindingsTextures_.count) {
      bindingsTextures_.textures[index] = nullptr;
      bindingsTextures_.handles[index] = {};
      // Update count to highest bound slot + 1
      while (bindingsTextures_.count > 0 && bindingsTextures_.textures[bindingsTextures_.count - 1] == nullptr) {
        bindingsTextures_.count--;
      }
    }
    dirtyFlags_ |= DirtyFlagBits_Textures;
    return;
  }

  auto* d3dTexture = static_cast<Texture*>(texture);
  ID3D12Resource* resource = d3dTexture->getResource();

  if (!resource) {
    IGL_LOG_ERROR("D3D12ResourcesBinder::bindTexture: texture resource is null\n");
    return;
  }

  // Transition texture to shader resource state
  // Note: This must happen immediately, not deferred until updateBindings()
  // Use pipeline-specific states for optimal barrier tracking:
  // - Graphics: PIXEL_SHADER_RESOURCE (pixel shader read)
  // - Compute: NON_PIXEL_SHADER_RESOURCE (compute/vertex/geometry shader read)
  auto* commandList = commandBuffer_.getCommandList();
  const auto targetState = isCompute_ ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
                                      : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  d3dTexture->transitionAll(commandList, targetState);

  // Store texture pointer for descriptor creation in updateBindings()
  bindingsTextures_.textures[index] = texture;

  // Mark textures dirty - descriptor will be created in updateBindings()
  dirtyFlags_ |= DirtyFlagBits_Textures;

  // Update binding count
  if (index >= bindingsTextures_.count) {
    bindingsTextures_.count = index + 1;
  }
}

void D3D12ResourcesBinder::bindSamplerState(uint32_t index, ISamplerState* samplerState) {
  if (index >= IGL_TEXTURE_SAMPLERS_MAX) {
    IGL_LOG_ERROR("D3D12ResourcesBinder::bindSamplerState: index %u exceeds maximum %u\n",
                  index,
                  IGL_TEXTURE_SAMPLERS_MAX);
    return;
  }

  if (!samplerState) {
    // Unbind sampler at this slot
    if (index < bindingsSamplers_.count) {
      bindingsSamplers_.samplers[index] = nullptr;
      bindingsSamplers_.handles[index] = {};
      // Update count to highest bound slot + 1
      while (bindingsSamplers_.count > 0 && bindingsSamplers_.samplers[bindingsSamplers_.count - 1] == nullptr) {
        bindingsSamplers_.count--;
      }
    }
    dirtyFlags_ |= DirtyFlagBits_Samplers;
    return;
  }

  // Store sampler pointer for descriptor creation in updateBindings()
  bindingsSamplers_.samplers[index] = samplerState;

  // Mark samplers dirty - descriptor will be created in updateBindings()
  dirtyFlags_ |= DirtyFlagBits_Samplers;

  // Update binding count
  if (index >= bindingsSamplers_.count) {
    bindingsSamplers_.count = index + 1;
  }
}

void D3D12ResourcesBinder::bindBuffer(uint32_t index,
                                      IBuffer* buffer,
                                      size_t offset,
                                      size_t size,
                                      bool isUAV,
                                      size_t elementStride) {
  if (index >= IGL_BUFFER_BINDINGS_MAX) {
    IGL_LOG_ERROR("D3D12ResourcesBinder::bindBuffer: index %u exceeds maximum %u\n",
                  index,
                  IGL_BUFFER_BINDINGS_MAX);
    return;
  }

  if (!buffer) {
    // Unbind buffer/UAV at this slot
    if (isUAV) {
      if (index < bindingsUAVs_.count) {
        bindingsUAVs_.buffers[index] = nullptr;
        bindingsUAVs_.offsets[index] = 0;
        bindingsUAVs_.elementStrides[index] = 0;
        bindingsUAVs_.handles[index] = {};
        while (bindingsUAVs_.count > 0 && bindingsUAVs_.buffers[bindingsUAVs_.count - 1] == nullptr) {
          bindingsUAVs_.count--;
        }
      }
      dirtyFlags_ |= DirtyFlagBits_UAVs;
    } else {
      if (index < bindingsBuffers_.count) {
        bindingsBuffers_.buffers[index] = nullptr;
        bindingsBuffers_.addresses[index] = 0;
        bindingsBuffers_.offsets[index] = 0;
        bindingsBuffers_.sizes[index] = 0;
        while (bindingsBuffers_.count > 0 && bindingsBuffers_.buffers[bindingsBuffers_.count - 1] == nullptr) {
          bindingsBuffers_.count--;
        }
      }
      dirtyFlags_ |= DirtyFlagBits_Buffers;
    }
    return;
  }

  auto* d3dBuffer = static_cast<Buffer*>(buffer);
  ID3D12Resource* resource = d3dBuffer->getResource();

  if (!resource) {
    IGL_LOG_ERROR("D3D12ResourcesBinder::bindBuffer: buffer resource is null\n");
    return;
  }

  D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = resource->GetGPUVirtualAddress() + offset;

  if (isUAV) {
    // Storage buffer (UAV) - store buffer pointer, offset, and element stride for descriptor creation
    if (elementStride == 0) {
      IGL_LOG_ERROR("D3D12ResourcesBinder::bindBuffer: UAV binding requires non-zero elementStride\n");
      return;
    }
    bindingsUAVs_.buffers[index] = buffer;
    bindingsUAVs_.offsets[index] = offset;
    bindingsUAVs_.elementStrides[index] = elementStride;
    dirtyFlags_ |= DirtyFlagBits_UAVs;
    if (index >= bindingsUAVs_.count) {
      bindingsUAVs_.count = index + 1;
    }
  } else {
    // Uniform buffer (CBV) - store buffer pointer and GPU address
    bindingsBuffers_.buffers[index] = buffer;
    bindingsBuffers_.addresses[index] = gpuAddress;
    bindingsBuffers_.offsets[index] = offset;
    bindingsBuffers_.sizes[index] = size;
    dirtyFlags_ |= DirtyFlagBits_Buffers;
    if (index >= bindingsBuffers_.count) {
      bindingsBuffers_.count = index + 1;
    }
  }
}

bool D3D12ResourcesBinder::updateBindings(Result* outResult) {
  auto* commandList = commandBuffer_.getCommandList();
  auto& context = commandBuffer_.getContext();
  auto* device = context.getDevice();

  if (!commandList || !device) {
    if (outResult) {
      *outResult = Result{Result::Code::RuntimeError, "Invalid command list or device"};
    }
    return false;
  }

  bool success = true;

  // Update textures (SRV table)
  if (dirtyFlags_ & DirtyFlagBits_Textures) {
    if (!updateTextureBindings(commandList, device, outResult)) {
      success = false;
    }
  }

  // Update samplers (sampler table)
  if (dirtyFlags_ & DirtyFlagBits_Samplers) {
    if (!updateSamplerBindings(commandList, device, outResult)) {
      success = false;
    }
  }

  // Update buffers (root CBVs or CBV table)
  if (dirtyFlags_ & DirtyFlagBits_Buffers) {
    if (!updateBufferBindings(commandList, device, outResult)) {
      success = false;
    }
  }

  // Update UAVs (UAV table for compute)
  if ((dirtyFlags_ & DirtyFlagBits_UAVs) && isCompute_) {
    if (!updateUAVBindings(commandList, device, outResult)) {
      success = false;
    }
  }

  // Clear dirty flags
  dirtyFlags_ = 0;

  return success;
}

void D3D12ResourcesBinder::reset() {
  bindingsTextures_ = {};
  bindingsSamplers_ = {};
  bindingsBuffers_ = {};
  bindingsUAVs_ = {};
  dirtyFlags_ = DirtyFlagBits_Textures | DirtyFlagBits_Samplers | DirtyFlagBits_Buffers |
                DirtyFlagBits_UAVs;
}

bool D3D12ResourcesBinder::updateTextureBindings(ID3D12GraphicsCommandList* cmdList,
                                                 ID3D12Device* device,
                                                 Result* outResult) {
  if (bindingsTextures_.count == 0) {
    return true; // Nothing to bind
  }

  // Validate that bindings are dense starting from slot 0
  if (bindingsTextures_.textures[0] == nullptr) {
    IGL_LOG_ERROR("D3D12ResourcesBinder: Texture bindings are sparse (slot 0 not bound). "
                  "D3D12 requires dense bindings starting at index 0.\n");
    if (outResult) {
      *outResult = Result{Result::Code::InvalidOperation,
                          "Texture bindings must be dense starting at slot 0"};
    }
    return false;
  }

  auto& context = commandBuffer_.getContext();

  // Allocate a contiguous range of descriptors for all textures on a single page
  // This ensures we can bind them as a single descriptor table
  uint32_t baseDescriptorIndex = 0;
  Result allocResult = commandBuffer_.allocateCbvSrvUavRange(bindingsTextures_.count, &baseDescriptorIndex);
  if (!allocResult.isOk()) {
    IGL_LOG_ERROR("D3D12ResourcesBinder: Failed to allocate contiguous SRV range (%u descriptors): %s\n",
                  bindingsTextures_.count,
                  allocResult.message.c_str());
    if (outResult) {
      *outResult = allocResult;
    }
    return false;
  }

  // Verify all textures are bound (dense binding requirement)
  for (uint32_t i = 0; i < bindingsTextures_.count; ++i) {
    if (bindingsTextures_.textures[i] == nullptr) {
      IGL_LOG_ERROR("D3D12ResourcesBinder: Sparse texture binding detected at slot %u\n", i);
      if (outResult) {
        *outResult = Result{Result::Code::InvalidOperation, "Texture bindings must be dense"};
      }
      return false;
    }
  }

  // Create SRV descriptors for all bound textures
  for (uint32_t i = 0; i < bindingsTextures_.count; ++i) {
    auto* texture = bindingsTextures_.textures[i];
    auto* d3dTexture = static_cast<Texture*>(texture);
    ID3D12Resource* resource = d3dTexture->getResource();

    // Use contiguous descriptor index (baseDescriptorIndex + i)
    const uint32_t descriptorIndex = baseDescriptorIndex + i;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(descriptorIndex);

    // Create SRV descriptor
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureFormatToDXGIShaderResourceViewFormat(d3dTexture->getFormat());
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    // Set view dimension based on texture type
    auto resourceDesc = resource->GetDesc();
    const bool isView = d3dTexture->isView();
    const uint32_t mostDetailedMip = isView ? d3dTexture->getMipLevelOffset() : 0;
    const uint32_t mipLevels =
        isView ? d3dTexture->getNumMipLevelsInView() : d3dTexture->getNumMipLevels();

    if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
      srvDesc.Texture3D.MipLevels = mipLevels;
      srvDesc.Texture3D.MostDetailedMip = mostDetailedMip;
    } else if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
      const auto textureType = d3dTexture->getType();
      const bool isArrayTexture =
          (isView && d3dTexture->getNumArraySlicesInView() > 0) ||
          (!isView && resourceDesc.DepthOrArraySize > 1);

      if (textureType == TextureType::TwoDArray) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MostDetailedMip = mostDetailedMip;
        srvDesc.Texture2DArray.MipLevels = mipLevels;
        srvDesc.Texture2DArray.FirstArraySlice = isView ? d3dTexture->getArraySliceOffset() : 0;
        srvDesc.Texture2DArray.ArraySize =
            isView ? d3dTexture->getNumArraySlicesInView() : resourceDesc.DepthOrArraySize;
      } else if (textureType == TextureType::Cube) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = mostDetailedMip;
        srvDesc.TextureCube.MipLevels = mipLevels;
      } else {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = mostDetailedMip;
        srvDesc.Texture2D.MipLevels = mipLevels;
      }
    } else {
      IGL_LOG_ERROR("D3D12ResourcesBinder: Unsupported texture dimension %d\n",
                    resourceDesc.Dimension);
      if (outResult) {
        *outResult = Result{Result::Code::Unsupported, "Unsupported texture dimension for SRV"};
      }
      return false;
    }

    device->CreateShaderResourceView(resource, &srvDesc, cpuHandle);
    D3D12Context::trackResourceCreation("SRV", 0);

    // Cache the GPU handle
    bindingsTextures_.handles[i] = gpuHandle;
  }

  // Bind the SRV table to the appropriate root parameter
  // Use the first descriptor's GPU handle as the base
  uint32_t rootParamIndex = isCompute_ ? kComputeRootParam_SRVTable : kGraphicsRootParam_SRVTable;

  if (isCompute_) {
    cmdList->SetComputeRootDescriptorTable(rootParamIndex, bindingsTextures_.handles[0]);
  } else {
    cmdList->SetGraphicsRootDescriptorTable(rootParamIndex, bindingsTextures_.handles[0]);
  }

  return true;
}

bool D3D12ResourcesBinder::updateSamplerBindings(ID3D12GraphicsCommandList* cmdList,
                                                 ID3D12Device* device,
                                                 Result* outResult) {
  if (bindingsSamplers_.count == 0) {
    return true; // Nothing to bind
  }

  // Validate dense bindings starting from slot 0
  if (bindingsSamplers_.samplers[0] == nullptr) {
    IGL_LOG_ERROR("D3D12ResourcesBinder: Sampler bindings are sparse (slot 0 not bound). "
                  "D3D12 requires dense bindings starting at index 0.\n");
    if (outResult) {
      *outResult = Result{Result::Code::InvalidOperation,
                          "Sampler bindings must be dense starting at slot 0"};
    }
    return false;
  }

  auto& context = commandBuffer_.getContext();

  // Allocate sampler descriptors for all samplers
  for (uint32_t i = 0; i < bindingsSamplers_.count; ++i) {
    if (bindingsSamplers_.samplers[i] == nullptr) {
      IGL_LOG_ERROR("D3D12ResourcesBinder: Sparse sampler binding detected at slot %u\n", i);
      if (outResult) {
        *outResult = Result{Result::Code::InvalidOperation, "Sampler bindings must be dense"};
      }
      return false;
    }

    auto* samplerState = bindingsSamplers_.samplers[i];
    uint32_t& descriptorIndex = commandBuffer_.getNextSamplerDescriptor();

    // Get descriptor handles
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getSamplerCpuHandle(descriptorIndex);
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getSamplerGpuHandle(descriptorIndex);

    // Create sampler descriptor
    D3D12_SAMPLER_DESC samplerDesc = {};
    if (auto* d3dSampler = dynamic_cast<SamplerState*>(samplerState)) {
      samplerDesc = d3dSampler->getDesc();
    } else {
      // Fallback sampler desc
      samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
      samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
      samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
      samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
      samplerDesc.MipLODBias = 0.0f;
      samplerDesc.MaxAnisotropy = 1;
      samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
      samplerDesc.BorderColor[0] = 0.0f;
      samplerDesc.BorderColor[1] = 0.0f;
      samplerDesc.BorderColor[2] = 0.0f;
      samplerDesc.BorderColor[3] = 0.0f;
      samplerDesc.MinLOD = 0.0f;
      samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    }

    device->CreateSampler(&samplerDesc, cpuHandle);
    D3D12Context::trackResourceCreation("Sampler", 0);

    // Cache the GPU handle
    bindingsSamplers_.handles[i] = gpuHandle;

    // Increment sampler descriptor counter
    descriptorIndex++;
  }

  // Bind the sampler table to the appropriate root parameter
  uint32_t rootParamIndex =
      isCompute_ ? kComputeRootParam_SamplerTable : kGraphicsRootParam_SamplerTable;

  if (isCompute_) {
    cmdList->SetComputeRootDescriptorTable(rootParamIndex, bindingsSamplers_.handles[0]);
  } else {
    cmdList->SetGraphicsRootDescriptorTable(rootParamIndex, bindingsSamplers_.handles[0]);
  }

  return true;
}

bool D3D12ResourcesBinder::updateBufferBindings(ID3D12GraphicsCommandList* cmdList,
                                                ID3D12Device* device,
                                                Result* outResult) {
  if (bindingsBuffers_.count == 0) {
    return true; // Nothing to bind
  }

  if (isCompute_) {
    // Compute pipeline: all CBVs go through descriptor table (root parameter 3)
    auto& context = commandBuffer_.getContext();

    // Count bound CBVs and validate dense binding
    uint32_t boundCbvCount = 0;
    for (uint32_t i = 0; i < bindingsBuffers_.count; ++i) {
      if (bindingsBuffers_.addresses[i] != 0) {
        boundCbvCount++;
      }
    }

    if (boundCbvCount == 0) {
      return true; // No CBVs to bind
    }

    // CRITICAL VALIDATION: Enforce dense CBV binding for compute shaders
    // =====================================================================
    // D3D12 descriptor tables bind contiguously starting from the base register.
    // For compute CBVs, this means:
    //   - VALID:   binding slots 0, 1, 2 (dense from b0)
    //   - INVALID: binding slots 0, 2 (gap at slot 1)
    //   - INVALID: binding slots 1, 2 (slot 0 not bound)
    //
    // This is FATAL validation - sparse bindings will return InvalidOperation error.
    // Application code must ensure CBVs are bound densely from index 0 with no gaps.
    //
    // Rationale: When we call SetComputeRootDescriptorTable with N descriptors at base b0,
    // D3D12 expects HLSL registers b0, b1, ..., b(N-1) to map 1:1 with descriptor table
    // entries. Gaps would cause shader register mismatches and undefined behavior.

    if (bindingsBuffers_.addresses[0] == 0) {
      IGL_LOG_ERROR("D3D12ResourcesBinder: Compute CBV bindings are sparse (slot 0 not bound). "
                    "D3D12 requires dense bindings starting at index 0.\n");
      if (outResult) {
        *outResult = Result{Result::Code::InvalidOperation,
                            "Compute CBV bindings must be dense starting at slot 0"};
      }
      return false;
    }

    // Verify no gaps in binding range (all slots from 0 to boundCbvCount-1 must be bound)
    for (uint32_t i = 1; i < boundCbvCount; ++i) {
      if (bindingsBuffers_.addresses[i] == 0) {
        IGL_LOG_ERROR("D3D12ResourcesBinder: Sparse compute CBV binding detected at slot %u "
                      "(expected dense binding through slot %u)\n", i, boundCbvCount - 1);
        if (outResult) {
          *outResult = Result{Result::Code::InvalidOperation, "Compute CBV bindings must be dense"};
        }
        return false;
      }
    }

    // Allocate a contiguous range of descriptors for all CBVs on a single page
    // This ensures we can bind them as a single descriptor table
    uint32_t baseDescriptorIndex = 0;
    Result allocResult = commandBuffer_.allocateCbvSrvUavRange(boundCbvCount, &baseDescriptorIndex);
    if (!allocResult.isOk()) {
      IGL_LOG_ERROR("D3D12ResourcesBinder: Failed to allocate contiguous CBV range (%u descriptors): %s\n",
                    boundCbvCount,
                    allocResult.message.c_str());
      if (outResult) {
        *outResult = allocResult;
      }
      return false;
    }

    // Create CBV descriptors for all bound buffers
    uint32_t descriptorOffset = 0;
    for (uint32_t i = 0; i < bindingsBuffers_.count; ++i) {
      if (bindingsBuffers_.addresses[i] == 0) {
        continue; // Skip unbound slots
      }

      // Validate address alignment (D3D12 requires 256-byte alignment)
      if (bindingsBuffers_.addresses[i] % kConstantBufferAlignment != 0) {
        IGL_LOG_ERROR("D3D12ResourcesBinder: Constant buffer %u address 0x%llx is not 256-byte aligned\n",
                      i, bindingsBuffers_.addresses[i]);
        if (outResult) {
          *outResult = Result{Result::Code::ArgumentInvalid,
                              "Constant buffer address must be 256-byte aligned"};
        }
        return false;
      }

      // Validate size
      size_t size = bindingsBuffers_.sizes[i];
      if (size > kMaxCBVSize) {
        IGL_LOG_ERROR("D3D12ResourcesBinder: Constant buffer %u size (%zu bytes) exceeds 64 KB limit\n",
                      i, size);
        if (outResult) {
          *outResult = Result{Result::Code::ArgumentOutOfRange,
                              "Constant buffer size exceeds 64 KB D3D12 limit"};
        }
        return false;
      }

      // Align size to 256-byte boundary
      const size_t alignedSize = (size + kConstantBufferAlignment - 1) & ~(kConstantBufferAlignment - 1);

      // Use contiguous descriptor index (baseDescriptorIndex + descriptorOffset)
      const uint32_t descriptorIndex = baseDescriptorIndex + descriptorOffset;
      D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);

      D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
      cbvDesc.BufferLocation = bindingsBuffers_.addresses[i];
      cbvDesc.SizeInBytes = static_cast<UINT>(alignedSize);

      device->CreateConstantBufferView(&cbvDesc, cpuHandle);
      descriptorOffset++;
    }

    // Sanity check: descriptorOffset should match boundCbvCount after dense packing
    IGL_DEBUG_ASSERT(descriptorOffset == boundCbvCount,
                     "CBV descriptor packing mismatch: allocated %u but created %u",
                     boundCbvCount, descriptorOffset);

    // Bind the CBV descriptor table to root parameter 3
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(baseDescriptorIndex);
    cmdList->SetComputeRootDescriptorTable(kComputeRootParam_CBVTable, gpuHandle);
  } else {
    // Graphics pipeline: b0 and b1 are root CBVs, b2+ would use descriptor table
    // For now, we only support b0 and b1 as root CBVs (legacy behavior)

    // Bind b0 (root parameter 1)
    if (bindingsBuffers_.count > 0 && bindingsBuffers_.addresses[0] != 0) {
      cmdList->SetGraphicsRootConstantBufferView(kGraphicsRootParam_RootCBV0,
                                                  bindingsBuffers_.addresses[0]);
    }

    // Bind b1 (root parameter 2)
    if (bindingsBuffers_.count > 1 && bindingsBuffers_.addresses[1] != 0) {
      cmdList->SetGraphicsRootConstantBufferView(kGraphicsRootParam_RootCBV1,
                                                  bindingsBuffers_.addresses[1]);
    }

    // Note: b2+ would require descriptor table support, which is not implemented yet
    if (bindingsBuffers_.count > 2) {
      IGL_LOG_ERROR("D3D12ResourcesBinder: Graphics pipeline only supports b0-b1 currently. "
                    "b2+ bindings are not yet implemented.\n");
    }
  }

  return true;
}

bool D3D12ResourcesBinder::updateUAVBindings(ID3D12GraphicsCommandList* cmdList,
                                             ID3D12Device* device,
                                             Result* outResult) {
  if (bindingsUAVs_.count == 0) {
    return true; // Nothing to bind
  }

  // Validate dense bindings
  if (bindingsUAVs_.buffers[0] == nullptr) {
    IGL_LOG_ERROR("D3D12ResourcesBinder: UAV bindings are sparse (slot 0 not bound). "
                  "D3D12 requires dense bindings starting at index 0.\n");
    if (outResult) {
      *outResult = Result{Result::Code::InvalidOperation,
                          "UAV bindings must be dense starting at slot 0"};
    }
    return false;
  }

  auto& context = commandBuffer_.getContext();

  // Verify all UAVs are bound (dense binding requirement)
  for (uint32_t i = 0; i < bindingsUAVs_.count; ++i) {
    if (bindingsUAVs_.buffers[i] == nullptr) {
      IGL_LOG_ERROR("D3D12ResourcesBinder: Sparse UAV binding detected at slot %u\n", i);
      if (outResult) {
        *outResult = Result{Result::Code::InvalidOperation, "UAV bindings must be dense"};
      }
      return false;
    }
  }

  // Allocate a contiguous range of descriptors for all UAVs on a single page
  // This ensures we can bind them as a single descriptor table
  uint32_t baseDescriptorIndex = 0;
  Result allocResult = commandBuffer_.allocateCbvSrvUavRange(bindingsUAVs_.count, &baseDescriptorIndex);
  if (!allocResult.isOk()) {
    IGL_LOG_ERROR("D3D12ResourcesBinder: Failed to allocate contiguous UAV range (%u descriptors): %s\n",
                  bindingsUAVs_.count,
                  allocResult.message.c_str());
    if (outResult) {
      *outResult = allocResult;
    }
    return false;
  }

  // Create UAV descriptors for all bound storage buffers
  for (uint32_t i = 0; i < bindingsUAVs_.count; ++i) {
    auto* buffer = bindingsUAVs_.buffers[i];
    auto* d3dBuffer = static_cast<Buffer*>(buffer);
    ID3D12Resource* resource = d3dBuffer->getResource();

    const size_t offset = bindingsUAVs_.offsets[i];
    const size_t elementStride = bindingsUAVs_.elementStrides[i];
    const size_t bufferSize = d3dBuffer->getSizeInBytes();

    // FATAL VALIDATION: UAV offset must be aligned to element stride
    // This check immediately fails the entire updateBindings() call and returns InvalidOperation.
    // Misaligned offsets would create invalid D3D12 UAV descriptors and cause device removal.
    if (offset % elementStride != 0) {
      IGL_LOG_ERROR(
          "D3D12ResourcesBinder: UAV offset %zu is not aligned to element stride %zu. "
          "This is a FATAL error - updateBindings() will fail.\n",
          offset,
          elementStride);
      if (outResult) {
        *outResult = Result{Result::Code::ArgumentInvalid,
                            "UAV offset must be aligned to element stride"};
      }
      return false;
    }

    // FATAL VALIDATION: UAV offset must be within buffer bounds
    // This check immediately fails the entire updateBindings() call and returns ArgumentOutOfRange.
    // Out-of-bounds offsets would access invalid memory and cause GPU faults.
    if (offset > bufferSize) {
      IGL_LOG_ERROR("D3D12ResourcesBinder: UAV offset %zu exceeds buffer size %zu. "
                    "This is a FATAL error - updateBindings() will fail.\n",
                    offset,
                    bufferSize);
      if (outResult) {
        *outResult = Result{Result::Code::ArgumentOutOfRange, "UAV offset exceeds buffer size"};
      }
      return false;
    }

    const size_t remaining = bufferSize - offset;
    // FATAL VALIDATION: At least one full element must fit in remaining buffer space
    // This check immediately fails the entire updateBindings() call and returns ArgumentOutOfRange.
    // Creating a UAV with zero elements or partial elements would be invalid.
    if (remaining < elementStride) {
      IGL_LOG_ERROR("D3D12ResourcesBinder: UAV remaining size %zu < element stride %zu. "
                    "This is a FATAL error - updateBindings() will fail.\n",
                    remaining,
                    elementStride);
      if (outResult) {
        *outResult = Result{Result::Code::ArgumentOutOfRange,
                            "UAV remaining size less than element stride"};
      }
      return false;
    }

    // Use contiguous descriptor index (baseDescriptorIndex + i)
    const uint32_t descriptorIndex = baseDescriptorIndex + i;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(descriptorIndex);

    // Create UAV descriptor for structured buffer
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;  // Required for structured buffers
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = static_cast<UINT64>(offset / elementStride);
    uavDesc.Buffer.NumElements = static_cast<UINT>(remaining / elementStride);
    uavDesc.Buffer.StructureByteStride = static_cast<UINT>(elementStride);
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

    device->CreateUnorderedAccessView(resource, nullptr, &uavDesc, cpuHandle);
    D3D12Context::trackResourceCreation("UAV", 0);

    // Cache the GPU handle
    bindingsUAVs_.handles[i] = gpuHandle;
  }

  // Bind the UAV table to root parameter 1 (compute only)
  cmdList->SetComputeRootDescriptorTable(kComputeRootParam_UAVTable, bindingsUAVs_.handles[0]);

  return true;
}

} // namespace igl::d3d12
