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
    } else {
      IGL_LOG_INFO("CommandQueue::submit() - Present OK\n");
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
    ctx.getCurrentFrameIndex() = (ctx.getCurrentFrameIndex() + 1) % kMaxFramesInFlight;
    IGL_LOG_INFO("CommandQueue::submit() - Advanced to frame index %u\n", ctx.getCurrentFrameIndex());
  }

  // Wait for next frame's resources to be available (not current frame!)
  // This allows 2-3 frames in flight for CPU/GPU parallelism
  const UINT nextFrameIndex = (ctx.getCurrentFrameIndex() + 1) % kMaxFramesInFlight;
  const UINT64 nextFenceValue = ctx.getFrameContexts()[nextFrameIndex].fenceValue;

  if (nextFenceValue != 0 && fence->GetCompletedValue() < nextFenceValue) {
    IGL_LOG_INFO("CommandQueue::submit() - Waiting for frame %u (fence value=%llu)\n",
                 nextFrameIndex, nextFenceValue);
    HRESULT hr = fence->SetEventOnCompletion(nextFenceValue, ctx.getFenceEvent());
    if (SUCCEEDED(hr)) {
      WaitForSingleObject(ctx.getFenceEvent(), INFINITE);
      IGL_LOG_INFO("CommandQueue::submit() - Frame %u resources available\n", nextFrameIndex);
    } else {
      IGL_LOG_ERROR("CommandQueue::submit() - SetEventOnCompletion failed: 0x%08X\n", static_cast<unsigned>(hr));
    }
  } else {
    IGL_LOG_INFO("CommandQueue::submit() - Frame %u resources already available (fence=%llu, completed=%llu)\n",
                 nextFrameIndex, nextFenceValue, fence->GetCompletedValue());
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
