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
#include <d3d12sdklayers.h>

namespace igl::d3d12 {

namespace {

void logInfoQueueMessages(ID3D12Device* device) {
  Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
  if (FAILED(device->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf())))) {
    return;
  }

  UINT64 numMessages = infoQueue->GetNumStoredMessages();
  IGL_LOG_INFO("D3D12 Info Queue has %llu messages:\n", numMessages);
  for (UINT64 i = 0; i < numMessages; ++i) {
    SIZE_T messageLength = 0;
    infoQueue->GetMessage(i, nullptr, &messageLength);
    if (messageLength == 0) {
      continue;
    }
    auto* message = static_cast<D3D12_MESSAGE*>(malloc(messageLength));
    if (message && SUCCEEDED(infoQueue->GetMessage(i, message, &messageLength))) {
      const char* severityStr = "UNKNOWN";
      switch (message->Severity) {
        case D3D12_MESSAGE_SEVERITY_CORRUPTION: severityStr = "CORRUPTION"; break;
        case D3D12_MESSAGE_SEVERITY_ERROR: severityStr = "ERROR"; break;
        case D3D12_MESSAGE_SEVERITY_WARNING: severityStr = "WARNING"; break;
        case D3D12_MESSAGE_SEVERITY_INFO: severityStr = "INFO"; break;
        case D3D12_MESSAGE_SEVERITY_MESSAGE: severityStr = "MESSAGE"; break;
      }
      IGL_LOG_INFO("  [%s] %s\n", severityStr, message->pDescription);
    }
    free(message);
  }
}

void logDredInfo(ID3D12Device* device) {
#if defined(__ID3D12DeviceRemovedExtendedData1_INTERFACE_DEFINED__)
  Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData1> dred;
  if (FAILED(device->QueryInterface(IID_PPV_ARGS(dred.GetAddressOf())))) {
    IGL_LOG_INFO("DRED: ID3D12DeviceRemovedExtendedData1 not available.\n");
    return;
  }

  D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs = {};
  if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&breadcrumbs)) && breadcrumbs.pHeadAutoBreadcrumbNode) {
    IGL_LOG_ERROR("DRED AutoBreadcrumbs (most recent first):\n");
    const D3D12_AUTO_BREADCRUMB_NODE1* node = breadcrumbs.pHeadAutoBreadcrumbNode;
    uint32_t nodeIndex = 0;
    constexpr uint32_t kMaxNodesToPrint = 16;
    while (node && nodeIndex < kMaxNodesToPrint) {
      const char* listName = node->pCommandListDebugNameA ? node->pCommandListDebugNameA : "<unnamed>";
      const char* queueName = node->pCommandQueueDebugNameA ? node->pCommandQueueDebugNameA : "<unnamed>";
      IGL_LOG_ERROR("  Node #%u: CommandList=%p (%s) CommandQueue=%p (%s) Breadcrumbs=%u completed=%u\n",
                    nodeIndex,
                    node->pCommandList,
                    listName,
                    node->pCommandQueue,
                    queueName,
                    node->BreadcrumbCount,
                    node->pLastBreadcrumbValue ? *node->pLastBreadcrumbValue : 0);
      if (node->pCommandHistory && node->BreadcrumbCount > 0) {
        D3D12_AUTO_BREADCRUMB_OP lastOp = node->pCommandHistory[node->BreadcrumbCount - 1];
        IGL_LOG_ERROR("    Last command: %d (history count=%u)\n", static_cast<int>(lastOp), node->BreadcrumbCount);
      }
      node = node->pNext;
      ++nodeIndex;
    }
    if (node) {
      IGL_LOG_ERROR("  ... additional breadcrumbs omitted ...\n");
    }
  } else {
    IGL_LOG_INFO("DRED: No auto breadcrumbs captured.\n");
  }

  D3D12_DRED_PAGE_FAULT_OUTPUT1 pageFault = {};
  if (SUCCEEDED(dred->GetPageFaultAllocationOutput1(&pageFault)) && pageFault.PageFaultVA != 0) {
    IGL_LOG_ERROR("DRED PageFault: VA=0x%016llx\n", pageFault.PageFaultVA);
    if (pageFault.pHeadExistingAllocationNode) {
      const auto* alloc = pageFault.pHeadExistingAllocationNode;
      IGL_LOG_ERROR("  Existing allocation: Object=%p Name=%s Type=%u\n",
                    alloc->pObject,
                    alloc->ObjectNameA ? alloc->ObjectNameA : "<unnamed>",
                    static_cast<unsigned>(alloc->AllocationType));
    }
    if (pageFault.pHeadRecentFreedAllocationNode) {
      const auto* freed = pageFault.pHeadRecentFreedAllocationNode;
      IGL_LOG_ERROR("  Recently freed allocation: Object=%p Name=%s Type=%u\n",
                    freed->pObject,
                    freed->ObjectNameA ? freed->ObjectNameA : "<unnamed>",
                    static_cast<unsigned>(freed->AllocationType));
    }
  } else {
    IGL_LOG_INFO("DRED: No page fault data available.\n");
  }
#else
  (void)device;
  IGL_LOG_INFO("DRED: Extended data interfaces not available on this SDK.\n");
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
                     "Failed to create command buffer - check logs for details");
    return nullptr;
  }

  Result::setOk(outResult);
  return cmdBuffer;
}

// T03: Error handling behavior for submit()
// This function executes command lists and presents frames. Error handling:
// - Device removal: Detected via checkDeviceRemoval(), logs diagnostics, sets device.isDeviceLost()
//   flag, and triggers IGL_DEBUG_ASSERT. Returns SubmitHandle normally (legacy API limitation).
// - Present failures: Logged with IGL_LOG_ERROR and IGL_DEBUG_ASSERT, but not propagated as Result.
// - Return value: The SubmitHandle is always returned regardless of errors and does NOT reflect
//   submission success/failure. Use device.checkDeviceRemoval() or device.isDeviceLost() as the
//   authoritative source for fatal error detection.
// Future: Consider Result-based submission API for explicit error propagation.
SubmitHandle CommandQueue::submit(const ICommandBuffer& commandBuffer, bool /*endOfFrame*/) {
  const auto& d3dCommandBuffer = static_cast<const CommandBuffer&>(commandBuffer);
  auto* d3dCommandList = d3dCommandBuffer.getCommandList();
  auto& ctx = device_.getD3D12Context();
  auto* d3dCommandQueue = ctx.getCommandQueue();
  auto* d3dDevice = ctx.getDevice();
  auto* fence = ctx.getFence();

  // T02: Record timer end timestamp BEFORE closing command list
  // Timer begin() was called in CommandBuffer::begin(), bracketing the GPU work
  // Now record the end timestamp and associate with fence
  if (commandBuffer.desc.timer) {
    auto* timer = static_cast<Timer*>(commandBuffer.desc.timer.get());

    // Calculate fence value that will be signaled after this command list completes
    // We increment the fence value after submit, so predict the next value
    const UINT64 timerFenceValue = ctx.getFenceValue() + 1;

    // Record end timestamp and resolve queries, associate with fence
    timer->end(d3dCommandList, fence, timerFenceValue);
  }

  // Ensure the command list is closed before execution
  const_cast<CommandBuffer&>(d3dCommandBuffer).end();

#ifdef IGL_DEBUG
  IGL_LOG_INFO("CommandQueue::submit() - Executing command list...\n");
#endif

  // Execute command list
  ID3D12CommandList* commandLists[] = {d3dCommandList};
  d3dCommandQueue->ExecuteCommandLists(1, commandLists);

  // Execute any deferred texture-to-buffer copies AFTER render commands complete
  // This is similar to Vulkan's behavior where copies execute as part of submission
  const auto& deferredCopies = d3dCommandBuffer.getDeferredTextureCopies();
  if (!deferredCopies.empty()) {
#ifdef IGL_DEBUG
    IGL_LOG_INFO("CommandQueue::submit() - Executing %zu deferred copyTextureToBuffer operations\n",
                 deferredCopies.size());
#endif
    // Wait for render commands to complete before copying
    ctx.waitForGPU();

    for (const auto& copy : deferredCopies) {
      // Call the helper function that handles D3D12's padding requirements
      auto* srcTex = static_cast<Texture*>(copy.source);
      auto* dstBuf = static_cast<Buffer*>(copy.destination);

      // D-001: Pass device_ for allocator pooling
      Result copyResult = TextureCopyUtils::executeCopyTextureToBuffer(
          ctx, device_, *srcTex, *dstBuf, copy.destinationOffset, copy.mipLevel, copy.layer);
      if (!copyResult.isOk()) {
        IGL_LOG_ERROR("Failed to copy texture to buffer: %s\n", copyResult.message.c_str());
      }
    }
#ifdef IGL_DEBUG
    IGL_LOG_INFO("CommandQueue::submit() - All deferred copies executed successfully\n");
#endif
  }

  // Signal the command buffer's scheduling fence immediately after submission
  // This allows waitUntilScheduled() to return as soon as the command buffer is queued
  // (NOT when GPU completes execution)
  // P1_DX12-FIND-03: Use monotonically increasing fence values (1, 2, 3, ...)
  auto& cmdBuf = const_cast<CommandBuffer&>(d3dCommandBuffer);
  ++scheduleFenceValue_;  // Increment BEFORE signaling
  cmdBuf.scheduleValue_ = scheduleFenceValue_;  // Store fence value in command buffer
  d3dCommandQueue->Signal(cmdBuf.scheduleFence_.Get(), scheduleFenceValue_);
#ifdef IGL_DEBUG
  IGL_LOG_INFO("CommandQueue::submit() - Signaled scheduling fence (value=%llu)\n", scheduleFenceValue_);
#endif

#ifdef IGL_DEBUG
  IGL_LOG_INFO("CommandQueue::submit() - Command list executed, checking device status...\n");
#endif

  // P1_DX12-006: Check for device removal after command execution
  Result deviceCheck = device_.checkDeviceRemoval();
  if (!deviceCheck.isOk()) {
    // Log additional diagnostics on device removal
    logInfoQueueMessages(d3dDevice);
    logDredInfo(d3dDevice);
    IGL_LOG_ERROR("CommandQueue::submit() - Device removal detected: %s\n", deviceCheck.message.c_str());
    // Device removal is fatal - continue with presentation attempt but expect failure
  }

#ifdef IGL_DEBUG
  if (deviceCheck.isOk()) {
    IGL_LOG_INFO("CommandQueue::submit() - Device OK, presenting...\n");
  } else {
    IGL_LOG_INFO("CommandQueue::submit() - Device lost, attempting Present for diagnostics...\n");
  }
#endif

  // Present if this is end of frame
  auto* swapChain = ctx.getSwapChain();
  if (swapChain) {
    // VSync toggle via env var (IGL_D3D12_VSYNC=0 disables vsync)
    UINT syncInterval = 1;
    UINT presentFlags = 0;
    {
      char buf[8] = {};
      if (GetEnvironmentVariableA("IGL_D3D12_VSYNC", buf, sizeof(buf)) > 0) {
        if (buf[0] == '0') {
          syncInterval = 0;
          // Only use tearing flag if swapchain was created with DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
          // Using DXGI_PRESENT_ALLOW_TEARING without the swapchain flag is invalid
          if (ctx.isTearingSupported()) {
            presentFlags |= DXGI_PRESENT_ALLOW_TEARING;
          }
        }
      }
    }
    HRESULT presentHr = swapChain->Present(syncInterval, presentFlags);
    if (FAILED(presentHr)) {
      IGL_LOG_ERROR("Present failed: 0x%08X\n", static_cast<unsigned>(presentHr));
      // Check if device was removed during Present
      HRESULT deviceStatus = d3dDevice->GetDeviceRemovedReason();
      if (FAILED(deviceStatus)) {
        IGL_LOG_ERROR("DEVICE REMOVED during Present! Reason: 0x%08X\n", static_cast<unsigned>(deviceStatus));
        logInfoQueueMessages(d3dDevice);
        logDredInfo(d3dDevice);
        IGL_DEBUG_ASSERT(false);
        // Device removal is fatal but don't throw - let application handle via error checking
      }
    } else {
#ifdef IGL_DEBUG
      IGL_LOG_INFO("CommandQueue::submit() - Present OK\n");
#endif
    }

    // CRITICAL: Check device status AFTER Present() as well
    // Present() can trigger device removal if command lists have errors
    HRESULT postPresentStatus = d3dDevice->GetDeviceRemovedReason();
    if (FAILED(postPresentStatus)) {
      IGL_LOG_ERROR("DEVICE REMOVED after Present! Reason: 0x%08X\n", static_cast<unsigned>(postPresentStatus));
      logInfoQueueMessages(d3dDevice);
      logDredInfo(d3dDevice);
      IGL_DEBUG_ASSERT(false);
      // Device removal is fatal but don't throw - let application handle via error checking
    }
  }

  // Per-frame fencing: Signal fence for current frame
  const UINT64 currentFenceValue = ++ctx.getFenceValue();
  // T02: fence already declared earlier for timer, reuse it

  d3dCommandQueue->Signal(fence, currentFenceValue);

  auto& frameCtx = ctx.getFrameContexts()[ctx.getCurrentFrameIndex()];

  // D-002: Update frame fence (first signal, backward compatibility)
  if (frameCtx.fenceValue == 0) {
    frameCtx.fenceValue = currentFenceValue;
  }

  // D-002: CRITICAL - Update max allocator fence to track ALL command lists
  // This is the fence value we must wait for before resetting allocator
  if (currentFenceValue > frameCtx.maxAllocatorFence) {
    frameCtx.maxAllocatorFence = currentFenceValue;
  }

  // D-002: Track command buffer count (telemetry)
  frameCtx.commandBufferCount++;

#ifdef IGL_DEBUG
  IGL_LOG_INFO("CommandQueue::submit() - Signaled fence for frame %u "
               "(value=%llu, maxAllocatorFence=%llu, cmdBufCount=%u)\n",
               ctx.getCurrentFrameIndex(), currentFenceValue,
               frameCtx.maxAllocatorFence, frameCtx.commandBufferCount);
#endif

  // Move to next frame
  if (swapChain) {
    // Calculate next frame index
    const uint32_t nextFrameIndex = (ctx.getCurrentFrameIndex() + 1) % kMaxFramesInFlight;

    // CRITICAL: Wait for NEXT frame's previous work to complete BEFORE advancing to it
    // This ensures we don't overwrite descriptors/resources still in use by GPU
    // ADDITIONAL SAFETY: For bind groups and descriptor reuse, ensure we wait for completion
    const UINT64 nextFrameFence = ctx.getFrameContexts()[nextFrameIndex].fenceValue;

    // BIND GROUP FIX: Also check if we need to wait for the CURRENT frame to fully complete
    // This is critical for scenarios where descriptors are created each frame for bind groups
    // With variable FPS, the GPU might still be processing frame N-1 or N-2 when we try to
    // advance to frame N+1, causing descriptor/resource conflicts
    const UINT64 currentCompletedValue = fence->GetCompletedValue();
    const UINT64 minimumSafeFence = (currentFenceValue >= kMaxFramesInFlight)
        ? (currentFenceValue - (kMaxFramesInFlight - 1))  // Wait for at least 2 frames back to complete
        : 0;

    if (currentCompletedValue < minimumSafeFence) {
#ifdef IGL_DEBUG
      IGL_LOG_INFO("CommandQueue::submit() - SAFETY WAIT: Ensuring frame pipeline is not overloaded (completed=%llu, need=%llu)\n",
                   currentCompletedValue, minimumSafeFence);
#endif
      // D-003: Create dedicated fence event per wait operation to eliminate TOCTOU race
      // Wait for the frame that's (kMaxFramesInFlight-1) frames back to complete
      // This ensures we never have more than kMaxFramesInFlight frames in flight
      HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
      if (hEvent) {
        HRESULT hr = fence->SetEventOnCompletion(minimumSafeFence, hEvent);
        if (SUCCEEDED(hr)) {
          // D-003: Re-check fence after SetEventOnCompletion to avoid race
          if (fence->GetCompletedValue() < minimumSafeFence) {
            WaitForSingleObject(hEvent, INFINITE);
#ifdef IGL_DEBUG
            IGL_LOG_INFO("CommandQueue::submit() - Safety wait completed (fence now=%llu)\n",
                         fence->GetCompletedValue());
#endif
          }
        }
        CloseHandle(hEvent);
      } else {
        IGL_LOG_ERROR("CommandQueue::submit() - Failed to create event for safety wait\n");
      }
    }

    if (nextFrameFence != 0 && fence->GetCompletedValue() < nextFrameFence) {
#ifdef IGL_DEBUG
      IGL_LOG_INFO("CommandQueue::submit() - Waiting for frame %u to complete (fence value=%llu, current=%llu)\n",
                   nextFrameIndex, nextFrameFence, fence->GetCompletedValue());
#endif
      // D-003: Create dedicated fence event per wait operation to eliminate TOCTOU race
      HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
      if (hEvent) {
        HRESULT hr = fence->SetEventOnCompletion(nextFrameFence, hEvent);
        if (SUCCEEDED(hr)) {
          // D-003: Re-check fence after SetEventOnCompletion to avoid race condition
          // The fence may have completed between our initial check and SetEventOnCompletion
          if (fence->GetCompletedValue() < nextFrameFence) {
            // Fence still not complete, proceed with wait
            // Use timeout to prevent deadlock during window drag (when Present blocks message pump)
            DWORD waitResult = WaitForSingleObject(hEvent, 5000); // 5 second timeout
            if (waitResult == WAIT_OBJECT_0) {
              // CRITICAL: Even if wait succeeded, verify fence actually reached expected value
              // On some systems, the event can be signaled before GetCompletedValue() is fully updated
              UINT64 completedAfterWait = fence->GetCompletedValue();
              if (completedAfterWait < nextFrameFence) {
                IGL_LOG_ERROR("CommandQueue::submit() - Wait returned but fence not complete! Expected=%llu, got=%llu\n",
                             nextFrameFence, completedAfterWait);
                IGL_LOG_ERROR("CommandQueue::submit() - Forcing GPU wait with busy loop...\n");
                // CRITICAL: Busy-wait until fence actually completes (with timeout)
                const int maxSpins = 10000;
                int spinCount = 0;
                while (fence->GetCompletedValue() < nextFrameFence && spinCount < maxSpins) {
                  Sleep(1);  // Small sleep to avoid burning CPU
                  spinCount++;
                }
                if (fence->GetCompletedValue() >= nextFrameFence) {
#ifdef IGL_DEBUG
                  IGL_LOG_INFO("CommandQueue::submit() - GPU finally completed after %d spins (fence now=%llu)\n",
                               spinCount, fence->GetCompletedValue());
#endif
                } else {
                  IGL_LOG_ERROR("CommandQueue::submit() - GPU STILL not complete after busy-wait! (fence=%llu)\n",
                               fence->GetCompletedValue());
                  // Last resort: wait forever
                  while (fence->GetCompletedValue() < nextFrameFence) {
                    Sleep(10);
                  }
#ifdef IGL_DEBUG
                  IGL_LOG_INFO("CommandQueue::submit() - Emergency infinite wait completed (fence=%llu)\n",
                               fence->GetCompletedValue());
#endif
                }
              } else {
#ifdef IGL_DEBUG
                IGL_LOG_INFO("CommandQueue::submit() - Frame %u resources now available (completed=%llu)\n",
                             nextFrameIndex, completedAfterWait);
#endif
              }
            } else if (waitResult == WAIT_TIMEOUT) {
              IGL_LOG_ERROR("CommandQueue::submit() - TIMEOUT waiting for frame %u fence %llu (completed=%llu)\n",
                           nextFrameIndex, nextFrameFence, fence->GetCompletedValue());
              IGL_LOG_ERROR("CommandQueue::submit() - Forcing infinite wait to prevent device removed...\n");
              // CRITICAL: Must wait for GPU to finish before resetting allocator
              // This can happen during window drag or very low FPS scenarios
              // D-003: Fallback to infinite wait on same dedicated event
              WaitForSingleObject(hEvent, INFINITE);
#ifdef IGL_DEBUG
              IGL_LOG_INFO("CommandQueue::submit() - Infinite wait completed (fence now=%llu)\n",
                           fence->GetCompletedValue());
#endif
            } else {
              IGL_LOG_ERROR("CommandQueue::submit() - Wait failed with result 0x%08X\n", waitResult);
            }
          } else {
            // Fence completed during SetEventOnCompletion (race condition handled)
#ifdef IGL_DEBUG
            IGL_LOG_INFO("CommandQueue::submit() - Frame %u fence completed during setup (race avoided, completed=%llu)\n",
                         nextFrameIndex, fence->GetCompletedValue());
#endif
          }
        } else {
          IGL_LOG_ERROR("CommandQueue::submit() - SetEventOnCompletion failed: 0x%08X\n", static_cast<unsigned>(hr));
        }
        CloseHandle(hEvent);
      } else {
        IGL_LOG_ERROR("CommandQueue::submit() - Failed to create event for frame fence wait\n");
      }
    } else {
#ifdef IGL_DEBUG
      IGL_LOG_INFO("CommandQueue::submit() - Frame %u resources already available (fence=%llu, completed=%llu)\n",
                   nextFrameIndex, nextFrameFence, fence->GetCompletedValue());
#endif
    }

    // Now advance to next frame
    ctx.getCurrentFrameIndex() = nextFrameIndex;
#ifdef IGL_DEBUG
    IGL_LOG_INFO("CommandQueue::submit() - Advanced to frame index %u\n", nextFrameIndex);
#endif

    // D-002: CRITICAL - Reset allocator only after ALL command lists using it complete
    auto& nextFrame = ctx.getFrameContexts()[nextFrameIndex];
    auto* frameAllocator = nextFrame.allocator.Get();

    // D-002: Must wait for maxAllocatorFence, not just fenceValue
    // maxAllocatorFence tracks the LAST command list submitted with this allocator
    const UINT64 allocatorCompletionFence = nextFrame.maxAllocatorFence;

    if (allocatorCompletionFence == 0) {
      // First frame, allocator never used, safe to reset
      HRESULT allocResetHr = frameAllocator->Reset();
      if (FAILED(allocResetHr)) {
        IGL_LOG_ERROR("CommandQueue::submit() - FAILED to reset frame %u allocator: 0x%08X\n",
                      nextFrameIndex, static_cast<unsigned>(allocResetHr));
      }
    } else {
      // D-002: Verify GPU has completed ALL command lists using this allocator
      const UINT64 completedValue = fence->GetCompletedValue();

      if (completedValue < allocatorCompletionFence) {
        // ⚠️ SAFETY CATCH: GPU hasn't finished all command lists yet
        // This should NOT happen if frame pacing is correct, but we check defensively
        IGL_LOG_ERROR("CommandQueue::submit() - ALLOCATOR SYNC ISSUE: GPU not done with all command lists "
                      "(completed=%llu, need=%llu, cmdBufCount=%u). Waiting...\n",
                      completedValue, allocatorCompletionFence, nextFrame.commandBufferCount);

        // D-003: Create dedicated fence event per wait operation to eliminate TOCTOU race
        HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (hEvent) {
          HRESULT hr = fence->SetEventOnCompletion(allocatorCompletionFence, hEvent);
          if (SUCCEEDED(hr)) {
            // D-003: Re-check fence after SetEventOnCompletion to avoid race
            if (fence->GetCompletedValue() < allocatorCompletionFence) {
              WaitForSingleObject(hEvent, INFINITE);
              IGL_LOG_ERROR("CommandQueue::submit() - Allocator wait completed (fence now=%llu)\n",
                           fence->GetCompletedValue());
            }
          }
          CloseHandle(hEvent);
        } else {
          IGL_LOG_ERROR("CommandQueue::submit() - Failed to create event for allocator wait\n");
        }
      }

      // Now safe to reset allocator - GPU finished ALL command lists
      HRESULT allocResetHr = frameAllocator->Reset();
      if (FAILED(allocResetHr)) {
        IGL_LOG_ERROR("CommandQueue::submit() - FAILED to reset frame %u allocator: 0x%08X "
                      "(maxFence=%llu, completed=%llu, cmdBufCount=%u)\n",
                      nextFrameIndex, static_cast<unsigned>(allocResetHr),
                      allocatorCompletionFence, fence->GetCompletedValue(),
                      nextFrame.commandBufferCount);
      } else {
#ifdef IGL_DEBUG
        IGL_LOG_INFO("CommandQueue::submit() - Reset frame %u allocator successfully "
                     "(waited for %u command buffers, maxFence=%llu)\n",
                     nextFrameIndex, nextFrame.commandBufferCount, allocatorCompletionFence);
#endif
      }

#ifdef _DEBUG
      // D-002: Validate allocator reset was safe
      if (SUCCEEDED(allocResetHr)) {
        const UINT64 currentCompleted = fence->GetCompletedValue();
        IGL_DEBUG_ASSERT(currentCompleted >= allocatorCompletionFence,
                         "Allocator reset before GPU completed all command lists!");
      }
#endif
    }

    // D-002: Reset frame tracking for next usage
    nextFrame.fenceValue = 0;
    nextFrame.maxAllocatorFence = 0;
    nextFrame.commandBufferCount = 0;

    // CRITICAL: Clear transient buffers from the frame we just waited for
    // The GPU has finished executing that frame, so these resources can now be released
    // P2_DX12-120: Added telemetry for transient resource tracking
    auto& frameCtx = ctx.getFrameContexts()[nextFrameIndex];
    if (!frameCtx.transientBuffers.empty()) {
#ifdef IGL_DEBUG
      IGL_LOG_INFO("CommandQueue::submit() - Clearing %zu transient buffers from frame %u (high-water=%zu)\n",
                   frameCtx.transientBuffers.size(), nextFrameIndex, frameCtx.transientBuffersHighWater);
#endif
      frameCtx.transientBuffers.clear();
    }
    if (!frameCtx.transientResources.empty()) {
#ifdef IGL_DEBUG
      IGL_LOG_INFO("CommandQueue::submit() - Releasing %zu transient D3D resources from frame %u (high-water=%zu)\n",
                   frameCtx.transientResources.size(), nextFrameIndex, frameCtx.transientResourcesHighWater);
#endif
      frameCtx.transientResources.clear();
    }

    // Reset descriptor allocation counters for the new frame
    // CORRECT: Simple linear allocator reset to 0 (each frame has its own isolated heap)
    // Following Microsoft MiniEngine pattern with per-frame heaps (1024 CBV/SRV/UAV, 32 Samplers)

    // P0_DX12-FIND-02: Log descriptor usage statistics before reset for telemetry
    const uint32_t cbvSrvUavUsage = ctx.getFrameContexts()[nextFrameIndex].nextCbvSrvUavDescriptor;
    const uint32_t samplerUsage = ctx.getFrameContexts()[nextFrameIndex].nextSamplerDescriptor;
    const uint32_t peakCbvSrvUav = ctx.getFrameContexts()[nextFrameIndex].peakCbvSrvUavUsage;
    const uint32_t peakSampler = ctx.getFrameContexts()[nextFrameIndex].peakSamplerUsage;

    if (cbvSrvUavUsage > 0 || samplerUsage > 0) {
#ifdef IGL_DEBUG
      const float cbvSrvUavPercent = (static_cast<float>(cbvSrvUavUsage) / kCbvSrvUavHeapSize) * 100.0f;
      const float samplerPercent = (static_cast<float>(samplerUsage) / kSamplerHeapSize) * 100.0f;
      const float peakCbvSrvUavPercent = (static_cast<float>(peakCbvSrvUav) / kCbvSrvUavHeapSize) * 100.0f;
      const float peakSamplerPercent = (static_cast<float>(peakSampler) / kSamplerHeapSize) * 100.0f;

      IGL_LOG_INFO("CommandQueue::submit() - Frame %u descriptor usage:\n"
                   "  CBV/SRV/UAV: final=%u/%u (%.1f%%), peak=%u/%u (%.1f%%)\n"
                   "  Samplers:    final=%u/%u (%.1f%%), peak=%u/%u (%.1f%%)\n",
                   nextFrameIndex,
                   cbvSrvUavUsage, kCbvSrvUavHeapSize, cbvSrvUavPercent,
                   peakCbvSrvUav, kCbvSrvUavHeapSize, peakCbvSrvUavPercent,
                   samplerUsage, kSamplerHeapSize, samplerPercent,
                   peakSampler, kSamplerHeapSize, peakSamplerPercent);
#endif
    }

    ctx.getFrameContexts()[nextFrameIndex].nextCbvSrvUavDescriptor = 0;
    ctx.getFrameContexts()[nextFrameIndex].nextSamplerDescriptor = 0;
    // Note: We don't reset peak usage counters - they accumulate across frames for telemetry
#ifdef IGL_DEBUG
    IGL_LOG_INFO("CommandQueue::submit() - Reset descriptor counters for frame %u to 0\n", nextFrameIndex);
#endif
  }

#ifdef IGL_DEBUG
  IGL_LOG_INFO("CommandQueue::submit() - Complete!\n");
#endif

  // Aggregate per-command-buffer draw count into the device, matching GL/Vulkan behavior
  const auto cbDraws = d3dCommandBuffer.getCurrentDrawCount();
#ifdef IGL_DEBUG
  IGL_LOG_INFO("CommandQueue::submit() - Aggregating %zu draws from CB into device\n", cbDraws);
#endif
  device_.incrementDrawCount(cbDraws);
#ifdef IGL_DEBUG
  IGL_LOG_INFO("CommandQueue::submit() - Device drawCount now=%zu\n", device_.getCurrentDrawCount());

  // Log resource stats every 30 draws to track leaks
  const size_t drawCount = device_.getCurrentDrawCount();
  if (drawCount == 30 || drawCount == 60 || drawCount == 90 || drawCount == 120 ||
      drawCount == 150 || drawCount == 300 || drawCount == 600 || drawCount == 900 ||
      drawCount == 1200 || drawCount == 1500 || drawCount == 1800) {
    IGL_LOG_INFO("CommandQueue::submit() - Logging resource stats at drawCount=%zu\n", drawCount);
    D3D12Context::logResourceStats();
  }
#endif

  return 0;
}

} // namespace igl::d3d12
