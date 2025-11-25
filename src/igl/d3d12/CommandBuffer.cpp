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
#include <igl/d3d12/Timer.h>
#include <igl/d3d12/D3D12FenceWaiter.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

CommandBuffer::CommandBuffer(Device& device, const CommandBufferDesc& desc)
    : ICommandBuffer(desc), device_(device) {
  auto* d3dDevice = device_.getD3D12Context().getDevice();

  if (!d3dDevice) {
    IGL_DEBUG_ASSERT(false, "D3D12 device is null - context not initialized");
    IGL_LOG_ERROR("D3D12 device is null - context not initialized");
    return;  // Leave commandList_ null to indicate failure
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
    IGL_DEBUG_ASSERT(false, "Device removed - see error above");
    return;  // Leave commandList_ null to indicate failure
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
    IGL_DEBUG_ASSERT(false, "%s", errorMsg);
    IGL_LOG_ERROR(errorMsg);
    return;  // Leave commandList_ null to indicate failure
  }

  // Command lists are created in recording state, close it for now
  commandList_->Close();

  // Create scheduling fence for waitUntilScheduled() support
  // D-003: Fence event is now created per-wait in waitUntilScheduled() to eliminate TOCTOU race
  hr = d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(scheduleFence_.GetAddressOf()));
  if (FAILED(hr)) {
    char errorMsg[256];
    snprintf(errorMsg, sizeof(errorMsg), "Failed to create scheduling fence: HRESULT = 0x%08X", static_cast<unsigned>(hr));
    IGL_DEBUG_ASSERT(false, "%s", errorMsg);
    IGL_LOG_ERROR(errorMsg);
    return;  // Leave fence null to indicate failure
  }
}

CommandBuffer::~CommandBuffer() {
  // D-003: No need to clean up scheduleFenceEvent_ - now using dedicated events per wait
  // scheduleFence_ is a ComPtr and will be automatically released
}

// Pre-allocated descriptor heap with fail-fast on exhaustion.
// Allocates from pre-allocated pages, switching pages as needed.
// Fails immediately if all pages are exhausted (Vulkan fail-fast pattern).
Result CommandBuffer::getNextCbvSrvUavDescriptor(uint32_t* outDescriptorIndex) {
  auto& ctx = device_.getD3D12Context();
  const uint32_t frameIdx = ctx.getCurrentFrameIndex();
  auto& frameCtx = ctx.getFrameContexts()[frameIdx];
  auto& pages = frameCtx.cbvSrvUavHeapPages;
  uint32_t currentPageIdx = frameCtx.currentCbvSrvUavPageIndex;

  // Validate we have at least one page
  if (pages.empty()) {
    return Result{Result::Code::RuntimeError, "No CBV/SRV/UAV descriptor heap pages available"};
  }

  // Get current page index validation
  if (currentPageIdx >= pages.size()) {
    return Result{Result::Code::RuntimeError, "Invalid descriptor heap page index"};
  }

  // Check current offset before acquiring reference (avoid use-after-reallocation)
  const uint32_t currentOffset = frameCtx.nextCbvSrvUavDescriptor;

  // Check if current page has space; fail fast if pre-allocation is enabled.
  if (currentOffset >= pages[currentPageIdx].capacity) {
    // Current page is full - check if we can move to next page
    const uint32_t nextPageIdx = currentPageIdx + 1;

    // Fail-fast if pre-allocated pages are exhausted (Vulkan pattern).
    if (nextPageIdx >= pages.size()) {
      char errorMsg[512];
      // All pages exhausted - fail immediately (no mid-frame allocation).
      // Calculate actual descriptor capacity from allocated pages.
      uint32_t totalCapacity = 0;
      for (const auto& page : pages) {
        totalCapacity += page.capacity;
      }
      snprintf(errorMsg, sizeof(errorMsg),
               "CBV/SRV/UAV descriptor heap exhausted! Frame %u used all %zu pre-allocated pages (%u descriptors total). "
               "This frame requires more descriptors than available. "
               "Increase D3D12ContextConfig::maxHeapPages or enable preAllocateDescriptorPages=true, or optimize descriptor usage.",
               frameIdx, pages.size(), totalCapacity);
      return Result{Result::Code::RuntimeError, errorMsg};
    }

    // Move to next pre-allocated page.
    currentPageIdx = nextPageIdx;
    frameCtx.currentCbvSrvUavPageIndex = currentPageIdx;
    frameCtx.nextCbvSrvUavDescriptor = 0;  // Reset offset for new page

    IGL_D3D12_LOG_VERBOSE("D3D12: Switching to pre-allocated CBV/SRV/UAV page %u for frame %u\n",
                 currentPageIdx, frameIdx);

    // Update active heap when switching pages.
    frameCtx.activeCbvSrvUavHeap = pages[currentPageIdx].heap;

    // Rebind heap on the command list when switching pages.
    if (commandList_.Get()) {
      ID3D12DescriptorHeap* heaps[] = {
        frameCtx.activeCbvSrvUavHeap.Get(),
        frameCtx.samplerHeap.Get()
      };
      commandList_->SetDescriptorHeaps(2, heaps);
      IGL_D3D12_LOG_VERBOSE("D3D12: Rebound descriptor heaps after switching to page %u\n", currentPageIdx);
    }
  }

  // SAFE: Acquire reference AFTER any potential reallocation from emplace_back
  auto& currentPage = pages[currentPageIdx];

  // Allocate from current page
  const uint32_t descriptorIndex = frameCtx.nextCbvSrvUavDescriptor++;
  currentPage.used = frameCtx.nextCbvSrvUavDescriptor;

  // Track peak usage for telemetry
  const uint32_t totalUsed = static_cast<uint32_t>(currentPageIdx * kDescriptorsPerPage + descriptorIndex);
  if (totalUsed > frameCtx.peakCbvSrvUavUsage) {
    frameCtx.peakCbvSrvUavUsage = totalUsed;
  }

  // Return the descriptor index within the current page
  *outDescriptorIndex = descriptorIndex;

#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandBuffer::getNextCbvSrvUavDescriptor() - frame %u, page %u, descriptor %u (total allocated: %u)\n",
               frameIdx, currentPageIdx, descriptorIndex, totalUsed);
#endif

  return Result{};
}

// Allocate a contiguous range of CBV/SRV/UAV descriptors from pre-allocated pages.
// Ensures the range can be bound as a single descriptor table.
// Fails immediately if all pages are exhausted (Vulkan fail-fast pattern).
Result CommandBuffer::allocateCbvSrvUavRange(uint32_t count, uint32_t* outBaseDescriptorIndex) {
  if (count == 0) {
    return Result{Result::Code::ArgumentInvalid, "Cannot allocate zero descriptors"};
  }

  auto& ctx = device_.getD3D12Context();
  const uint32_t frameIdx = ctx.getCurrentFrameIndex();
  auto& frameCtx = ctx.getFrameContexts()[frameIdx];
  auto& pages = frameCtx.cbvSrvUavHeapPages;
  uint32_t currentPageIdx = frameCtx.currentCbvSrvUavPageIndex;

  if (pages.empty()) {
    return Result{Result::Code::RuntimeError, "No CBV/SRV/UAV descriptor heap pages available"};
  }

  if (currentPageIdx >= pages.size()) {
    return Result{Result::Code::RuntimeError, "Invalid descriptor heap page index"};
  }

  // Check space before acquiring reference (avoid use-after-reallocation)
  const uint32_t currentOffset = frameCtx.nextCbvSrvUavDescriptor;
  const uint32_t spaceRemaining = pages[currentPageIdx].capacity - currentOffset;

  // Check if the requested range fits in the current page; fail fast on exhaustion.
  if (count > spaceRemaining) {
    // Not enough space in current page - validate range and check for next page
    if (count > kDescriptorsPerPage) {
      char errorMsg[256];
      snprintf(errorMsg, sizeof(errorMsg),
               "Requested descriptor range (%u) exceeds page capacity (%u)",
               count, kDescriptorsPerPage);
      return Result{Result::Code::ArgumentOutOfRange, errorMsg};
    }

    // Move to next pre-allocated page (fail-fast if exhausted).
    const uint32_t nextPageIdx = currentPageIdx + 1;
    if (nextPageIdx >= pages.size()) {
      char errorMsg[512];
      snprintf(errorMsg, sizeof(errorMsg),
               "CBV/SRV/UAV descriptor heap exhausted! Frame %u needs page %u for contiguous range of %u descriptors, "
               "but only %zu pages are pre-allocated. "
               "Increase D3D12ContextConfig::maxHeapPages or optimize descriptor usage.",
               frameIdx, nextPageIdx, count, pages.size());
      return Result{Result::Code::RuntimeError, errorMsg};
    }

    // Move to next pre-allocated page
    currentPageIdx = nextPageIdx;
    frameCtx.currentCbvSrvUavPageIndex = currentPageIdx;
    frameCtx.nextCbvSrvUavDescriptor = 0;

    IGL_D3D12_LOG_VERBOSE("D3D12: Switching to pre-allocated CBV/SRV/UAV page %u for contiguous range of %u descriptors\n",
                 currentPageIdx, count);

    // Rebind heap on command list when switching pages
    frameCtx.activeCbvSrvUavHeap = pages[currentPageIdx].heap;
    if (commandList_.Get()) {
      ID3D12DescriptorHeap* heaps[] = {
        frameCtx.activeCbvSrvUavHeap.Get(),
        frameCtx.samplerHeap.Get()
      };
      commandList_->SetDescriptorHeaps(2, heaps);
    }
  }

  // SAFE: Acquire reference AFTER any potential reallocation from emplace_back
  auto& currentPage = pages[currentPageIdx];

  // Allocate the range from current page
  const uint32_t baseIndex = frameCtx.nextCbvSrvUavDescriptor;
  frameCtx.nextCbvSrvUavDescriptor += count;
  currentPage.used = frameCtx.nextCbvSrvUavDescriptor;

  // Track peak usage
  const uint32_t totalUsed = static_cast<uint32_t>(currentPageIdx * kDescriptorsPerPage + baseIndex + count);
  if (totalUsed > frameCtx.peakCbvSrvUavUsage) {
    frameCtx.peakCbvSrvUavUsage = totalUsed;
  }

  *outBaseDescriptorIndex = baseIndex;

#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandBuffer::allocateCbvSrvUavRange() - frame %u, page %u, base %u, count %u\n",
               frameIdx, currentPageIdx, baseIndex, count);
#endif

  return Result{};
}

uint32_t& CommandBuffer::getNextSamplerDescriptor() {
  auto& ctx = device_.getD3D12Context();
  const uint32_t frameIdx = ctx.getCurrentFrameIndex();
  auto& frameCtx = ctx.getFrameContexts()[frameIdx];

  // Add bounds checking to prevent sampler descriptor heap overflow.
  // The sampler heap is allocated with kSamplerHeapSize descriptors.
  const uint32_t currentValue = frameCtx.nextSamplerDescriptor;

  // Track peak usage for telemetry (before incrementing)
  if (currentValue > frameCtx.peakSamplerUsage) {
    frameCtx.peakSamplerUsage = currentValue;

    // Warn if approaching capacity (>80%)
    const float usage = static_cast<float>(currentValue) / static_cast<float>(kSamplerHeapSize);
    if (usage > 0.8f) {
      IGL_LOG_ERROR("D3D12: Sampler descriptor usage at %.1f%% capacity (%u/%u) for frame %u\n",
                    usage * 100.0f, currentValue, kSamplerHeapSize, frameIdx);
    }
  }

  // CRITICAL: Assert on overflow in debug builds
  IGL_DEBUG_ASSERT(currentValue < kSamplerHeapSize,
                   "D3D12: Sampler descriptor heap overflow! Allocated: %u, Capacity: %u (frame %u). "
                   "This will cause memory corruption and device removal. Increase heap size or optimize descriptor usage.",
                   currentValue, kSamplerHeapSize, frameIdx);

  // Graceful degradation in release builds: clamp to last valid descriptor
  if (currentValue >= kSamplerHeapSize) {
    IGL_LOG_ERROR("D3D12: Sampler descriptor heap overflow! Allocated: %u, Capacity: %u (frame %u)\n"
                  "Clamping to last valid descriptor. Rendering artifacts expected.\n",
                  currentValue, kSamplerHeapSize, frameIdx);
    // Return reference to a clamped value to prevent further damage
    // This will cause rendering artifacts but prevent crashes
    static uint32_t clampedValue = kSamplerHeapSize - 1;
    return clampedValue;
  }

#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandBuffer::getNextSamplerDescriptor() - frame %u, current value=%u\n",
               frameIdx, currentValue);
#endif
  return frameCtx.nextSamplerDescriptor;
}

void CommandBuffer::trackTransientBuffer(std::shared_ptr<IBuffer> buffer) {
  // Add to the CURRENT frame's transient buffer list
  // These will be kept alive until the frame completes GPU execution
  auto& ctx = device_.getD3D12Context();
  const uint32_t frameIdx = ctx.getCurrentFrameIndex();
  auto& frameCtx = ctx.getFrameContexts()[frameIdx];

  frameCtx.transientBuffers.push_back(std::move(buffer));

  // Track high-water mark for telemetry.
  const size_t currentCount = frameCtx.transientBuffers.size();
  if (currentCount > frameCtx.transientBuffersHighWater) {
    frameCtx.transientBuffersHighWater = currentCount;
  }

#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandBuffer::trackTransientBuffer() - Added buffer to frame %u (total=%zu, high-water=%zu)\n",
               frameIdx, currentCount, frameCtx.transientBuffersHighWater);
#endif
}

void CommandBuffer::trackTransientResource(ID3D12Resource* resource) {
  if (!resource) {
    return;
  }
  auto& ctx = device_.getD3D12Context();
  const uint32_t frameIdx = ctx.getCurrentFrameIndex();
  auto& frameCtx = ctx.getFrameContexts()[frameIdx];

  igl::d3d12::ComPtr<ID3D12Resource> keepAlive;
  resource->AddRef();
  keepAlive.Attach(resource);
  frameCtx.transientResources.push_back(std::move(keepAlive));

  // Track high-water mark for telemetry.
  const size_t currentCount = frameCtx.transientResources.size();
  if (currentCount > frameCtx.transientResourcesHighWater) {
    frameCtx.transientResourcesHighWater = currentCount;
  }

#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandBuffer::trackTransientResource() - Added resource to frame %u (total=%zu, high-water=%zu)\n",
               frameIdx, currentCount, frameCtx.transientResourcesHighWater);
#endif
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

  // CRITICAL: Set the per-frame descriptor heaps before recording commands.
  // Each frame has its own isolated heaps to prevent descriptor conflicts.
  // Uses the current page's heap (will be updated if we grow to new pages).
  auto& ctx = device_.getD3D12Context();
  const uint32_t frameIdx = ctx.getCurrentFrameIndex();
  auto& frameCtx = ctx.getFrameContexts()[frameIdx];

  // Initialize active heap to current page at frame start.
  if (frameCtx.cbvSrvUavHeapPages.empty()) {
    IGL_LOG_ERROR("CommandBuffer::begin() - No CBV/SRV/UAV heap pages available for frame %u\n", frameIdx);
    return;
  }

  frameCtx.activeCbvSrvUavHeap = frameCtx.cbvSrvUavHeapPages[frameCtx.currentCbvSrvUavPageIndex].heap;

  if (!frameCtx.activeCbvSrvUavHeap.Get()) {
    IGL_LOG_ERROR("CommandBuffer::begin() - No CBV/SRV/UAV heap available for frame %u\n", frameIdx);
    return;
  }

  // Use the CURRENT FRAME's command allocator from FrameContext
  // Following Microsoft's D3D12HelloFrameBuffering pattern
  auto* frameAllocator = ctx.getFrameContexts()[frameIdx].allocator.Get();

  // Microsoft pattern: Reset allocator THEN reset command list
  // Allocator was reset in CommandQueue::submit() after fence wait, OR is in initial ready state
#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandBuffer::begin() - Frame %u: Resetting command list with allocator...\n", frameIdx);
#endif
  HRESULT hr = commandList_->Reset(frameAllocator, nullptr);
  if (FAILED(hr)) {
    IGL_LOG_ERROR("CommandBuffer::begin() - Reset command list FAILED: 0x%08X\n", static_cast<unsigned>(hr));
    return;
  }
#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandBuffer::begin() - Command list reset OK\n");
#endif
  recording_ = true;

  // Bind heaps using active heap, not legacy accessor, now that the command list
  // has been reset and is in the recording state.
  ID3D12DescriptorHeap* heaps[] = {
      frameCtx.activeCbvSrvUavHeap.Get(),
      frameCtx.samplerHeap.Get()
  };
  commandList_->SetDescriptorHeaps(2, heaps);

#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandBuffer::begin() - Set per-frame descriptor heaps for frame %u\n", frameIdx);
#endif

  // Record timer start timestamp after reset and before any GPU work is recorded.
  // This ensures the timer measures the actual command buffer workload.
  if (desc.timer) {
    auto* timer = static_cast<Timer*>(desc.timer.get());
    timer->begin(commandList_.Get());
  }
}

void CommandBuffer::end() {
  if (!recording_) {
    return;
  }

  // No timer recording here; timer->begin() was called in begin(),
  // and timer->end() will be called in CommandQueue::submit() before close.

  // Close the command list - all recording is complete
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

  // Create encoder with lightweight constructor, then initialize with render pass
  // NOTE: begin() may encounter D3D12 errors (descriptor allocation, resource transitions, etc.)
  // but currently only logs failures and does not propagate errors to outResult.
  // Callers should rely on D3D12 debug layer and logging for error detection.
  // TODO: Consider propagating errors from begin() to outResult for better error handling.
  auto encoder = std::make_unique<RenderCommandEncoder>(*this, framebuffer);
  encoder->begin(renderPass);
  return encoder;
}

std::unique_ptr<IComputeCommandEncoder> CommandBuffer::createComputeCommandEncoder() {
  // Begin command buffer if not already begun
  begin();

  return std::make_unique<ComputeCommandEncoder>(*this);
}

void CommandBuffer::present(const std::shared_ptr<ITexture>& /*surface*/) const {
  // Note: Actual present happens in CommandQueue::submit(). This call serves
  // as a marker indicating that this command buffer should trigger a swapchain
  // Present when submitted.
  willPresent_ = true;
}

void CommandBuffer::waitUntilScheduled() {
  // If scheduleValue_ is 0, the command buffer hasn't been submitted yet
  if (scheduleValue_ == 0) {
#ifdef IGL_DEBUG
    IGL_D3D12_LOG_VERBOSE("CommandBuffer::waitUntilScheduled() - Not yet submitted, returning immediately\n");
#endif
    return;
  }

  // Check if the scheduling fence has already been signaled
  if (!scheduleFence_.Get()) {
    IGL_LOG_ERROR("CommandBuffer::waitUntilScheduled() - Scheduling fence is null\n");
    return;
  }

  const UINT64 completedValue = scheduleFence_->GetCompletedValue();
  if (completedValue >= scheduleValue_) {
#ifdef IGL_DEBUG
    IGL_D3D12_LOG_VERBOSE("CommandBuffer::waitUntilScheduled() - Already scheduled (completed=%llu, target=%llu)\n",
                 completedValue, scheduleValue_);
#endif
    return;
  }

  // Wait for the scheduling fence to be signaled
#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandBuffer::waitUntilScheduled() - Waiting for scheduling (completed=%llu, target=%llu)\n",
               completedValue, scheduleValue_);
#endif

  // Use FenceWaiter RAII wrapper for proper fence waiting with TOCTOU protection
  FenceWaiter waiter(scheduleFence_.Get(), scheduleValue_);
  Result waitResult = waiter.wait();
  if (!waitResult.isOk()) {
    IGL_LOG_ERROR("CommandBuffer::waitUntilScheduled() - Fence wait failed: %s\n",
                  waitResult.message.c_str());
    return;
  }

#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandBuffer::waitUntilScheduled() - Scheduling complete (fence now=%llu)\n",
               scheduleFence_->GetCompletedValue());
#endif
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
  igl::d3d12::ComPtr<ID3D12Fence> fence;
  auto* device = ctx.getDevice();
  if (!device || FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())))) {
    return;
  }

  queue->Signal(fence.Get(), 1);

  FenceWaiter waiter(fence.Get(), 1);
  Result waitResult = waiter.wait();
  if (!waitResult.isOk()) {
    IGL_LOG_ERROR("CommandBuffer::waitUntilCompleted() - Fence wait failed: %s\n",
                  waitResult.message.c_str());
    return;
  }

#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandBuffer::waitUntilCompleted() - GPU work completed\n");
#endif
}

void CommandBuffer::pushDebugGroupLabel(const char* label,
                                        const igl::Color& /*color*/) const {
  // Only emit GPU debug markers while the command list is in recording state.
  if (!recording_ || !commandList_.Get() || !label) {
    return;
  }

  const size_t len = strlen(label);
  std::wstring wlabel(len, L' ');
  std::mbstowcs(&wlabel[0], label, len);
  commandList_->BeginEvent(
      0, wlabel.c_str(), static_cast<UINT>((wlabel.length() + 1) * sizeof(wchar_t)));
}

void CommandBuffer::popDebugGroupLabel() const {
  // Only pop GPU debug markers while the command list is in recording state.
  if (!recording_ || !commandList_.Get()) {
    return;
  }

  commandList_->EndEvent();
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
    igl::d3d12::ComPtr<ID3D12Resource> readback;
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

    igl::d3d12::ComPtr<ID3D12CommandAllocator> allocator;
    igl::d3d12::ComPtr<ID3D12GraphicsCommandList> list;
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
  igl::d3d12::ComPtr<ID3D12CommandAllocator> allocator;
  igl::d3d12::ComPtr<ID3D12GraphicsCommandList> list;
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

// Public API: Record texture-to-buffer copy for deferred execution
void CommandBuffer::copyTextureToBuffer(ITexture& source,
                                       IBuffer& destination,
                                       uint64_t destinationOffset,
                                       uint32_t mipLevel,
                                       uint32_t layer) {
  // Like Vulkan, defer the copy operation until command buffer submission
  // D3D12 requires this to execute AFTER render commands complete, not during recording
  // (Unlike Vulkan which can record into the command buffer, D3D12 has closed command list and padding constraints)

  IGL_D3D12_LOG_VERBOSE("copyTextureToBuffer: Recording deferred copy operation (will execute in CommandQueue::submit)\n");

  deferredTextureCopies_.push_back({
    &source,
    &destination,
    destinationOffset,
    mipLevel,
    layer
  });
}

} // namespace igl::d3d12

