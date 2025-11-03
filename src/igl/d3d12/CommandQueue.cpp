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
#include <stdexcept>

namespace igl::d3d12 {

CommandQueue::CommandQueue(Device& device) : device_(device) {}

std::shared_ptr<ICommandBuffer> CommandQueue::createCommandBuffer(const CommandBufferDesc& desc,
                                                                   Result* IGL_NULLABLE outResult) {
  Result::setOk(outResult);
  return std::make_shared<CommandBuffer>(device_, desc);
}

SubmitHandle CommandQueue::submit(const ICommandBuffer& commandBuffer, bool /*endOfFrame*/) {
  const auto& d3dCommandBuffer = static_cast<const CommandBuffer&>(commandBuffer);
  auto* d3dCommandList = d3dCommandBuffer.getCommandList();
  auto& ctx = device_.getD3D12Context();
  auto* d3dCommandQueue = ctx.getCommandQueue();
  auto* d3dDevice = ctx.getDevice();

  // Ensure the command list is closed before execution
  const_cast<CommandBuffer&>(d3dCommandBuffer).end();

  IGL_LOG_INFO("CommandQueue::submit() - Executing command list...\n");

  // Execute command list
  ID3D12CommandList* commandLists[] = {d3dCommandList};
  d3dCommandQueue->ExecuteCommandLists(1, commandLists);

  IGL_LOG_INFO("CommandQueue::submit() - Command list executed, checking device status...\n");

  // Check for device removed error
  HRESULT hr = d3dDevice->GetDeviceRemovedReason();
  if (FAILED(hr)) {
    IGL_LOG_ERROR("DEVICE REMOVED! Reason: 0x%08X\n", static_cast<unsigned>(hr));

    // Print debug layer messages
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(d3dDevice->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf())))) {
      UINT64 numMessages = infoQueue->GetNumStoredMessages();
      IGL_LOG_INFO("D3D12 Info Queue has %llu messages:\n", numMessages);
      for (UINT64 i = 0; i < numMessages; ++i) {
        SIZE_T messageLength = 0;
        infoQueue->GetMessage(i, nullptr, &messageLength);
        if (messageLength > 0) {
          auto message = (D3D12_MESSAGE*)malloc(messageLength);
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
    }

    throw std::runtime_error("D3D12 device removed - check debug output above");
  }

  IGL_LOG_INFO("CommandQueue::submit() - Device OK, presenting...\n");

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
          presentFlags |= DXGI_PRESENT_ALLOW_TEARING;
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
        throw std::runtime_error("D3D12 device removed during Present");
      }
    } else {
      IGL_LOG_INFO("CommandQueue::submit() - Present OK\n");
    }

    // CRITICAL: Check device status AFTER Present() as well
    // Present() can trigger device removal if command lists have errors
    HRESULT postPresentStatus = d3dDevice->GetDeviceRemovedReason();
    if (FAILED(postPresentStatus)) {
      IGL_LOG_ERROR("DEVICE REMOVED after Present! Reason: 0x%08X\n", static_cast<unsigned>(postPresentStatus));

      // Print debug layer messages
      Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
      if (SUCCEEDED(d3dDevice->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf())))) {
        UINT64 numMessages = infoQueue->GetNumStoredMessages();
        IGL_LOG_INFO("D3D12 Info Queue has %llu messages:\n", numMessages);
        for (UINT64 i = 0; i < numMessages; ++i) {
          SIZE_T messageLength = 0;
          infoQueue->GetMessage(i, nullptr, &messageLength);
          if (messageLength > 0) {
            auto message = (D3D12_MESSAGE*)malloc(messageLength);
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
      }

      throw std::runtime_error("D3D12 device removed after Present - check debug output above");
    }
  }

  // Per-frame fencing: Signal fence for current frame
  const UINT64 currentFenceValue = ++ctx.getFenceValue();
  auto* fence = ctx.getFence();

  d3dCommandQueue->Signal(fence, currentFenceValue);
  ctx.getFrameContexts()[ctx.getCurrentFrameIndex()].fenceValue = currentFenceValue;
  IGL_LOG_INFO("CommandQueue::submit() - Signaled fence for frame %u (value=%llu)\n",
               ctx.getCurrentFrameIndex(), currentFenceValue);

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
      IGL_LOG_INFO("CommandQueue::submit() - SAFETY WAIT: Ensuring frame pipeline is not overloaded (completed=%llu, need=%llu)\n",
                   currentCompletedValue, minimumSafeFence);
      // Wait for the frame that's (kMaxFramesInFlight-1) frames back to complete
      // This ensures we never have more than kMaxFramesInFlight frames in flight
      HRESULT hr = fence->SetEventOnCompletion(minimumSafeFence, ctx.getFenceEvent());
      if (SUCCEEDED(hr)) {
        if (fence->GetCompletedValue() < minimumSafeFence) {
          WaitForSingleObject(ctx.getFenceEvent(), INFINITE);
          IGL_LOG_INFO("CommandQueue::submit() - Safety wait completed (fence now=%llu)\n",
                       fence->GetCompletedValue());
        }
      }
    }

    if (nextFrameFence != 0 && fence->GetCompletedValue() < nextFrameFence) {
      IGL_LOG_INFO("CommandQueue::submit() - Waiting for frame %u to complete (fence value=%llu, current=%llu)\n",
                   nextFrameIndex, nextFrameFence, fence->GetCompletedValue());
      HRESULT hr = fence->SetEventOnCompletion(nextFrameFence, ctx.getFenceEvent());
      if (SUCCEEDED(hr)) {
        // CRITICAL: Re-check fence after SetEventOnCompletion to avoid race condition
        // The fence may have completed between our initial check and SetEventOnCompletion,
        // in which case the event won't be signaled and we'd wait forever
        if (fence->GetCompletedValue() < nextFrameFence) {
          // Fence still not complete, proceed with wait
          // Use timeout to prevent deadlock during window drag (when Present blocks message pump)
          DWORD waitResult = WaitForSingleObject(ctx.getFenceEvent(), 5000); // 5 second timeout
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
                IGL_LOG_INFO("CommandQueue::submit() - GPU finally completed after %d spins (fence now=%llu)\n",
                             spinCount, fence->GetCompletedValue());
              } else {
                IGL_LOG_ERROR("CommandQueue::submit() - GPU STILL not complete after busy-wait! (fence=%llu)\n",
                             fence->GetCompletedValue());
                // Last resort: wait forever
                while (fence->GetCompletedValue() < nextFrameFence) {
                  Sleep(10);
                }
                IGL_LOG_INFO("CommandQueue::submit() - Emergency infinite wait completed (fence=%llu)\n",
                             fence->GetCompletedValue());
              }
            } else {
              IGL_LOG_INFO("CommandQueue::submit() - Frame %u resources now available (completed=%llu)\n",
                           nextFrameIndex, completedAfterWait);
            }
          } else if (waitResult == WAIT_TIMEOUT) {
            IGL_LOG_ERROR("CommandQueue::submit() - TIMEOUT waiting for frame %u fence %llu (completed=%llu)\n",
                         nextFrameIndex, nextFrameFence, fence->GetCompletedValue());
            IGL_LOG_ERROR("CommandQueue::submit() - Forcing infinite wait to prevent device removed...\n");
            // CRITICAL: Must wait for GPU to finish before resetting allocator
            // This can happen during window drag or very low FPS scenarios
            WaitForSingleObject(ctx.getFenceEvent(), INFINITE);
            IGL_LOG_INFO("CommandQueue::submit() - Infinite wait completed (fence now=%llu)\n",
                         fence->GetCompletedValue());
          } else {
            IGL_LOG_ERROR("CommandQueue::submit() - Wait failed with result 0x%08X\n", waitResult);
          }
        } else {
          // Fence completed during SetEventOnCompletion (race condition handled)
          IGL_LOG_INFO("CommandQueue::submit() - Frame %u fence completed during setup (race avoided, completed=%llu)\n",
                       nextFrameIndex, fence->GetCompletedValue());
        }
      } else {
        IGL_LOG_ERROR("CommandQueue::submit() - SetEventOnCompletion failed: 0x%08X\n", static_cast<unsigned>(hr));
      }
    } else {
      IGL_LOG_INFO("CommandQueue::submit() - Frame %u resources already available (fence=%llu, completed=%llu)\n",
                   nextFrameIndex, nextFrameFence, fence->GetCompletedValue());
    }

    // Now advance to next frame
    ctx.getCurrentFrameIndex() = nextFrameIndex;
    IGL_LOG_INFO("CommandQueue::submit() - Advanced to frame index %u\n", nextFrameIndex);

    // CRITICAL: Reset the command allocator for this frame AFTER fence wait
    // Following Microsoft's D3D12HelloFrameBuffering pattern: allocator can only be reset
    // when GPU has finished all work submitted with it
    auto* frameAllocator = ctx.getFrameContexts()[nextFrameIndex].allocator.Get();
    HRESULT allocResetHr = frameAllocator->Reset();
    if (FAILED(allocResetHr)) {
      IGL_LOG_ERROR("CommandQueue::submit() - FAILED to reset frame %u allocator: 0x%08X\n",
                    nextFrameIndex, static_cast<unsigned>(allocResetHr));
    } else {
      IGL_LOG_INFO("CommandQueue::submit() - Reset frame %u allocator successfully\n", nextFrameIndex);
    }

    // CRITICAL: Clear transient buffers from the frame we just waited for
    // The GPU has finished executing that frame, so these resources can now be released
    if (!ctx.getFrameContexts()[nextFrameIndex].transientBuffers.empty()) {
      IGL_LOG_INFO("CommandQueue::submit() - Clearing %zu transient buffers from frame %u\n",
                   ctx.getFrameContexts()[nextFrameIndex].transientBuffers.size(), nextFrameIndex);
      ctx.getFrameContexts()[nextFrameIndex].transientBuffers.clear();
    }

    // Reset descriptor allocation counters for the new frame
    // CORRECT: Simple linear allocator reset to 0 (each frame has its own isolated heap)
    // Following Microsoft MiniEngine pattern with per-frame heaps (1024 CBV/SRV/UAV, 32 Samplers)
    ctx.getFrameContexts()[nextFrameIndex].nextCbvSrvUavDescriptor = 0;
    ctx.getFrameContexts()[nextFrameIndex].nextSamplerDescriptor = 0;
    IGL_LOG_INFO("CommandQueue::submit() - Reset descriptor counters for frame %u to 0\n", nextFrameIndex);
  }

  IGL_LOG_INFO("CommandQueue::submit() - Complete!\n");

  // Aggregate per-command-buffer draw count into the device, matching GL/Vulkan behavior
  const auto cbDraws = d3dCommandBuffer.getCurrentDrawCount();
  IGL_LOG_INFO("CommandQueue::submit() - Aggregating %zu draws from CB into device\n", cbDraws);
  device_.incrementDrawCount(cbDraws);
  IGL_LOG_INFO("CommandQueue::submit() - Device drawCount now=%zu\n", device_.getCurrentDrawCount());

  return 0;
}

} // namespace igl::d3d12
