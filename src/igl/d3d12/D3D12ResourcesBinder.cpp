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
#include <igl/d3d12/RenderPipelineState.h>
#include <igl/d3d12/SamplerState.h>
#include <igl/d3d12/Texture.h>

namespace igl::d3d12 {

namespace {
// D3D12 alignment requirement for constant buffer views
constexpr size_t kConstantBufferAlignment = 256;
constexpr size_t kMaxCBVSize = 65536; // 64 KB (D3D12 spec limit)

// Compute pipeline hardcoded root parameter layout
// Note: Graphics pipelines use pure reflection-based layout queried from RenderPipelineState
// Compute pipelines still use this hardcoded layout (should be migrated to reflection)
constexpr uint32_t kComputeRootParam_PushConstants = 0;
constexpr uint32_t kComputeRootParam_UAVTable = 1;
constexpr uint32_t kComputeRootParam_SRVTable = 2;
constexpr uint32_t kComputeRootParam_CBVTable = 3;
constexpr uint32_t kComputeRootParam_SamplerTable = 4;

} // namespace

D3D12ResourcesBinder::D3D12ResourcesBinder(CommandBuffer& commandBuffer, bool isCompute) :
  commandBuffer_(commandBuffer), isCompute_(isCompute) {}

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
      while (bindingsTextures_.count > 0 &&
             bindingsTextures_.textures[bindingsTextures_.count - 1] == nullptr) {
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
      while (bindingsSamplers_.count > 0 &&
             bindingsSamplers_.samplers[bindingsSamplers_.count - 1] == nullptr) {
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
        while (bindingsUAVs_.count > 0 &&
               bindingsUAVs_.buffers[bindingsUAVs_.count - 1] == nullptr) {
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
        while (bindingsBuffers_.count > 0 &&
               bindingsBuffers_.buffers[bindingsBuffers_.count - 1] == nullptr) {
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

  if (isUAV) {
    // Storage buffer (UAV) - store buffer pointer, offset, and element stride for descriptor
    // creation
    if (elementStride == 0) {
      IGL_LOG_ERROR(
          "D3D12ResourcesBinder::bindBuffer: UAV binding requires non-zero elementStride\n");
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
    // Uniform buffer (CBV) - D3D12 requires 256-byte alignment for CBV addresses
    // Compute base address (must be 256-byte aligned)
    D3D12_GPU_VIRTUAL_ADDRESS baseAddress = resource->GetGPUVirtualAddress();
    D3D12_GPU_VIRTUAL_ADDRESS alignedAddress =
        (baseAddress + offset) & ~(kConstantBufferAlignment - 1);

    bindingsBuffers_.buffers[index] = buffer;
    bindingsBuffers_.addresses[index] = alignedAddress;
    bindingsBuffers_.offsets[index] = offset;
    bindingsBuffers_.sizes[index] = size;
    dirtyFlags_ |= DirtyFlagBits_Buffers;
    if (index >= bindingsBuffers_.count) {
      bindingsBuffers_.count = index + 1;
    }
  }
}

bool D3D12ResourcesBinder::updateBindings(const RenderPipelineState* renderPipeline,
                                          Result* outResult) {
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
    if (!updateTextureBindings(commandList, device, renderPipeline, outResult)) {
      success = false;
    }
  }

  // Update samplers (sampler table)
  if (dirtyFlags_ & DirtyFlagBits_Samplers) {
    if (!updateSamplerBindings(commandList, device, renderPipeline, outResult)) {
      success = false;
    }
  }

  // Update buffers (CBV table)
  if (dirtyFlags_ & DirtyFlagBits_Buffers) {
    if (!updateBufferBindings(commandList, device, renderPipeline, outResult)) {
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
  dirtyFlags_ =
      DirtyFlagBits_Textures | DirtyFlagBits_Samplers | DirtyFlagBits_Buffers | DirtyFlagBits_UAVs;
}

bool D3D12ResourcesBinder::updateTextureBindings(ID3D12GraphicsCommandList* cmdList,
                                                 ID3D12Device* device,
                                                 const RenderPipelineState* renderPipeline,
                                                 Result* outResult) {
  if (bindingsTextures_.count == 0) {
    return true; // Nothing to bind
  }

  auto& context = commandBuffer_.getContext();

  // Determine how many descriptors to allocate based on pipeline's root signature
  // For graphics: Use pipeline's declared SRV range (0 to maxSRVSlot inclusive)
  // For compute: Use bindingsTextures_.count (legacy sparse allocation)
  uint32_t descriptorRangeSize = bindingsTextures_.count;

  if (!isCompute_ && renderPipeline) {
    // Graphics pipeline: Match root signature's SRV descriptor range exactly
    const UINT pipelineSRVCount = renderPipeline->getSRVDescriptorCount();
    if (pipelineSRVCount > 0) {
      descriptorRangeSize = pipelineSRVCount;
      IGL_D3D12_LOG_VERBOSE("updateTextureBindings: Using pipeline SRV range size=%u (bound=%u)\n",
                            descriptorRangeSize,
                            bindingsTextures_.count);
    }
  }

  // Allocate a contiguous range of descriptors for all textures on a single page
  // This ensures we can bind them as a single descriptor table
  uint32_t baseDescriptorIndex = 0;
  Result allocResult =
      commandBuffer_.allocateCbvSrvUavRange(descriptorRangeSize, &baseDescriptorIndex);
  if (!allocResult.isOk()) {
    IGL_LOG_ERROR(
        "D3D12ResourcesBinder: Failed to allocate contiguous SRV range (%u descriptors): %s\n",
        descriptorRangeSize,
        allocResult.message.c_str());
    if (outResult) {
      *outResult = allocResult;
    }
    return false;
  }

  // Create SRV descriptors for all texture slots from 0 to descriptorRangeSize-1.
  // For unbound slots, emit a null SRV so that the descriptor table is fully
  // initialized and matches the root signature descriptor range exactly.
  for (uint32_t i = 0; i < descriptorRangeSize; ++i) {
    const uint32_t descriptorIndex = baseDescriptorIndex + i;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(descriptorIndex);

    // Check if this slot is bound (may be null if beyond bindingsTextures_.count)
    auto* texture = (i < bindingsTextures_.count) ? bindingsTextures_.textures[i] : nullptr;
    if (!texture) {
      // Create an explicit null SRV descriptor. D3D12 does not permit both
      // the resource AND the descriptor pointer to be null, so we bind a
      // well-formed descriptor with zeroed fields instead. This is treated as
      // a null descriptor by the runtime.
      D3D12_SHADER_RESOURCE_VIEW_DESC nullSrv = {};
      nullSrv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      nullSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      nullSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      nullSrv.Texture2D.MipLevels = 1;
      device->CreateShaderResourceView(nullptr, &nullSrv, cpuHandle);
      D3D12Context::trackResourceCreation("SRV", 0);
      // Only cache handle if within bounds (avoid out-of-bounds write)
      if (i < IGL_TEXTURE_SAMPLERS_MAX) {
        bindingsTextures_.handles[i] = gpuHandle;
      }
      continue;
    }

    auto* d3dTexture = static_cast<Texture*>(texture);
    ID3D12Resource* resource = d3dTexture->getResource();
    if (!resource) {
      D3D12_SHADER_RESOURCE_VIEW_DESC nullSrv = {};
      nullSrv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      nullSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      nullSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      nullSrv.Texture2D.MipLevels = 1;
      device->CreateShaderResourceView(nullptr, &nullSrv, cpuHandle);
      D3D12Context::trackResourceCreation("SRV", 0);
      // Only cache handle if within bounds (avoid out-of-bounds write)
      if (i < IGL_TEXTURE_SAMPLERS_MAX) {
        bindingsTextures_.handles[i] = gpuHandle;
      }
      continue;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureFormatToDXGIShaderResourceViewFormat(d3dTexture->getFormat());
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    auto resourceDesc = resource->GetDesc();
    const bool isView = d3dTexture->isView();
    const uint32_t mostDetailedMip = isView ? d3dTexture->getMipLevelOffset() : 0;
    const uint32_t mipLevels = isView ? d3dTexture->getNumMipLevelsInView()
                                      : d3dTexture->getNumMipLevels();

    if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
      srvDesc.Texture3D.MipLevels = mipLevels;
      srvDesc.Texture3D.MostDetailedMip = mostDetailedMip;
    } else if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
      const auto textureType = d3dTexture->getType();
      const bool isArrayTexture = (isView && d3dTexture->getNumArraySlicesInView() > 0) ||
                                  (!isView && resourceDesc.DepthOrArraySize > 1);

      // Prioritize cube textures so that cubemaps created as 2D arrays
      // with 6 faces are exposed as TEXTURECUBE to shaders that declare
      // TextureCube / samplerCube.
      if (textureType == TextureType::Cube) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = mostDetailedMip;
        srvDesc.TextureCube.MipLevels = mipLevels;
      } else if (textureType == TextureType::TwoDArray || isArrayTexture) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MostDetailedMip = mostDetailedMip;
        srvDesc.Texture2DArray.MipLevels = mipLevels;
        srvDesc.Texture2DArray.FirstArraySlice = isView ? d3dTexture->getArraySliceOffset() : 0;
        srvDesc.Texture2DArray.ArraySize = isView ? d3dTexture->getNumArraySlicesInView()
                                                  : resourceDesc.DepthOrArraySize;
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

    // Cache the GPU handle (only if within bounds)
    if (i < IGL_TEXTURE_SAMPLERS_MAX) {
      bindingsTextures_.handles[i] = gpuHandle;
    }
  }

  // Bind the SRV table to the appropriate root parameter
  // Use the first descriptor in the allocated range (baseDescriptorIndex)
  D3D12_GPU_DESCRIPTOR_HANDLE tableBaseHandle = context.getCbvSrvUavGpuHandle(baseDescriptorIndex);

  if (isCompute_) {
    cmdList->SetComputeRootDescriptorTable(kComputeRootParam_SRVTable, tableBaseHandle);
  } else {
    // Graphics pipeline: Query reflection-based root parameter index from pipeline
    if (!renderPipeline) {
      IGL_LOG_ERROR("updateTextureBindings: renderPipeline is NULL, cannot bind SRV table\n");
    } else {
      const UINT srvTableIndex = renderPipeline->getSRVTableRootParameterIndex();
      IGL_D3D12_LOG_VERBOSE(
          "updateTextureBindings: srvTableIndex=%u (UINT_MAX=%u)\n", srvTableIndex, UINT_MAX);
      if (srvTableIndex != UINT_MAX) {
        cmdList->SetGraphicsRootDescriptorTable(srvTableIndex, tableBaseHandle);
        IGL_D3D12_LOG_VERBOSE(
            "updateTextureBindings: Bound SRV table to root param %u (range size %u)\n",
            srvTableIndex,
            descriptorRangeSize);
      } else {
        IGL_LOG_ERROR(
            "updateTextureBindings: srvTableIndex is UINT_MAX, shader doesn't use SRVs?\n");
      }
    }
  }

  return true;
}

bool D3D12ResourcesBinder::updateSamplerBindings(ID3D12GraphicsCommandList* cmdList,
                                                 ID3D12Device* device,
                                                 const RenderPipelineState* renderPipeline,
                                                 Result* outResult) {
  if (bindingsSamplers_.count == 0) {
    return true; // Nothing to bind
  }

  auto& context = commandBuffer_.getContext();

  // Determine how many descriptors to allocate based on pipeline's root signature
  // For graphics: Use pipeline's declared sampler range (0 to maxSamplerSlot inclusive)
  // For compute: Use bindingsSamplers_.count (legacy behavior)
  uint32_t descriptorRangeSize = bindingsSamplers_.count;

  if (!isCompute_ && renderPipeline) {
    // Graphics pipeline: Match root signature's sampler descriptor range exactly
    const UINT pipelineSamplerCount = renderPipeline->getSamplerDescriptorCount();
    if (pipelineSamplerCount > 0) {
      descriptorRangeSize = pipelineSamplerCount;
      IGL_D3D12_LOG_VERBOSE(
          "updateSamplerBindings: Using pipeline sampler range size=%u (bound=%u)\n",
          descriptorRangeSize,
          bindingsSamplers_.count);
    }
  }

  // Get base sampler descriptor index for contiguous allocation
  uint32_t baseSamplerIndex = commandBuffer_.getNextSamplerDescriptor();

  // Create sampler descriptors for all slots from 0 to descriptorRangeSize-1
  // For unbound slots, create a default sampler to fill the table
  for (uint32_t i = 0; i < descriptorRangeSize; ++i) {
    const uint32_t descriptorIndex = baseSamplerIndex + i;

    // Get descriptor handles
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getSamplerCpuHandle(descriptorIndex);
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getSamplerGpuHandle(descriptorIndex);

    // Check if this slot is bound (may be null if beyond bindingsSamplers_.count)
    auto* samplerState = (i < bindingsSamplers_.count) ? bindingsSamplers_.samplers[i] : nullptr;

    // Create sampler descriptor
    D3D12_SAMPLER_DESC samplerDesc = {};
    if (samplerState) {
      if (auto* d3dSampler = dynamic_cast<SamplerState*>(samplerState)) {
        samplerDesc = d3dSampler->getDesc();
      } else {
        // Fallback for bound but invalid sampler
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
    } else {
      // Unbound slot: Create default sampler for unused descriptor table entries
      samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
      samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
      samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
      samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
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

    // Cache the GPU handle (only if within bounds)
    if (i < IGL_TEXTURE_SAMPLERS_MAX) {
      bindingsSamplers_.handles[i] = gpuHandle;
    }
  }

  // Update sampler descriptor counter to reserve the allocated range
  commandBuffer_.getNextSamplerDescriptor() = baseSamplerIndex + descriptorRangeSize;

  // Bind the sampler table to the appropriate root parameter
  // Use the first descriptor in the allocated range
  D3D12_GPU_DESCRIPTOR_HANDLE tableBaseHandle = context.getSamplerGpuHandle(baseSamplerIndex);
  if (isCompute_) {
    cmdList->SetComputeRootDescriptorTable(kComputeRootParam_SamplerTable, tableBaseHandle);
  } else {
    // Graphics pipeline: Query reflection-based root parameter index from pipeline
    if (!renderPipeline) {
      IGL_LOG_ERROR("updateSamplerBindings: renderPipeline is NULL, cannot bind sampler table\n");
    } else {
      const UINT samplerTableIndex = renderPipeline->getSamplerTableRootParameterIndex();
      IGL_D3D12_LOG_VERBOSE("updateSamplerBindings: samplerTableIndex=%u (UINT_MAX=%u)\n",
                            samplerTableIndex,
                            UINT_MAX);
      if (samplerTableIndex != UINT_MAX) {
        cmdList->SetGraphicsRootDescriptorTable(samplerTableIndex, tableBaseHandle);
        IGL_D3D12_LOG_VERBOSE(
            "updateSamplerBindings: Bound sampler table to root param %u (range size %u)\n",
            samplerTableIndex,
            descriptorRangeSize);
      } else {
        IGL_LOG_ERROR(
            "updateSamplerBindings: samplerTableIndex is UINT_MAX, shader doesn't use samplers?\n");
      }
    }
  }

  return true;
}

bool D3D12ResourcesBinder::updateBufferBindings(ID3D12GraphicsCommandList* cmdList,
                                                ID3D12Device* device,
                                                const RenderPipelineState* renderPipeline,
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
      IGL_LOG_ERROR(
          "D3D12ResourcesBinder: Compute CBV bindings are sparse (slot 0 not bound). "
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
        IGL_LOG_ERROR(
            "D3D12ResourcesBinder: Sparse compute CBV binding detected at slot %u "
            "(expected dense binding through slot %u)\n",
            i,
            boundCbvCount - 1);
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
      IGL_LOG_ERROR(
          "D3D12ResourcesBinder: Failed to allocate contiguous CBV range (%u descriptors): %s\n",
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
        IGL_LOG_ERROR(
            "D3D12ResourcesBinder: Constant buffer %u address 0x%llx is not 256-byte aligned\n",
            i,
            bindingsBuffers_.addresses[i]);
        if (outResult) {
          *outResult = Result{Result::Code::ArgumentInvalid,
                              "Constant buffer address must be 256-byte aligned"};
        }
        return false;
      }

      // Validate size
      size_t size = bindingsBuffers_.sizes[i];
      if (size > kMaxCBVSize) {
        IGL_LOG_ERROR(
            "D3D12ResourcesBinder: Constant buffer %u size (%zu bytes) exceeds 64 KB limit\n",
            i,
            size);
        if (outResult) {
          *outResult = Result{Result::Code::ArgumentOutOfRange,
                              "Constant buffer size exceeds 64 KB D3D12 limit"};
        }
        return false;
      }

      // Align size to 256-byte boundary
      const size_t alignedSize =
          (size + kConstantBufferAlignment - 1) & ~(kConstantBufferAlignment - 1);

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
                     boundCbvCount,
                     descriptorOffset);

    // Bind the CBV descriptor table to root parameter 3
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(baseDescriptorIndex);
    cmdList->SetComputeRootDescriptorTable(kComputeRootParam_CBVTable, gpuHandle);
  } else {
    // Graphics pipeline: Reflection-based CBV descriptor table binding
    auto& context = commandBuffer_.getContext();

    // Count bound CBVs
    uint32_t boundCbvCount = 0;
    for (uint32_t i = 0; i < bindingsBuffers_.count; ++i) {
      if (bindingsBuffers_.addresses[i] != 0) {
        boundCbvCount++;
      }
    }

    if (boundCbvCount == 0) {
      return true; // No CBVs to bind
    }

    // Determine how many descriptors to allocate based on pipeline's root signature
    // Use pipeline's declared CBV range (0 to maxCBVSlot inclusive) to match root signature
    uint32_t descriptorRangeSize = bindingsBuffers_.count;

    if (renderPipeline) {
      const UINT pipelineCBVCount = renderPipeline->getCBVDescriptorCount();
      if (pipelineCBVCount > 0) {
        descriptorRangeSize = pipelineCBVCount;
      }
    }

    // Allocate a contiguous range of descriptors from 0 to descriptorRangeSize-1
    uint32_t baseDescriptorIndex = 0;
    Result allocResult =
        commandBuffer_.allocateCbvSrvUavRange(descriptorRangeSize, &baseDescriptorIndex);
    if (!allocResult.isOk()) {
      IGL_LOG_ERROR("D3D12ResourcesBinder: Failed to allocate CBV range (%u descriptors): %s\n",
                    descriptorRangeSize,
                    allocResult.message.c_str());
      if (outResult) {
        *outResult = allocResult;
      }
      return false;
    }

    IGL_D3D12_LOG_VERBOSE(
        "updateBufferBindings: Graphics CBV binding - range b0-b%u, %u descriptors\n",
        descriptorRangeSize - 1,
        descriptorRangeSize);

    // Create CBV descriptors for all slots from 0 to descriptorRangeSize-1
    // For unbound slots, create null descriptors to match the root signature range
    for (uint32_t slotIndex = 0; slotIndex < descriptorRangeSize; ++slotIndex) {
      const uint32_t descriptorIndex = baseDescriptorIndex + slotIndex;
      D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);

      // Check if this slot is bound (may be null if beyond bindingsBuffers_.count)
      const bool isSlotBound = (slotIndex < bindingsBuffers_.count) &&
                               (bindingsBuffers_.addresses[slotIndex] != 0);

      if (isSlotBound) {
        // Bound slot: Create valid CBV descriptor
        // Validate address alignment (D3D12 requires 256-byte alignment)
        if (bindingsBuffers_.addresses[slotIndex] % kConstantBufferAlignment != 0) {
          IGL_LOG_ERROR(
              "D3D12ResourcesBinder: Constant buffer %u address 0x%llx is not 256-byte aligned\n",
              slotIndex,
              bindingsBuffers_.addresses[slotIndex]);
          if (outResult) {
            *outResult = Result{Result::Code::ArgumentInvalid,
                                "Constant buffer address must be 256-byte aligned"};
          }
          return false;
        }

        // Validate size
        size_t size = bindingsBuffers_.sizes[slotIndex];
        if (size > kMaxCBVSize) {
          IGL_LOG_ERROR(
              "D3D12ResourcesBinder: Constant buffer %u size (%zu bytes) exceeds 64 KB limit\n",
              slotIndex,
              size);
          if (outResult) {
            *outResult = Result{Result::Code::ArgumentOutOfRange,
                                "Constant buffer size exceeds 64 KB D3D12 limit"};
          }
          return false;
        }

        // Align size to 256-byte boundary
        const size_t alignedSize =
            (size + kConstantBufferAlignment - 1) & ~(kConstantBufferAlignment - 1);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = bindingsBuffers_.addresses[slotIndex];
        cbvDesc.SizeInBytes = static_cast<UINT>(alignedSize);

        device->CreateConstantBufferView(&cbvDesc, cpuHandle);
        IGL_D3D12_LOG_VERBOSE(
            "D3D12ResourcesBinder: Created CBV descriptor for b%u (address=0x%llx, size=%u)\n",
            slotIndex,
            cbvDesc.BufferLocation,
            cbvDesc.SizeInBytes);
      } else {
        // Unbound slot: Create NULL descriptor to fill the root signature descriptor range
        D3D12_CONSTANT_BUFFER_VIEW_DESC nullCbvDesc = {};
        nullCbvDesc.BufferLocation = 0; // NULL CBV
        nullCbvDesc.SizeInBytes =
            D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16; // Minimum valid size

        device->CreateConstantBufferView(&nullCbvDesc, cpuHandle);
        IGL_D3D12_LOG_VERBOSE("D3D12ResourcesBinder: Created NULL CBV descriptor for b%u\n",
                              slotIndex);
      }
    }

    // Query pipeline for reflection-based CBV table root parameter index
    if (!renderPipeline) {
      IGL_LOG_ERROR("updateBufferBindings: renderPipeline is NULL, cannot bind CBV table\n");
      if (outResult) {
        *outResult = Result{Result::Code::ArgumentInvalid,
                            "renderPipeline is required for graphics CBV binding"};
      }
      return false;
    }

    const UINT cbvTableIndex = renderPipeline->getCBVTableRootParameterIndex();

    if (cbvTableIndex != UINT_MAX) {
      // Bind the CBV descriptor table to the reflection-based root parameter
      D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(baseDescriptorIndex);
      cmdList->SetGraphicsRootDescriptorTable(cbvTableIndex, gpuHandle);
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
    IGL_LOG_ERROR(
        "D3D12ResourcesBinder: UAV bindings are sparse (slot 0 not bound). "
        "D3D12 requires dense bindings starting at index 0.\n");
    if (outResult) {
      *outResult =
          Result{Result::Code::InvalidOperation, "UAV bindings must be dense starting at slot 0"};
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
  Result allocResult =
      commandBuffer_.allocateCbvSrvUavRange(bindingsUAVs_.count, &baseDescriptorIndex);
  if (!allocResult.isOk()) {
    IGL_LOG_ERROR(
        "D3D12ResourcesBinder: Failed to allocate contiguous UAV range (%u descriptors): %s\n",
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
        *outResult =
            Result{Result::Code::ArgumentInvalid, "UAV offset must be aligned to element stride"};
      }
      return false;
    }

    // FATAL VALIDATION: UAV offset must be within buffer bounds
    // This check immediately fails the entire updateBindings() call and returns ArgumentOutOfRange.
    // Out-of-bounds offsets would access invalid memory and cause GPU faults.
    if (offset > bufferSize) {
      IGL_LOG_ERROR(
          "D3D12ResourcesBinder: UAV offset %zu exceeds buffer size %zu. "
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
      IGL_LOG_ERROR(
          "D3D12ResourcesBinder: UAV remaining size %zu < element stride %zu. "
          "This is a FATAL error - updateBindings() will fail.\n",
          remaining,
          elementStride);
      if (outResult) {
        *outResult =
            Result{Result::Code::ArgumentOutOfRange, "UAV remaining size less than element stride"};
      }
      return false;
    }

    // Use contiguous descriptor index (baseDescriptorIndex + i)
    const uint32_t descriptorIndex = baseDescriptorIndex + i;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(descriptorIndex);

    // Create UAV descriptor for structured buffer
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN; // Required for structured buffers
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
