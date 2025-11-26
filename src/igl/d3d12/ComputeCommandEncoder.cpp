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
  commandBuffer_(commandBuffer), resourcesBinder_(commandBuffer, true /* isCompute */), isEncoding_(true) {
  IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder created\n");

  // Set descriptor heaps for this command list.
  // Use active heap from frame context, not the legacy accessor.
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
      IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder: Descriptor heaps set (active heap from FrameContext)\n");
    }
  }
}

void ComputeCommandEncoder::endEncoding() {
  if (!isEncoding_) {
    return;
  }

  IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder::endEncoding()\n");
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
  if (!commandBuffer_.isRecording() || !commandList) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindComputePipelineState - command list is closed or null\n");
    return;
  }

  // Set compute root signature and pipeline state
  commandList->SetComputeRootSignature(currentPipeline_->getRootSignature());
  commandList->SetPipelineState(currentPipeline_->getPipelineState());

  IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder::bindComputePipelineState - PSO and root signature set\n");
}

void ComputeCommandEncoder::dispatchThreadGroups(const Dimensions& threadgroupCount,
                                                 const Dimensions& /*threadgroupSize*/,
                                                 const Dependencies& dependencies) {
  if (!currentPipeline_) {
    IGL_LOG_ERROR("ComputeCommandEncoder::dispatchThreadGroups - no pipeline state bound\n");
    return;
  }

  auto* commandList = commandBuffer_.getCommandList();
  if (!commandBuffer_.isRecording() || !commandList) {
    IGL_LOG_ERROR("ComputeCommandEncoder::dispatchThreadGroups - command list is closed or null\n");
    return;
  }

  IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder::dispatchThreadGroups(%u, %u, %u)\n",
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
    IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder: Inserted %zu UAV barriers before dispatch\n", barriers.size());
  }

  // Apply all resource bindings (textures, samplers, buffers, UAVs) before dispatch.
  // For compute pipelines, pass nullptr since there's no RenderPipelineState
  Result bindResult;
  if (!resourcesBinder_.updateBindings(nullptr, &bindResult)) {
    IGL_LOG_ERROR("dispatchThreadGroups: Failed to update resource bindings: %s\n", bindResult.message.c_str());
    return;
  }

  // Bind all cached resources to root parameters
  // Root signature layout (from Device::createComputePipeline):
  // - Parameter 0: Root Constants (b0) - 16 DWORDs
  // - Parameter 1: UAV table (u0-uN)
  // - Parameter 2: SRV table (t0-tN)
  // - Parameter 3: CBV table (b1-bN)
  // - Parameter 4: Sampler table (s0-sN)

  // Bind UAVs (parameter 1), with debug validation to catch sparse binding.
  if (boundUavCount_ > 0) {
    IGL_DEBUG_ASSERT(cachedUavHandles_[0].ptr != 0,
                     "UAV count > 0 but base handle is null - did you bind only higher slots?");
    if (cachedUavHandles_[0].ptr != 0) {
      commandList->SetComputeRootDescriptorTable(1, cachedUavHandles_[0]);
      IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder: Bound %zu UAVs\n", boundUavCount_);
    } else {
      IGL_LOG_ERROR("ComputeCommandEncoder: UAV count > 0 but base handle is null - skipping binding and clearing boundUavCount_ to 0\n");
      // Clear count to avoid repeated errors on subsequent dispatches
      boundUavCount_ = 0;
    }
  }

  // Bind SRVs (Parameter 2)
  if (boundSrvCount_ > 0) {
    IGL_DEBUG_ASSERT(cachedSrvHandles_[0].ptr != 0,
                     "SRV count > 0 but base handle is null - did you bind only higher slots?");
    if (cachedSrvHandles_[0].ptr != 0) {
      commandList->SetComputeRootDescriptorTable(2, cachedSrvHandles_[0]);
      IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder: Bound %zu SRVs\n", boundSrvCount_);
    } else {
      IGL_LOG_ERROR("ComputeCommandEncoder: SRV count > 0 but base handle is null - skipping binding and clearing boundSrvCount_ to 0\n");
      // Clear count to avoid repeated errors on subsequent dispatches
      boundSrvCount_ = 0;
    }
  }

  // Bind CBVs (parameter 3). Only create/allocate CBV descriptors when bindings have
  // changed or the heap page has changed.
  if (boundCbvCount_ > 0) {
    auto& context = commandBuffer_.getContext();
    auto& frameCtx = context.getFrameContexts()[context.getCurrentFrameIndex()];
    const uint32_t currentPageIdx = frameCtx.currentCbvSrvUavPageIndex;

    // Check if heap page changed - invalidates cached descriptors
    const bool heapPageChanged = (cachedCbvPageIndex_ != currentPageIdx);
    if (heapPageChanged) {
      cbvBindingsDirty_ = true;
      IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder: Heap page changed (%u -> %u), invalidating CBV cache\n",
                   cachedCbvPageIndex_, currentPageIdx);
    }

    // Only recreate descriptors if bindings are dirty or heap changed
    if (cbvBindingsDirty_) {
      auto* device = context.getDevice();

      // Allocate descriptors for CBV table - use fixed-size array to avoid heap allocation
      uint32_t cbvIndices[kMaxComputeBuffers] = {};
      for (size_t i = 0; i < boundCbvCount_; ++i) {
        uint32_t descriptorIndex = 0;
        Result allocResult = commandBuffer_.getNextCbvSrvUavDescriptor(&descriptorIndex);
        if (!allocResult.isOk()) {
          IGL_LOG_ERROR("ComputeCommandEncoder: Failed to allocate CBV descriptor %zu: %s\n", i, allocResult.message.c_str());
          return;
        }
        cbvIndices[i] = descriptorIndex;
      }

      // Create CBV descriptors for all bound constant buffers
      for (size_t i = 0; i < boundCbvCount_; ++i) {
        if (cachedCbvAddresses_[i] != 0 && cachedCbvSizes_[i] > 0) {
          const uint32_t descriptorIndex = cbvIndices[i];
          D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);

          // Enforce 64 KB limit for CBVs.
          constexpr size_t kMaxCBVSize = 65536;  // 64 KB (D3D12 spec limit)
          if (cachedCbvSizes_[i] > kMaxCBVSize) {
            IGL_LOG_ERROR("ComputeCommandEncoder: Constant buffer %zu size (%zu bytes) exceeds D3D12 64 KB limit\n",
                          i, cachedCbvSizes_[i]);
            continue;  // Skip this CBV
          }

          // Align size to 256-byte boundary (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
          const size_t alignedSize = (cachedCbvSizes_[i] + 255) & ~255;

          IGL_DEBUG_ASSERT(alignedSize <= kMaxCBVSize, "CBV size exceeds 64 KB after alignment");

          D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
          cbvDesc.BufferLocation = cachedCbvAddresses_[i];
          cbvDesc.SizeInBytes = static_cast<UINT>(alignedSize);

          device->CreateConstantBufferView(&cbvDesc, cpuHandle);
        }
      }

      // Cache the base index and page for reuse
      cachedCbvBaseIndex_ = cbvIndices[0];
      cachedCbvPageIndex_ = currentPageIdx;
      cbvBindingsDirty_ = false;

      IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder: Created %zu CBV descriptors at page %u (descriptors %u-%u)\n",
                   boundCbvCount_, currentPageIdx, cbvIndices[0], cbvIndices[boundCbvCount_ - 1]);
    }

    // Recompute GPU handle from cached base index for current heap
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(cachedCbvBaseIndex_);

    // Defensive check: ensure handle is valid before binding
    IGL_DEBUG_ASSERT(gpuHandle.ptr != 0, "CBV count > 0 but GPU handle is null");
    if (gpuHandle.ptr != 0) {
      commandList->SetComputeRootDescriptorTable(3, gpuHandle);
      IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder: Bound %zu CBVs via descriptor table (base index %u)\n",
                   boundCbvCount_, cachedCbvBaseIndex_);
    } else {
      IGL_LOG_ERROR("ComputeCommandEncoder: CBV GPU handle is null, skipping binding\n");
    }
  }

  // Bind Samplers (Parameter 4)
  if (boundSamplerCount_ > 0) {
    IGL_DEBUG_ASSERT(cachedSamplerHandles_[0].ptr != 0,
                     "Sampler count > 0 but base handle is null - did you bind only higher slots?");
    if (cachedSamplerHandles_[0].ptr != 0) {
      commandList->SetComputeRootDescriptorTable(4, cachedSamplerHandles_[0]);
      IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder: Bound %zu samplers\n", boundSamplerCount_);
    } else {
      IGL_LOG_ERROR("ComputeCommandEncoder: Sampler count > 0 but base handle is null - skipping binding and clearing boundSamplerCount_ to 0\n");
      // Clear count to avoid repeated errors on subsequent dispatches
      boundSamplerCount_ = 0;
    }
  }

  // Dispatch compute work
  // Note: threadgroupSize is embedded in the compute shader ([numthreads(...)])
  commandList->Dispatch(threadgroupCount.width, threadgroupCount.height, threadgroupCount.depth);

  // Insert resource-specific UAV barriers for bound UAVs to ensure compute writes are visible.
  // Only barrier UAVs that were actually bound (more efficient than a global barrier).
  if (boundUavCount_ > 0) {
    // Use fixed-size array to avoid heap allocation in hot path
    D3D12_RESOURCE_BARRIER barriers[kMaxComputeBuffers];
    UINT barrierCount = 0;

    for (size_t i = 0; i < boundUavCount_; ++i) {
      if (boundUavResources_[i] != nullptr) {
        D3D12_RESOURCE_BARRIER& barrier = barriers[barrierCount++];
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.UAV.pResource = boundUavResources_[i];  // Resource-specific UAV barrier
      }
    }

    if (barrierCount > 0) {
      commandList->ResourceBarrier(barrierCount, barriers);
      IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder::dispatchThreadGroups - dispatch complete, %u resource-specific UAV barriers inserted\n",
                   barrierCount);
    }
  }
}

void ComputeCommandEncoder::bindPushConstants(const void* data,
                                              size_t length,
                                              size_t offset) {
  auto* commandList = commandBuffer_.getCommandList();
  if (!commandBuffer_.isRecording() || !commandList || !data || length == 0) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindPushConstants: Invalid parameters or closed command list (list=%p, data=%p, len=%zu)\n",
                  commandList, data, length);
    return;
  }

  // Compute root signature parameter 0 is declared as D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS (b0).
  // Increased to 32 DWORDs (128 bytes) to match Vulkan.
  constexpr size_t kMaxPushConstantBytes = 128;

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

  IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder::bindPushConstants: Set %u DWORDs (%zu bytes) at offset %zu to root parameter 0 (b0)\n",
               num32BitValues, length, offset);
}

void ComputeCommandEncoder::bindTexture(uint32_t index, ITexture* texture) {
  // Delegate to D3D12ResourcesBinder for centralized descriptor management.
  resourcesBinder_.bindTexture(index, texture);
}

void ComputeCommandEncoder::bindBuffer(uint32_t index, IBuffer* buffer, size_t offset, size_t /*bufferSize*/) {
  if (!buffer) {
    IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder::bindBuffer: null buffer\n");
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

  IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder::bindBuffer(%u): isUniform=%d, isStorage=%d\n",
               index, isUniformBuffer, isStorageBuffer);

  if (isStorageBuffer) {
    // Storage buffer - bind as UAV (unordered access view) for read/write
    if (index >= kMaxComputeBuffers) {
      IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: UAV index %u exceeds max %zu\n",
                    index, kMaxComputeBuffers);
      return;
    }

    // Determine element stride for structured buffer views
    // If storageStride is not specified, default to 4 bytes to preserve existing behavior
    size_t elementStride = d3dBuffer->getStorageElementStride();
    if (elementStride == 0) {
      elementStride = 4;
    }

    // D3D12 requires UAV buffer views to use element-aligned offsets
    if (offset % elementStride != 0) {
      IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: Storage buffer offset %zu is not aligned to "
                    "element stride (%zu bytes). UAV FirstElement will be truncated (offset/stride).\n",
                    offset, elementStride);
      // Continue but log warning – FirstElement below uses integer division
    }

    // Validate offset doesn't exceed buffer size to prevent underflow
    const size_t bufferSizeBytes = d3dBuffer->getSizeInBytes();
    if (offset > bufferSizeBytes) {
      IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: Storage buffer offset %zu exceeds buffer size %zu; skipping UAV binding\n",
                    offset, bufferSizeBytes);
      return;
    }
    const size_t remaining = bufferSizeBytes - offset;

    // Check for undersized buffer (would create empty or partial view)
    if (remaining < elementStride) {
      IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: Remaining buffer size %zu is less than element stride %zu; "
                    "UAV will have NumElements=0 (empty view). Check buffer size and offset.\n",
                    remaining, elementStride);
      // Continue to create the descriptor, but it will be empty (NumElements=0)
    }

    // Use Result-based allocation with dynamic heap growth.
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
    // Element index and count are expressed in units of elementStride bytes
    // Division truncates if offset is not aligned; see warning above
    uavDesc.Buffer.FirstElement = static_cast<UINT64>(offset / elementStride);
    // CRITICAL: NumElements must be (size - offset) / stride, not total size / stride
    uavDesc.Buffer.NumElements = static_cast<UINT>(remaining / elementStride);
    uavDesc.Buffer.StructureByteStride = static_cast<UINT>(elementStride);
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE; // No flags for structured buffers

    // Pre-creation validation.
    IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateUnorderedAccessView");
    IGL_DEBUG_ASSERT(d3dBuffer->getResource() != nullptr, "Buffer resource is null");
    IGL_DEBUG_ASSERT(cpuHandle.ptr != 0, "UAV descriptor handle is invalid");

    device->CreateUnorderedAccessView(d3dBuffer->getResource(), nullptr, &uavDesc, cpuHandle);

    cachedUavHandles_[index] = gpuHandle;
    for (size_t i = index + 1; i < kMaxComputeBuffers; ++i) {
      cachedUavHandles_[i] = {};
    }
    boundUavCount_ = static_cast<size_t>(index + 1);

    // Track UAV resource for precise barrier synchronization.
    // Note: UAV bindings are assumed to be dense (slots 0..boundUavCount_-1).
    // Both cachedUavHandles_ and boundUavResources_ rely on this invariant.
    boundUavResources_[index] = d3dBuffer->getResource();
    for (size_t i = index + 1; i < kMaxComputeBuffers; ++i) {
      boundUavResources_[i] = nullptr;
    }

    IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder::bindBuffer: Created UAV at index %u, descriptor slot %u\n",
                 index, descriptorIndex);

    commandBuffer_.trackTransientResource(d3dBuffer->getResource());
  } else if (isUniformBuffer) {
    // Uniform buffer - bind as CBV (constant buffer view)
    if (index >= kMaxComputeBuffers) {
      IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: CBV index %u exceeds max %zu\n",
                    index, kMaxComputeBuffers);
      return;
    }

    // Enforce dense binding: CBVs must start at slot 0 with no gaps
    if (index > 0 && cachedCbvAddresses_[0] == 0) {
      IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: CBV bindings must be dense starting from slot 0. "
                    "Cannot bind slot %u when slot 0 is not bound.\n", index);
      return;
    }

    // Check for gaps in bindings
    for (size_t i = 0; i < index; ++i) {
      if (cachedCbvAddresses_[i] == 0) {
        IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: CBV bindings must be dense. "
                      "Cannot bind slot %u when slot %zu is not bound (gap detected).\n", index, i);
        return;
      }
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
    // Store buffer size for CBV descriptor creation on the next dispatch.
    // Actual descriptor creation happens in dispatchThreadGroups when cbvBindingsDirty_ is set.
    size_t bufferSize = d3dBuffer->getSizeInBytes() - offset;

    // D3D12 spec: Constant buffers must be ≤ 64 KB
    constexpr size_t kMaxCBVSize = 65536;  // 64 KB
    if (bufferSize > kMaxCBVSize) {
      IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: Buffer size (%zu bytes) exceeds D3D12 64 KB limit for constant buffers at index %u. Clamping to 64 KB.\n",
                    bufferSize, index);
      bufferSize = kMaxCBVSize;
    }

    cachedCbvSizes_[index] = bufferSize;
    for (size_t i = index + 1; i < kMaxComputeBuffers; ++i) {
      cachedCbvAddresses_[i] = 0;
      cachedCbvSizes_[i] = 0;
    }
    boundCbvCount_ = static_cast<size_t>(index + 1);

    // Mark CBV bindings as dirty to trigger descriptor recreation on the next dispatch.
    cbvBindingsDirty_ = true;

    IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder::bindBuffer: Cached CBV at index %u, address 0x%llx, size %zu\n",
                 index, cachedCbvAddresses_[index], cachedCbvSizes_[index]);

    commandBuffer_.trackTransientResource(d3dBuffer->getResource());
  } else {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindBuffer: Buffer must be Uniform or Storage type\n");
  }
}

void ComputeCommandEncoder::bindUniform(const UniformDesc& /*uniformDesc*/, const void* /*data*/) {
  // Single uniform binding not supported in D3D12
  // Use uniform buffers (CBVs) instead
  IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder::bindUniform - not supported, use uniform buffers\n");
}

void ComputeCommandEncoder::bindBytes(uint32_t /*index*/, const void* /*data*/, size_t /*length*/) {

  // D3D12 backend does not support bindBytes
  // Applications should use uniform buffers (bindBuffer) instead
  // This is a no-op to maintain compatibility with cross-platform code
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
  IGL_LOG_INFO_ONCE("bindBytes is not supported in D3D12 backend. Use bindBuffer with uniform buffers instead.\n");
}

void ComputeCommandEncoder::bindImageTexture(uint32_t index, ITexture* texture, TextureFormat /*format*/) {
  if (!texture) {
    IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder::bindImageTexture: null texture\n");
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

  // Allocate descriptor and create UAV using Result-based allocation with dynamic heap growth.
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

  // Pre-creation validation.
  IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateUnorderedAccessView");
  IGL_DEBUG_ASSERT(d3dTexture->getResource() != nullptr, "Texture resource is null");
  IGL_DEBUG_ASSERT(cpuHandle.ptr != 0, "UAV descriptor handle is invalid");

  device->CreateUnorderedAccessView(d3dTexture->getResource(), nullptr, &uavDesc, cpuHandle);

  cachedUavHandles_[index] = gpuHandle;
  for (size_t i = index + 1; i < kMaxComputeBuffers; ++i) {
    cachedUavHandles_[i] = {};
  }
  boundUavCount_ = static_cast<size_t>(index + 1);

  // Track UAV resources for precise barrier synchronization.
  // Note: UAV bindings are assumed to be dense (slots 0..boundUavCount_-1).
  // Both cachedUavHandles_ and boundUavResources_ rely on this invariant.
  boundUavResources_[index] = d3dTexture->getResource();
  for (size_t i = index + 1; i < kMaxComputeBuffers; ++i) {
    boundUavResources_[i] = nullptr;
  }

  IGL_D3D12_LOG_VERBOSE("ComputeCommandEncoder::bindImageTexture: Created UAV at index %u, descriptor slot %u\n",
               index, descriptorIndex);
}

void ComputeCommandEncoder::bindSamplerState(uint32_t index, ISamplerState* samplerState) {
  // Delegate to D3D12ResourcesBinder for centralized descriptor management.
  resourcesBinder_.bindSamplerState(index, samplerState);
}

void ComputeCommandEncoder::pushDebugGroupLabel(const char* label, const Color& /*color*/) const {
  auto* commandList = commandBuffer_.getCommandList();
  if (!commandBuffer_.isRecording() || !commandList || !label) {
    return;
  }
  // PIX debug markers
  const size_t len = strlen(label);
  std::wstring wlabel(len, L' ');
  std::mbstowcs(&wlabel[0], label, len);
  commandList->BeginEvent(
      0, wlabel.c_str(), static_cast<UINT>((wlabel.length() + 1) * sizeof(wchar_t)));
}

void ComputeCommandEncoder::insertDebugEventLabel(const char* label, const Color& /*color*/) const {
  auto* commandList = commandBuffer_.getCommandList();
  if (!commandBuffer_.isRecording() || !commandList || !label) {
    return;
  }
  const size_t len = strlen(label);
  std::wstring wlabel(len, L' ');
  std::mbstowcs(&wlabel[0], label, len);
  commandList->SetMarker(
      0, wlabel.c_str(), static_cast<UINT>((wlabel.length() + 1) * sizeof(wchar_t)));
}

void ComputeCommandEncoder::popDebugGroupLabel() const {
  auto* commandList = commandBuffer_.getCommandList();
  if (!commandBuffer_.isRecording() || !commandList) {
    return;
  }
  commandList->EndEvent();
}

} // namespace igl::d3d12
