/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/RenderCommandEncoder.h>
#include <igl/d3d12/CommandBuffer.h>
#include <igl/RenderPass.h>

namespace igl::d3d12 {

RenderCommandEncoder::RenderCommandEncoder(CommandBuffer& commandBuffer,
                                           const RenderPassDesc& renderPass)
    : IRenderCommandEncoder(nullptr),
      commandBuffer_(commandBuffer),
      commandList_(commandBuffer.getCommandList()) {
  auto& context = commandBuffer_.getContext();

  // Transition render target to RENDER_TARGET state
  auto* backBuffer = context.getCurrentBackBuffer();
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = backBuffer;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

  commandList_->ResourceBarrier(1, &barrier);

  // Clear render target if requested
  if (!renderPass.colorAttachments.empty() &&
      renderPass.colorAttachments[0].loadAction == LoadAction::Clear) {
    const auto& clearColor = renderPass.colorAttachments[0].clearColor;
    const float color[] = {clearColor.r, clearColor.g, clearColor.b, clearColor.a};

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = context.getCurrentRTV();
    commandList_->ClearRenderTargetView(rtv, color, 0, nullptr);
  }

  // Set render target
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = context.getCurrentRTV();
  commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
}

void RenderCommandEncoder::endEncoding() {
  // Transition render target back to PRESENT state
  auto& context = commandBuffer_.getContext();
  auto* backBuffer = context.getCurrentBackBuffer();

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = backBuffer;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

  commandList_->ResourceBarrier(1, &barrier);

  // Close the command buffer
  commandBuffer_.end();
}

void RenderCommandEncoder::bindViewport(const Viewport& /*viewport*/) {}
void RenderCommandEncoder::bindScissorRect(const ScissorRect& /*rect*/) {}
void RenderCommandEncoder::bindRenderPipelineState(
    const std::shared_ptr<IRenderPipelineState>& /*pipelineState*/) {}
void RenderCommandEncoder::bindDepthStencilState(
    const std::shared_ptr<IDepthStencilState>& /*depthStencilState*/) {}

void RenderCommandEncoder::bindVertexBuffer(uint32_t /*index*/,
                                            IBuffer& /*buffer*/,
                                            size_t /*bufferOffset*/) {}
void RenderCommandEncoder::bindIndexBuffer(IBuffer& /*buffer*/,
                                           IndexFormat /*format*/,
                                           size_t /*bufferOffset*/) {}

void RenderCommandEncoder::bindBytes(size_t /*index*/,
                                     uint8_t /*target*/,
                                     const void* /*data*/,
                                     size_t /*length*/) {}
void RenderCommandEncoder::bindPushConstants(const void* /*data*/,
                                             size_t /*length*/,
                                             size_t /*offset*/) {}
void RenderCommandEncoder::bindSamplerState(size_t /*index*/,
                                            uint8_t /*target*/,
                                            ISamplerState* /*samplerState*/) {}
void RenderCommandEncoder::bindTexture(size_t /*index*/,
                                       uint8_t /*target*/,
                                       ITexture* /*texture*/) {}
void RenderCommandEncoder::bindTexture(size_t /*index*/, ITexture* /*texture*/) {}
void RenderCommandEncoder::bindUniform(const UniformDesc& /*uniformDesc*/, const void* /*data*/) {}

void RenderCommandEncoder::draw(size_t /*vertexCount*/,
                                uint32_t /*instanceCount*/,
                                uint32_t /*firstVertex*/,
                                uint32_t /*baseInstance*/) {}
void RenderCommandEncoder::drawIndexed(size_t /*indexCount*/,
                                       uint32_t /*instanceCount*/,
                                       uint32_t /*firstIndex*/,
                                       int32_t /*vertexOffset*/,
                                       uint32_t /*baseInstance*/) {}
void RenderCommandEncoder::multiDrawIndirect(IBuffer& /*indirectBuffer*/,
                                             size_t /*indirectBufferOffset*/,
                                             uint32_t /*drawCount*/,
                                             uint32_t /*stride*/) {}
void RenderCommandEncoder::multiDrawIndexedIndirect(IBuffer& /*indirectBuffer*/,
                                                    size_t /*indirectBufferOffset*/,
                                                    uint32_t /*drawCount*/,
                                                    uint32_t /*stride*/) {}

void RenderCommandEncoder::setStencilReferenceValue(uint32_t /*value*/) {}
void RenderCommandEncoder::setBlendColor(const Color& /*color*/) {}
void RenderCommandEncoder::setDepthBias(float /*depthBias*/, float /*slopeScale*/, float /*clamp*/) {}

void RenderCommandEncoder::pushDebugGroupLabel(const char* /*label*/, const Color& /*color*/) const {}
void RenderCommandEncoder::insertDebugEventLabel(const char* /*label*/, const Color& /*color*/) const {}
void RenderCommandEncoder::popDebugGroupLabel() const {}

void RenderCommandEncoder::bindBuffer(uint32_t /*index*/,
                                       IBuffer* /*buffer*/,
                                       size_t /*offset*/,
                                       size_t /*bufferSize*/) {}
void RenderCommandEncoder::bindBindGroup(BindGroupTextureHandle /*handle*/) {}
void RenderCommandEncoder::bindBindGroup(BindGroupBufferHandle /*handle*/,
                                          uint32_t /*numDynamicOffsets*/,
                                          const uint32_t* /*dynamicOffsets*/) {}

} // namespace igl::d3d12
