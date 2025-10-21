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

  // Create command allocator
  HRESULT hr = d3dDevice->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      IID_PPV_ARGS(commandAllocator_.GetAddressOf()));

  if (FAILED(hr)) {
    IGL_DEBUG_ABORT("Failed to create command allocator");
  }

  // Create command list
  hr = d3dDevice->CreateCommandList(
      0,
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      commandAllocator_.Get(),
      nullptr,
      IID_PPV_ARGS(commandList_.GetAddressOf()));

  if (FAILED(hr)) {
    IGL_DEBUG_ABORT("Failed to create command list");
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
  return std::make_unique<RenderCommandEncoder>(*this, renderPass);
}

std::unique_ptr<IComputeCommandEncoder> CommandBuffer::createComputeCommandEncoder() {
  return nullptr;
}

void CommandBuffer::present(const std::shared_ptr<ITexture>& /*surface*/) const {
  // Stub: Not yet implemented
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

} // namespace igl::d3d12
