/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/D3D12FrameManager.h>
#include <igl/d3d12/D3D12Context.h>
#include <igl/d3d12/D3D12FenceWaiter.h>

namespace igl::d3d12 {

void FrameManager::advanceFrame(UINT64 currentFenceValue) {
  // Calculate next frame index
  const uint32_t nextFrameIndex = (context_.getCurrentFrameIndex() + 1) % kMaxFramesInFlight;

  // STEP 1: Pipeline overload protection
  waitForPipelineSync(currentFenceValue);

  // STEP 2: Wait for next frame's resources to be available
  if (!waitForFrame(nextFrameIndex)) {
    IGL_LOG_ERROR("FrameManager: Skipping frame advancement due to fence wait failure\n");
    return;
  }

  // STEP 3: Advance to next frame
  context_.getCurrentFrameIndex() = nextFrameIndex;
#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("FrameManager: Advanced to frame index %u\n", nextFrameIndex);
#endif

  // STEP 4: Reset allocator safely
  resetAllocator(nextFrameIndex);

  // STEP 5: Clear transient resources
  clearTransientResources(nextFrameIndex);

  // STEP 6: Reset descriptor counters
  resetDescriptorCounters(nextFrameIndex);
}

void FrameManager::waitForPipelineSync(UINT64 currentFenceValue) {
  auto* fence = context_.getFence();

  // Ensure we don't have more than kMaxFramesInFlight frames in flight
  const UINT64 minimumSafeFence = (currentFenceValue >= kMaxFramesInFlight)
      ? (currentFenceValue - (kMaxFramesInFlight - 1))
      : 0;

  const UINT64 currentCompletedValue = fence->GetCompletedValue();
  if (currentCompletedValue < minimumSafeFence) {
#ifdef IGL_DEBUG
    IGL_D3D12_LOG_VERBOSE("FrameManager: SAFETY WAIT - Pipeline overload protection (completed=%llu, need=%llu)\n",
                 currentCompletedValue, minimumSafeFence);
#endif

    FenceWaiter waiter(fence, minimumSafeFence);
    Result waitResult = waiter.wait(INFINITE);
    if (!waitResult.isOk()) {
      IGL_LOG_ERROR("FrameManager: CRITICAL - Pipeline safety wait failed: %s; continuing but overload protection compromised\n",
                    waitResult.message.c_str());
      // Continue anyway - this is a safety net, not a hard requirement
      // But future work should consider aborting here as well
    }
#ifdef IGL_DEBUG
    else {
      IGL_D3D12_LOG_VERBOSE("FrameManager: Safety wait completed (fence now=%llu)\n",
                   fence->GetCompletedValue());
    }
#endif
  }
}

bool FrameManager::waitForFrame(uint32_t frameIndex) {
  auto* fence = context_.getFence();
  const UINT64 frameFence = context_.getFrameContexts()[frameIndex].fenceValue;

  if (frameFence != 0 && fence->GetCompletedValue() < frameFence) {
#ifdef IGL_DEBUG
    IGL_D3D12_LOG_VERBOSE("FrameManager: Waiting for frame %u (fence=%llu, current=%llu)\n",
                 frameIndex, frameFence, fence->GetCompletedValue());
#endif

    FenceWaiter waiter(fence, frameFence);

    // Try with 5-second timeout first (handles window drag scenarios)
    Result waitResult = waiter.wait(5000);
    if (!waitResult.isOk()) {
      // Check if it's a timeout or other error
      if (FenceWaiter::isTimeoutError(waitResult)) {
        IGL_LOG_ERROR("FrameManager: Wait for frame %u fence %llu timed out after 5s; forcing infinite wait\n",
                      frameIndex, frameFence);
      } else {
        IGL_LOG_ERROR("FrameManager: Wait for frame %u fence %llu failed: %s; forcing infinite wait\n",
                      frameIndex, frameFence, waitResult.message.c_str());
      }
      // Fall back to infinite wait
      waitResult = waiter.wait(INFINITE);
      if (!waitResult.isOk()) {
        IGL_LOG_ERROR("FrameManager: CRITICAL - Infinite wait for frame %u failed: %s; aborting frame advancement\n",
                      frameIndex, waitResult.message.c_str());
        return false; // Abort frame advancement - unsafe to proceed
      }
    }

#ifdef IGL_DEBUG
    IGL_D3D12_LOG_VERBOSE("FrameManager: Frame %u resources now available (completed=%llu)\n",
                 frameIndex, fence->GetCompletedValue());
#endif
  } else {
#ifdef IGL_DEBUG
    IGL_D3D12_LOG_VERBOSE("FrameManager: Frame %u resources already available (fence=%llu, completed=%llu)\n",
                 frameIndex, frameFence, fence->GetCompletedValue());
#endif
  }
  return true;
}

void FrameManager::resetAllocator(uint32_t frameIndex) {
  auto* fence = context_.getFence();
  auto& frame = context_.getFrameContexts()[frameIndex];
  auto* allocator = frame.allocator.Get();

  const UINT64 allocatorFence = frame.maxAllocatorFence;

  if (allocatorFence == 0) {
    // First time using this allocator
    HRESULT hr = allocator->Reset();
    if (FAILED(hr)) {
      IGL_LOG_ERROR("FrameManager: Failed to reset frame %u allocator: 0x%08X\n",
                    frameIndex, static_cast<unsigned>(hr));
    }
  } else {
    // Verify GPU completed all command lists using this allocator
    const UINT64 completedValue = fence->GetCompletedValue();

    if (completedValue < allocatorFence) {
      IGL_LOG_ERROR("FrameManager: ALLOCATOR SYNC ISSUE - GPU not done with all command lists "
                    "(completed=%llu, need=%llu, cmdBufCount=%u). Waiting...\n",
                    completedValue, allocatorFence, frame.commandBufferCount);

      FenceWaiter waiter(fence, allocatorFence);
      Result waitResult = waiter.wait(INFINITE);
      if (!waitResult.isOk()) {
        IGL_LOG_ERROR("FrameManager: CRITICAL - Allocator wait failed: %s; skipping unsafe allocator reset for frame %u\n",
                      waitResult.message.c_str(), frameIndex);
        // Do not reset allocator if GPU hasn't completed - would cause sync violations
        return;
      }
      IGL_D3D12_LOG_VERBOSE("FrameManager: Allocator wait completed (fence now=%llu)\n",
                   fence->GetCompletedValue());
    }

    // Reset allocator (safe now - GPU has completed all command lists)
    HRESULT hr = allocator->Reset();
    if (FAILED(hr)) {
      IGL_LOG_ERROR("FrameManager: Failed to reset frame %u allocator: 0x%08X "
                    "(maxFence=%llu, completed=%llu, cmdBufCount=%u)\n",
                    frameIndex, static_cast<unsigned>(hr),
                    allocatorFence, fence->GetCompletedValue(),
                    frame.commandBufferCount);
    } else {
#ifdef IGL_DEBUG
      IGL_D3D12_LOG_VERBOSE("FrameManager: Reset frame %u allocator (waited for %u command buffers, maxFence=%llu)\n",
                   frameIndex, frame.commandBufferCount, allocatorFence);
#endif
    }

#ifdef _DEBUG
    if (SUCCEEDED(hr)) {
      const UINT64 currentCompleted = fence->GetCompletedValue();
      IGL_DEBUG_ASSERT(currentCompleted >= allocatorFence,
                       "Allocator reset before GPU completed all command lists!");
    }
#endif
  }

  // Reset frame tracking
  frame.fenceValue = 0;
  frame.maxAllocatorFence = 0;
  frame.commandBufferCount = 0;
}

void FrameManager::clearTransientResources(uint32_t frameIndex) {
  auto& frame = context_.getFrameContexts()[frameIndex];

  if (!frame.transientBuffers.empty()) {
#ifdef IGL_DEBUG
    IGL_D3D12_LOG_VERBOSE("FrameManager: Clearing %zu transient buffers from frame %u (high-water=%zu)\n",
                 frame.transientBuffers.size(), frameIndex, frame.transientBuffersHighWater);
#endif
    frame.transientBuffers.clear();
  }

  if (!frame.transientResources.empty()) {
#ifdef IGL_DEBUG
    IGL_D3D12_LOG_VERBOSE("FrameManager: Releasing %zu transient D3D resources from frame %u (high-water=%zu)\n",
                 frame.transientResources.size(), frameIndex, frame.transientResourcesHighWater);
#endif
    frame.transientResources.clear();
  }
}

void FrameManager::resetDescriptorCounters(uint32_t frameIndex) {
  auto& frame = context_.getFrameContexts()[frameIndex];

  const uint32_t cbvSrvUavUsage = frame.nextCbvSrvUavDescriptor;
  const uint32_t samplerUsage = frame.nextSamplerDescriptor;
  const uint32_t peakCbvSrvUav = frame.peakCbvSrvUavUsage;
  const uint32_t peakSampler = frame.peakSamplerUsage;

  if (cbvSrvUavUsage > 0 || samplerUsage > 0) {
#ifdef IGL_DEBUG
    const float cbvSrvUavPercent = (static_cast<float>(cbvSrvUavUsage) / kCbvSrvUavHeapSize) * 100.0f;
    const float samplerPercent = (static_cast<float>(samplerUsage) / kSamplerHeapSize) * 100.0f;
    const float peakCbvSrvUavPercent = (static_cast<float>(peakCbvSrvUav) / kCbvSrvUavHeapSize) * 100.0f;
    const float peakSamplerPercent = (static_cast<float>(peakSampler) / kSamplerHeapSize) * 100.0f;

    IGL_D3D12_LOG_VERBOSE("FrameManager: Frame %u descriptor usage:\n"
                 "  CBV/SRV/UAV: final=%u/%u (%.1f%%), peak=%u/%u (%.1f%%)\n"
                 "  Samplers:    final=%u/%u (%.1f%%), peak=%u/%u (%.1f%%)\n",
                 frameIndex,
                 cbvSrvUavUsage, kCbvSrvUavHeapSize, cbvSrvUavPercent,
                 peakCbvSrvUav, kCbvSrvUavHeapSize, peakCbvSrvUavPercent,
                 samplerUsage, kSamplerHeapSize, samplerPercent,
                 peakSampler, kSamplerHeapSize, peakSamplerPercent);
#endif
  }

  // Reset counters
  frame.nextCbvSrvUavDescriptor = 0;
  frame.nextSamplerDescriptor = 0;

#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("FrameManager: Reset descriptor counters for frame %u to 0\n", frameIndex);
#endif
}

} // namespace igl::d3d12
