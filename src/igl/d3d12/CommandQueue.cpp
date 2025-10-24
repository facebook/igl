/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/CommandQueue.h>
#include <igl/d3d12/CommandBuffer.h>
#include <igl/d3d12/Device.h>
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
    HRESULT presentHr = swapChain->Present(1, 0); // VSync on
    if (FAILED(presentHr)) {
      IGL_LOG_ERROR("Present failed: 0x%08X\n", static_cast<unsigned>(presentHr));
    } else {
      IGL_LOG_INFO("CommandQueue::submit() - Present OK\n");
    }
  }

  IGL_LOG_INFO("CommandQueue::submit() - Waiting for GPU...\n");

  // Wait for GPU to finish (simple synchronization for Phase 2)
  // TODO: For Phase 3+, use per-frame fences for better performance
  ctx.waitForGPU();

  IGL_LOG_INFO("CommandQueue::submit() - Complete!\n");

  // Aggregate per-command-buffer draw count into the device, matching GL/Vulkan behavior
  const auto cbDraws = d3dCommandBuffer.getCurrentDrawCount();
  IGL_LOG_INFO("CommandQueue::submit() - Aggregating %zu draws from CB into device\n", cbDraws);
  device_.incrementDrawCount(cbDraws);
  IGL_LOG_INFO("CommandQueue::submit() - Device drawCount now=%zu\n", device_.getCurrentDrawCount());

  return 0;
}

} // namespace igl::d3d12
