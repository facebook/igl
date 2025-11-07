/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/CommandBuffer.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/RenderCommandEncoder.h>
#include <igl/d3d12/ComputeCommandEncoder.h>
#include <igl/d3d12/Buffer.h>
#include <igl/d3d12/Texture.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

CommandBuffer::CommandBuffer(Device& device, const CommandBufferDesc& desc)
    : ICommandBuffer(desc), device_(device) {
  auto* d3dDevice = device_.getD3D12Context().getDevice();

  if (!d3dDevice) {
    IGL_DEBUG_ABORT("D3D12 device is null - context not initialized");
    return;
  }

  // Check if device is in good state
  HRESULT deviceRemovedReason = d3dDevice->GetDeviceRemovedReason();
  if (FAILED(deviceRemovedReason)) {
    char errorMsg[512];
    snprintf(errorMsg, sizeof(errorMsg),
             "D3D12 device was removed before creating command buffer. Reason: 0x%08X\n"
             "  0x887A0005 = DXGI_ERROR_DEVICE_REMOVED\n"
             "  0x887A0006 = DXGI_ERROR_DEVICE_HUNG\n"
             "  0x887A0007 = DXGI_ERROR_DEVICE_RESET\n"
             "  0x887A0020 = DXGI_ERROR_DRIVER_INTERNAL_ERROR",
             static_cast<unsigned>(deviceRemovedReason));
    IGL_LOG_ERROR(errorMsg);
    IGL_DEBUG_ABORT("Device removed - see error above");
    return;
  }

  // Use the current frame's command allocator - allocators are created ready-to-use
  // Following Microsoft's D3D12HelloFrameBuffering: each frame has its own allocator
  auto& ctx = device_.getD3D12Context();
  const uint32_t frameIdx = ctx.getCurrentFrameIndex();
  auto* frameAllocator = ctx.getFrameContexts()[frameIdx].allocator.Get();

  HRESULT hr = d3dDevice->CreateCommandList(
      0,
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      frameAllocator,  // Use frame allocator directly - it's in ready-to-use state after creation
      nullptr,
      IID_PPV_ARGS(commandList_.GetAddressOf()));

  if (FAILED(hr)) {
    char errorMsg[256];
    snprintf(errorMsg, sizeof(errorMsg), "Failed to create command list: HRESULT = 0x%08X", static_cast<unsigned>(hr));
    IGL_DEBUG_ABORT(errorMsg);
  }

  // Command lists are created in recording state, close it for now
  commandList_->Close();

  // Create scheduling fence for waitUntilScheduled() support
  hr = d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(scheduleFence_.GetAddressOf()));
  if (FAILED(hr)) {
    char errorMsg[256];
    snprintf(errorMsg, sizeof(errorMsg), "Failed to create scheduling fence: HRESULT = 0x%08X", static_cast<unsigned>(hr));
    IGL_DEBUG_ABORT(errorMsg);
  }

  // Create event for fence waiting
  scheduleFenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!scheduleFenceEvent_) {
    IGL_DEBUG_ABORT("Failed to create scheduling fence event");
  }
}

CommandBuffer::~CommandBuffer() {
  // Clean up scheduling fence event handle
  if (scheduleFenceEvent_) {
    CloseHandle(scheduleFenceEvent_);
    scheduleFenceEvent_ = nullptr;
  }
  // scheduleFence_ is a ComPtr and will be automatically released
}

uint32_t& CommandBuffer::getNextCbvSrvUavDescriptor() {
  auto& ctx = device_.getD3D12Context();
  const uint32_t frameIdx = ctx.getCurrentFrameIndex();
  IGL_LOG_INFO("CommandBuffer::getNextCbvSrvUavDescriptor() - frame %u, current value=%u\n",
               frameIdx, ctx.getFrameContexts()[frameIdx].nextCbvSrvUavDescriptor);
  return ctx.getFrameContexts()[frameIdx].nextCbvSrvUavDescriptor;
}

uint32_t& CommandBuffer::getNextSamplerDescriptor() {
  auto& ctx = device_.getD3D12Context();
  const uint32_t frameIdx = ctx.getCurrentFrameIndex();
  IGL_LOG_INFO("CommandBuffer::getNextSamplerDescriptor() - frame %u, current value=%u\n",
               frameIdx, ctx.getFrameContexts()[frameIdx].nextSamplerDescriptor);
  return ctx.getFrameContexts()[frameIdx].nextSamplerDescriptor;
}

void CommandBuffer::trackTransientBuffer(std::shared_ptr<IBuffer> buffer) {
  // Add to the CURRENT frame's transient buffer list
  // These will be kept alive until the frame completes GPU execution
  auto& ctx = device_.getD3D12Context();
  const uint32_t frameIdx = ctx.getCurrentFrameIndex();
  ctx.getFrameContexts()[frameIdx].transientBuffers.push_back(std::move(buffer));
  IGL_LOG_INFO("CommandBuffer::trackTransientBuffer() - Added buffer to frame %u (total=%zu)\n",
               frameIdx, ctx.getFrameContexts()[frameIdx].transientBuffers.size());
}

void CommandBuffer::trackTransientResource(ID3D12Resource* resource) {
  if (!resource) {
    return;
  }
  auto& ctx = device_.getD3D12Context();
  const uint32_t frameIdx = ctx.getCurrentFrameIndex();
  Microsoft::WRL::ComPtr<ID3D12Resource> keepAlive;
  resource->AddRef();
  keepAlive.Attach(resource);
  ctx.getFrameContexts()[frameIdx].transientResources.push_back(std::move(keepAlive));
  IGL_LOG_INFO("CommandBuffer::trackTransientResource() - Added resource to frame %u (total=%zu)\n",
               frameIdx, ctx.getFrameContexts()[frameIdx].transientResources.size());
}

void CommandBuffer::begin() {
  if (recording_) {
    return;
  }

  // NOTE: Transient buffers are now stored in FrameContext and cleared when advancing frames
  // NOTE: Descriptor counters are now stored in FrameContext and shared across all CommandBuffers
  // They are reset at the start of each frame in CommandQueue::submit(), not here

  // Reset per-command-buffer draw count for this recording
  currentDrawCount_ = 0;

  // CRITICAL: Set the per-frame descriptor heaps before recording commands
  // Each frame has its own isolated heaps to prevent descriptor conflicts
  auto& ctx = device_.getD3D12Context();
  const uint32_t frameIdx = ctx.getCurrentFrameIndex();

  ID3D12DescriptorHeap* heaps[] = {
      ctx.getFrameContexts()[frameIdx].cbvSrvUavHeap.Get(),
      ctx.getFrameContexts()[frameIdx].samplerHeap.Get()
  };
  commandList_->SetDescriptorHeaps(2, heaps);

  IGL_LOG_INFO("CommandBuffer::begin() - Set per-frame descriptor heaps for frame %u\n", frameIdx);

  // Use the CURRENT FRAME's command allocator from FrameContext
  // Following Microsoft's D3D12HelloFrameBuffering pattern
  auto* frameAllocator = ctx.getFrameContexts()[frameIdx].allocator.Get();

  // Microsoft pattern: Reset allocator THEN reset command list
  // Allocator was reset in CommandQueue::submit() after fence wait, OR is in initial ready state
  IGL_LOG_INFO("CommandBuffer::begin() - Frame %u: Resetting command list with allocator...\n", frameIdx);
  HRESULT hr = commandList_->Reset(frameAllocator, nullptr);
  if (FAILED(hr)) {
    IGL_LOG_ERROR("CommandBuffer::begin() - Reset command list FAILED: 0x%08X\n", static_cast<unsigned>(hr));
    return;
  }
  IGL_LOG_INFO("CommandBuffer::begin() - Command list reset OK\n");
  recording_ = true;
}

void CommandBuffer::end() {
  if (!recording_) {
    return;
  }
  commandList_->Close();
  recording_ = false;
}

D3D12Context& CommandBuffer::getContext() {
  return device_.getD3D12Context();
}

const D3D12Context& CommandBuffer::getContext() const {
  return device_.getD3D12Context();
}

// Device draw count is incremented by CommandQueue::submit() using this buffer's count

std::unique_ptr<IRenderCommandEncoder> CommandBuffer::createRenderCommandEncoder(
    const RenderPassDesc& renderPass,
    const std::shared_ptr<IFramebuffer>& framebuffer,
    const Dependencies& /*dependencies*/,
    Result* IGL_NULLABLE outResult) {
  Result::setOk(outResult);

  // Begin command buffer if not already begun
  begin();

  return std::make_unique<RenderCommandEncoder>(*this, renderPass, framebuffer);
}

std::unique_ptr<IComputeCommandEncoder> CommandBuffer::createComputeCommandEncoder() {
  // Begin command buffer if not already begun
  begin();

  return std::make_unique<ComputeCommandEncoder>(*this);
}

void CommandBuffer::present(const std::shared_ptr<ITexture>& /*surface*/) const {
  // Note: Actual present happens in CommandQueue::submit()
  // This is just a marker to indicate that this command buffer will present when submitted
  // For D3D12, the present call must happen AFTER ExecuteCommandLists, so it's handled in submit()
}

void CommandBuffer::waitUntilScheduled() {
  // If scheduleValue_ is 0, the command buffer hasn't been submitted yet
  if (scheduleValue_ == 0) {
    IGL_LOG_INFO("CommandBuffer::waitUntilScheduled() - Not yet submitted, returning immediately\n");
    return;
  }

  // Check if the scheduling fence has already been signaled
  if (!scheduleFence_.Get()) {
    IGL_LOG_ERROR("CommandBuffer::waitUntilScheduled() - Scheduling fence is null\n");
    return;
  }

  const UINT64 completedValue = scheduleFence_->GetCompletedValue();
  if (completedValue >= scheduleValue_) {
    IGL_LOG_INFO("CommandBuffer::waitUntilScheduled() - Already scheduled (completed=%llu, target=%llu)\n",
                 completedValue, scheduleValue_);
    return;
  }

  // Wait for the scheduling fence to be signaled
  IGL_LOG_INFO("CommandBuffer::waitUntilScheduled() - Waiting for scheduling (completed=%llu, target=%llu)\n",
               completedValue, scheduleValue_);

  if (!scheduleFenceEvent_) {
    IGL_LOG_ERROR("CommandBuffer::waitUntilScheduled() - Fence event is null\n");
    return;
  }

  HRESULT hr = scheduleFence_->SetEventOnCompletion(scheduleValue_, scheduleFenceEvent_);
  if (FAILED(hr)) {
    IGL_LOG_ERROR("CommandBuffer::waitUntilScheduled() - SetEventOnCompletion failed: 0x%08X\n",
                  static_cast<unsigned>(hr));
    return;
  }

  // Re-check after SetEventOnCompletion to avoid race condition
  if (scheduleFence_->GetCompletedValue() < scheduleValue_) {
    WaitForSingleObject(scheduleFenceEvent_, INFINITE);
  }

  IGL_LOG_INFO("CommandBuffer::waitUntilScheduled() - Scheduling complete (fence now=%llu)\n",
               scheduleFence_->GetCompletedValue());
}

void CommandBuffer::waitUntilCompleted() {
  // Wait for all submitted GPU work to complete
  // The CommandQueue tracks frame completion via fences, so we need to wait for the current frame
  auto& ctx = getContext();
  auto* queue = ctx.getCommandQueue();
  if (!queue) {
    return;
  }

  // Signal a fence and wait for it
  // This ensures all previously submitted command lists have completed on the GPU
  Microsoft::WRL::ComPtr<ID3D12Fence> fence;
  auto* device = ctx.getDevice();
  if (!device || FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())))) {
    return;
  }

  HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!fenceEvent) {
    return;
  }

  queue->Signal(fence.Get(), 1);
  fence->SetEventOnCompletion(1, fenceEvent);
  WaitForSingleObject(fenceEvent, INFINITE);
  CloseHandle(fenceEvent);

  IGL_LOG_INFO("CommandBuffer::waitUntilCompleted() - GPU work completed\n");
}

void CommandBuffer::pushDebugGroupLabel(const char* label,
                                        const igl::Color& /*color*/) const {
  if (commandList_.Get() && label) {
    const size_t len = strlen(label);
    std::wstring wlabel(len, L' ');
    std::mbstowcs(&wlabel[0], label, len);
    commandList_->BeginEvent(0, wlabel.c_str(), static_cast<UINT>((wlabel.length() + 1) * sizeof(wchar_t)));
  }
}

void CommandBuffer::popDebugGroupLabel() const {
  if (commandList_.Get()) {
    commandList_->EndEvent();
  }
}

void CommandBuffer::copyBuffer(IBuffer& source,
                               IBuffer& destination,
                               uint64_t sourceOffset,
                               uint64_t destinationOffset,
                               uint64_t size) {
  auto* src = static_cast<Buffer*>(&source);
  auto* dst = static_cast<Buffer*>(&destination);
  ID3D12Resource* srcRes = src->getResource();
  ID3D12Resource* dstRes = dst->getResource();
  if (!srcRes || !dstRes || size == 0) {
    return;
  }

  // Use a transient copy with appropriate heap handling
  auto& ctx = getContext();
  ID3D12Device* device = ctx.getDevice();
  ID3D12CommandQueue* queue = ctx.getCommandQueue();
  if (!device || !queue) {
    return;
  }

  auto doCopyOnList = [&](ID3D12GraphicsCommandList* list,
                          ID3D12Resource* dstResLocal,
                          uint64_t dstOffsetLocal) {
    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = srcRes;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = dstResLocal;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    list->ResourceBarrier(2, barriers);

    list->CopyBufferRegion(dstResLocal, dstOffsetLocal, srcRes, sourceOffset, size);

    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    list->ResourceBarrier(2, barriers);
  };

  if (dst->storage() == ResourceStorage::Shared) {
    // GPU cannot write into UPLOAD heap; use a READBACK staging buffer and then memcpy into UPLOAD
    D3D12_HEAP_PROPERTIES readbackHeap{};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size + destinationOffset;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Microsoft::WRL::ComPtr<ID3D12Resource> readback;
    HRESULT hr = device->CreateCommittedResource(&readbackHeap,
                                                  D3D12_HEAP_FLAG_NONE,
                                                  &desc,
                                                  D3D12_RESOURCE_STATE_COPY_DEST,
                                                  nullptr,
                                                  IID_PPV_ARGS(readback.GetAddressOf()));
    if (FAILED(hr)) {
      IGL_LOG_ERROR("copyBuffer: Failed to create READBACK buffer, hr=0x%08X\n", static_cast<unsigned>(hr));
      return;
    }

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                              IID_PPV_ARGS(allocator.GetAddressOf()))) ||
        FAILED(device->CreateCommandList(0,
                                         D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         allocator.Get(),
                                         nullptr,
                                         IID_PPV_ARGS(list.GetAddressOf())))) {
      IGL_LOG_ERROR("copyBuffer: Failed to create transient command list\n");
      return;
    }

    // Transition source to COPY_SOURCE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = srcRes;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    list->ResourceBarrier(1, &barrier);

    // Copy from source to readback (readback is already in COPY_DEST state)
    list->CopyBufferRegion(readback.Get(), destinationOffset, srcRes, sourceOffset, size);

    // Transition source back
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
    list->ResourceBarrier(1, &barrier);
    list->Close();
    ID3D12CommandList* lists[] = {list.Get()};
    queue->ExecuteCommandLists(1, lists);
    ctx.waitForGPU();

    // Map readback and copy into the UPLOAD buffer
    void* rbPtr = nullptr;
    D3D12_RANGE readRange{static_cast<SIZE_T>(destinationOffset), static_cast<SIZE_T>(destinationOffset + size)};
    if (SUCCEEDED(readback->Map(0, &readRange, &rbPtr)) && rbPtr) {
      // Map destination upload buffer
      Result r1;
      void* dstPtr = dst->map(BufferRange(size, destinationOffset), &r1);
      if (dstPtr && r1.isOk()) {
        std::memcpy(dstPtr, static_cast<uint8_t*>(rbPtr) + destinationOffset, size);
        dst->unmap();
      }
      readback->Unmap(0, nullptr);
    }
    return;
  }

  // Default path: copy using a transient command list to DEFAULT/COMMON destinations
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
  if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            IID_PPV_ARGS(allocator.GetAddressOf()))) ||
      FAILED(device->CreateCommandList(0,
                                       D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       allocator.Get(),
                                       nullptr,
                                       IID_PPV_ARGS(list.GetAddressOf())))) {
    return;
  }

  doCopyOnList(list.Get(), dstRes, destinationOffset);
  list->Close();
  ID3D12CommandList* lists2[] = {list.Get()};
  queue->ExecuteCommandLists(1, lists2);
  ctx.waitForGPU();
}

void CommandBuffer::copyTextureToBuffer(ITexture& source,
                                       IBuffer& destination,
                                       uint64_t destinationOffset,
                                       uint32_t mipLevel,
                                       uint32_t layer) {
  auto* srcTex = static_cast<Texture*>(&source);
  auto* dstBuf = static_cast<Buffer*>(&destination);

  ID3D12Resource* srcRes = srcTex->getResource();
  ID3D12Resource* dstRes = dstBuf->getResource();

  if (!srcRes || !dstRes) {
    IGL_LOG_ERROR("copyTextureToBuffer: Invalid source or destination resource\n");
    return;
  }

  auto& ctx = getContext();
  ID3D12Device* device = ctx.getDevice();
  ID3D12CommandQueue* queue = ctx.getCommandQueue();

  if (!device || !queue) {
    IGL_LOG_ERROR("copyTextureToBuffer: Device or command queue is null\n");
    return;
  }

  // Get texture description for GetCopyableFootprints
  D3D12_RESOURCE_DESC srcDesc = srcRes->GetDesc();
  DXGI_FORMAT dxgiFormat = textureFormatToDXGIFormat(srcTex->getFormat());

  // Calculate subresource index
  const uint32_t subresource = srcTex->calcSubresourceIndex(mipLevel, layer);

  // Get copyable footprint for this subresource
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
  UINT numRows;
  UINT64 rowSizeInBytes;
  UINT64 totalBytes;

  device->GetCopyableFootprints(&srcDesc,
                                subresource,
                                1,
                                destinationOffset,
                                &layout,
                                &numRows,
                                &rowSizeInBytes,
                                &totalBytes);

  // Calculate the unpacked texture data size (without D3D12 padding)
  // rowSizeInBytes is the unpadded row size, so we can use it directly
  const UINT64 unpackedDataSize = rowSizeInBytes * numRows * layout.Footprint.Depth;

  // Check if destination buffer is large enough for the unpacked data
  if (destinationOffset + unpackedDataSize > dstBuf->getSizeInBytes()) {
    IGL_LOG_ERROR("copyTextureToBuffer: Destination buffer too small (need %llu, have %zu)\n",
                  destinationOffset + unpackedDataSize, dstBuf->getSizeInBytes());
    return;
  }

  // For UPLOAD/READBACK buffers, we need an intermediate READBACK buffer
  // For DEFAULT buffers, we can copy directly
  bool needsReadbackStaging = (dstBuf->storage() == ResourceStorage::Shared);

  Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer;
  ID3D12Resource* copyDestination = dstRes;

  if (needsReadbackStaging) {
    // Create a READBACK buffer for staging
    // The readback buffer needs to be large enough to hold the data at layout.Offset
    D3D12_HEAP_PROPERTIES readbackHeap{};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC readbackDesc{};
    readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    readbackDesc.Width = layout.Offset + totalBytes;  // Use layout.Offset, not destinationOffset
    readbackDesc.Height = 1;
    readbackDesc.DepthOrArraySize = 1;
    readbackDesc.MipLevels = 1;
    readbackDesc.Format = DXGI_FORMAT_UNKNOWN;
    readbackDesc.SampleDesc.Count = 1;
    readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = device->CreateCommittedResource(&readbackHeap,
                                                  D3D12_HEAP_FLAG_NONE,
                                                  &readbackDesc,
                                                  D3D12_RESOURCE_STATE_COPY_DEST,
                                                  nullptr,
                                                  IID_PPV_ARGS(readbackBuffer.GetAddressOf()));
    if (FAILED(hr)) {
      IGL_LOG_ERROR("copyTextureToBuffer: Failed to create readback buffer, hr=0x%08X\n",
                    static_cast<unsigned>(hr));
      return;
    }

    copyDestination = readbackBuffer.Get();
  }

  // Create transient command list for the copy operation
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;

  if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            IID_PPV_ARGS(allocator.GetAddressOf()))) ||
      FAILED(device->CreateCommandList(0,
                                       D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       allocator.Get(),
                                       nullptr,
                                       IID_PPV_ARGS(cmdList.GetAddressOf())))) {
    IGL_LOG_ERROR("copyTextureToBuffer: Failed to create command list\n");
    return;
  }

  // Get current texture state
  D3D12_RESOURCE_STATES srcStateBefore = srcTex->getSubresourceState(mipLevel, layer);

  // Transition texture to COPY_SOURCE if needed
  if (srcStateBefore != D3D12_RESOURCE_STATE_COPY_SOURCE) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = srcRes;
    barrier.Transition.Subresource = subresource;
    barrier.Transition.StateBefore = srcStateBefore;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    cmdList->ResourceBarrier(1, &barrier);
  }

  // Setup source texture copy location
  D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
  srcLocation.pResource = srcRes;
  srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  srcLocation.SubresourceIndex = subresource;

  // Setup destination buffer copy location
  D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
  dstLocation.pResource = copyDestination;
  dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dstLocation.PlacedFootprint = layout;

  // Perform the copy
  cmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

  // Transition texture back to original state
  if (srcStateBefore != D3D12_RESOURCE_STATE_COPY_SOURCE) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = srcRes;
    barrier.Transition.Subresource = subresource;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter = srcStateBefore;
    cmdList->ResourceBarrier(1, &barrier);
  }

  cmdList->Close();

  // Execute the command list
  ID3D12CommandList* lists[] = {cmdList.Get()};
  queue->ExecuteCommandLists(1, lists);

  // Wait for GPU to complete
  ctx.waitForGPU();

  // If we used a readback staging buffer, copy to the final UPLOAD destination
  if (needsReadbackStaging) {
    void* readbackData = nullptr;
    // Map the readback buffer region containing the texture data
    D3D12_RANGE readRange{static_cast<SIZE_T>(layout.Offset),
                         static_cast<SIZE_T>(layout.Offset + totalBytes)};

    if (SUCCEEDED(readbackBuffer->Map(0, &readRange, &readbackData)) && readbackData) {
      // Map the destination UPLOAD buffer
      Result mapResult;
      void* dstData = dstBuf->map(BufferRange(unpackedDataSize, destinationOffset), &mapResult);

      if (dstData && mapResult.isOk()) {
        // Copy row-by-row, removing D3D12's row pitch padding
        const uint8_t* src = static_cast<uint8_t*>(readbackData) + layout.Offset;
        uint8_t* dst = static_cast<uint8_t*>(dstData);
        const UINT64 srcRowPitch = layout.Footprint.RowPitch;
        const UINT64 dstRowPitch = rowSizeInBytes;  // Unpadded row size

        for (UINT z = 0; z < layout.Footprint.Depth; ++z) {
          for (UINT row = 0; row < numRows; ++row) {
            std::memcpy(dst, src, dstRowPitch);
            src += srcRowPitch;
            dst += dstRowPitch;
          }
        }

        dstBuf->unmap();
      } else {
        IGL_LOG_ERROR("copyTextureToBuffer: Failed to map destination buffer\n");
      }

      readbackBuffer->Unmap(0, nullptr);
    } else {
      IGL_LOG_ERROR("copyTextureToBuffer: Failed to map readback buffer\n");
    }
  }
}

} // namespace igl::d3d12

