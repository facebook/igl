/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/CommandBuffer.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/RenderCommandEncoder.h>

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

  // Create command allocator
  HRESULT hr = d3dDevice->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      IID_PPV_ARGS(commandAllocator_.GetAddressOf()));

  if (FAILED(hr)) {
    char errorMsg[256];
    snprintf(errorMsg, sizeof(errorMsg), "Failed to create command allocator: HRESULT = 0x%08X", static_cast<unsigned>(hr));
    IGL_DEBUG_ABORT(errorMsg);
  }

  // Create command list
  hr = d3dDevice->CreateCommandList(
      0,
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      commandAllocator_.Get(),
      nullptr,
      IID_PPV_ARGS(commandList_.GetAddressOf()));

  if (FAILED(hr)) {
    char errorMsg[256];
    snprintf(errorMsg, sizeof(errorMsg), "Failed to create command list: HRESULT = 0x%08X", static_cast<unsigned>(hr));
    IGL_DEBUG_ABORT(errorMsg);
  }

  // Command lists are created in recording state, close it for now
  commandList_->Close();
}

void CommandBuffer::begin() {
  commandAllocator_->Reset();
  commandList_->Reset(commandAllocator_.Get(), nullptr);
}

void CommandBuffer::end() {
  commandList_->Close();
}

D3D12Context& CommandBuffer::getContext() {
  return device_.getD3D12Context();
}

const D3D12Context& CommandBuffer::getContext() const {
  return device_.getD3D12Context();
}

std::unique_ptr<IRenderCommandEncoder> CommandBuffer::createRenderCommandEncoder(
    const RenderPassDesc& renderPass,
    const std::shared_ptr<IFramebuffer>& /*framebuffer*/,
    const Dependencies& /*dependencies*/,
    Result* IGL_NULLABLE outResult) {
  Result::setOk(outResult);

  // Begin command buffer if not already begun
  begin();

  return std::make_unique<RenderCommandEncoder>(*this, renderPass);
}

std::unique_ptr<IComputeCommandEncoder> CommandBuffer::createComputeCommandEncoder() {
  return nullptr;
}

void CommandBuffer::present(const std::shared_ptr<ITexture>& /*surface*/) const {
  // Present the current back buffer to the screen
  auto& context = device_.getD3D12Context();
  auto* swapChain = context.getSwapChain();

  if (!swapChain) {
    IGL_LOG_ERROR("CommandBuffer::present() - No swapchain available\n");
    return;
  }

  // Present with vsync (1) or without (0)
  HRESULT hr = swapChain->Present(1, 0);

  if (FAILED(hr)) {
    IGL_LOG_ERROR("CommandBuffer::present() - Present failed: HRESULT = 0x%08X\n", static_cast<unsigned>(hr));
  }
}

void CommandBuffer::waitUntilScheduled() {
  // Stub: Not yet implemented
}

void CommandBuffer::waitUntilCompleted() {
  // Stub: Not yet implemented
}

void CommandBuffer::pushDebugGroupLabel(const char* /*label*/,
                                        const igl::Color& /*color*/) const {
  // Stub: Not yet implemented
}

void CommandBuffer::popDebugGroupLabel() const {
  // Stub: Not yet implemented
}

void CommandBuffer::copyBuffer(IBuffer& /*source*/,
                               IBuffer& /*destination*/,
                               uint64_t /*sourceOffset*/,
                               uint64_t /*destinationOffset*/,
                               uint64_t /*size*/) {
  // Stub: Not yet implemented
}

void CommandBuffer::copyTextureToBuffer(ITexture& /*source*/,
                                       IBuffer& /*destination*/,
                                       uint64_t /*destinationOffset*/,
                                       uint32_t /*mipLevel*/,
                                       uint32_t /*layer*/) {
  // Stub: Not yet implemented
}

} // namespace igl::d3d12
