/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/CommandQueue.h>
#include <igl/d3d12/CommandBuffer.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/Timer.h>
#include <igl/d3d12/Texture.h>
#include <igl/d3d12/Buffer.h>
#include <igl/d3d12/TextureCopyUtils.h>
#include <igl/d3d12/D3D12FrameManager.h>
#include <igl/d3d12/D3D12PresentManager.h>
#include <igl/d3d12/D3D12Context.h>
#include <d3d12sdklayers.h>

namespace igl::d3d12 {

namespace {

/**
 * @brief Execute deferred texture-to-buffer copies after render commands
 */
void executeDeferredCopies(D3D12Context& ctx, Device& device, const CommandBuffer& cmdBuffer) {
  const auto& deferredCopies = cmdBuffer.getDeferredTextureCopies();
  if (deferredCopies.empty()) {
    return;
  }

#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandQueue: Executing %zu deferred copyTextureToBuffer operations\n",
               deferredCopies.size());
#endif

  // Wait for render commands to complete
  ctx.waitForGPU();

  for (const auto& copy : deferredCopies) {
    auto* srcTex = static_cast<Texture*>(copy.source);
    auto* dstBuf = static_cast<Buffer*>(copy.destination);

    Result copyResult = TextureCopyUtils::executeCopyTextureToBuffer(
        ctx, device, *srcTex, *dstBuf, copy.destinationOffset, copy.mipLevel, copy.layer);
    if (!copyResult.isOk()) {
      IGL_LOG_ERROR("Failed to copy texture to buffer: %s\n", copyResult.message.c_str());
    }
  }

#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandQueue: All deferred copies executed successfully\n");
#endif
}

// No standalone helper needed - this is now inlined in CommandQueue::submit()

/**
 * @brief Update per-frame fence tracking
 */
void updateFrameFences(D3D12Context& ctx, UINT64 currentFenceValue) {
  auto& frameCtx = ctx.getFrameContexts()[ctx.getCurrentFrameIndex()];

  // Update frame fence (backward compatibility)
  if (frameCtx.fenceValue == 0) {
    frameCtx.fenceValue = currentFenceValue;
  }

  // Update max allocator fence (critical for safe allocator reset)
  if (currentFenceValue > frameCtx.maxAllocatorFence) {
    frameCtx.maxAllocatorFence = currentFenceValue;
  }

  // Track command buffer count
  frameCtx.commandBufferCount++;

#ifdef IGL_DEBUG
  // Keep this at IGL_LOG_INFO because the test harness (test_all_sessions.bat) parses
  // "Signaled fence" from the INFO log; do not downgrade to VERBOSE
  IGL_LOG_INFO("CommandQueue: Signaled fence for frame %u "
               "(value=%llu, maxAllocatorFence=%llu, cmdBufCount=%u)\n",
               ctx.getCurrentFrameIndex(), currentFenceValue,
               frameCtx.maxAllocatorFence, frameCtx.commandBufferCount);
#endif
}

} // namespace

CommandQueue::CommandQueue(Device& device) : device_(device) {}

std::shared_ptr<ICommandBuffer> CommandQueue::createCommandBuffer(const CommandBufferDesc& desc,
                                                                   Result* IGL_NULLABLE outResult) {
  auto cmdBuffer = std::make_shared<CommandBuffer>(device_, desc);

  // Check if CommandBuffer was successfully initialized
  // CommandBuffer leaves commandList_ null on failure
  if (!cmdBuffer->getCommandList()) {
    Result::setResult(outResult, Result::Code::RuntimeError,
                     "Failed to create D3D12 command list. "
                     "Possible causes: device removed, out of memory, or device initialization failed. "
                     "Check debug output for HRESULT error code.");
    return nullptr;
  }

  Result::setOk(outResult);
  return cmdBuffer;
}

// T03/T09: Error handling behavior for submit()
// This function executes command lists and presents frames. Error handling:
// - Device removal: Detected via checkDeviceRemoval(), logs diagnostics, sets device.isDeviceLost()
//   flag, and triggers IGL_DEBUG_ASSERT. Returns SubmitHandle normally (legacy API limitation).
// - Present failures: Logged with IGL_LOG_ERROR via PresentManager. Device removal during Present
//   also triggers IGL_DEBUG_ASSERT. Non-removal failures (swapchain/window issues) are logged but
//   do not assert. Present result is checked but not propagated as Result (legacy API limitation).
// - Return value: The SubmitHandle is always returned regardless of errors and does NOT reflect
//   submission success/failure. Use device.checkDeviceRemoval() or device.isDeviceLost() as the
//   authoritative source for fatal error detection.
// Future: Consider Result-based submission API for explicit error propagation.
//
// T09: Refactored from 614 lines to <100 lines using helper classes:
// - FenceWaiter: RAII fence waiting with TOCTOU protection
// - FrameManager: Frame advancement and resource management
// - PresentManager: Swapchain presentation with device removal detection
SubmitHandle CommandQueue::submit(const ICommandBuffer& commandBuffer, bool /*endOfFrame*/) {
  auto& cmdBuffer = const_cast<CommandBuffer&>(static_cast<const CommandBuffer&>(commandBuffer));
  auto& ctx = device_.getD3D12Context();
  auto* commandList = cmdBuffer.getCommandList();
  auto* fence = ctx.getFence();

  // Defensive: Ensure we have a valid command list
  IGL_DEBUG_ASSERT(commandList, "D3D12 CommandQueue::submit() with null command list");

  // Record timer end timestamp before closing command list
  if (commandBuffer.desc.timer) {
    auto* timer = static_cast<Timer*>(commandBuffer.desc.timer.get());
    const UINT64 timerFenceValue = ctx.getFenceValue() + 1;
    timer->end(commandList, fence, timerFenceValue);
  }

  // Close command list
  cmdBuffer.end();

  // Signal scheduling fence
  ++scheduleFenceValue_;
  cmdBuffer.scheduleValue_ = scheduleFenceValue_;
  ctx.getCommandQueue()->Signal(cmdBuffer.scheduleFence_.Get(), scheduleFenceValue_);
#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandQueue: Signaled scheduling fence (value=%llu)\n", scheduleFenceValue_);
#endif

#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandQueue::submit() - Executing command list...\n");
#endif

  // Execute command list
  ID3D12CommandList* commandLists[] = {commandList};
  ctx.getCommandQueue()->ExecuteCommandLists(1, commandLists);

  // Execute deferred texture-to-buffer copies
  executeDeferredCopies(ctx, device_, cmdBuffer);

  // Check device status
  Result deviceCheck = device_.checkDeviceRemoval();
  if (!deviceCheck.isOk()) {
    IGL_LOG_ERROR("CommandQueue::submit() - Device removal detected: %s\n",
                  deviceCheck.message.c_str());
  }

  // Present if we have a swapchain
  bool presentOk = true;
  if (ctx.getSwapChain()) {
    PresentManager presentMgr(ctx);
    presentOk = presentMgr.present();
    if (!presentOk) {
      IGL_LOG_ERROR("CommandQueue::submit() - Present failed; frame advancement may be unsafe\n");
      // Note: Continue with fence signaling for now to maintain legacy behavior,
      // but future work should consider early-return or recovery strategy
    }
  }

  // Signal fence for current frame
  const UINT64 currentFenceValue = ++ctx.getFenceValue();
  ctx.getCommandQueue()->Signal(ctx.getFence(), currentFenceValue);

  // Update frame fence tracking
  updateFrameFences(ctx, currentFenceValue);

  // Advance to next frame with proper synchronization
  if (ctx.getSwapChain()) {
    FrameManager frameMgr(ctx);
    frameMgr.advanceFrame(currentFenceValue);
  }

#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandQueue::submit() - Complete!\n");
#endif

  // Aggregate per-command-buffer draw count into the device, matching GL/Vulkan behavior
  const auto cbDraws = cmdBuffer.getCurrentDrawCount();
#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandQueue::submit() - Aggregating %zu draws from CB into device\n", cbDraws);
#endif
  device_.incrementDrawCount(cbDraws);
#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("CommandQueue::submit() - Device drawCount now=%zu\n", device_.getCurrentDrawCount());

  // Log resource stats every 30 draws to track leaks
  const size_t drawCount = device_.getCurrentDrawCount();
  if (drawCount == 30 || drawCount == 60 || drawCount == 90 || drawCount == 120 ||
      drawCount == 150 || drawCount == 300 || drawCount == 600 || drawCount == 900 ||
      drawCount == 1200 || drawCount == 1500 || drawCount == 1800) {
    IGL_D3D12_LOG_VERBOSE("CommandQueue::submit() - Logging resource stats at drawCount=%zu\n", drawCount);
    D3D12Context::logResourceStats();
  }
#endif

  return 0;
}

} // namespace igl::d3d12
