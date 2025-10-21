/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/RenderCommandEncoder.h>
#include <igl/d3d12/CommandBuffer.h>
#include <igl/d3d12/Buffer.h>
#include <igl/d3d12/RenderPipelineState.h>
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
  if (!backBuffer) {
    IGL_LOG_ERROR("RenderCommandEncoder: No back buffer available\n");
    return;
  }

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

  if (!backBuffer) {
    IGL_LOG_ERROR("RenderCommandEncoder::endEncoding: No back buffer available\n");
    return;
  }

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

void RenderCommandEncoder::bindViewport(const Viewport& viewport) {
  D3D12_VIEWPORT vp = {};
  vp.TopLeftX = viewport.x;
  vp.TopLeftY = viewport.y;
  vp.Width = viewport.width;
  vp.Height = viewport.height;
  vp.MinDepth = viewport.minDepth;
  vp.MaxDepth = viewport.maxDepth;
  commandList_->RSSetViewports(1, &vp);
}

void RenderCommandEncoder::bindScissorRect(const ScissorRect& rect) {
  D3D12_RECT scissor = {};
  scissor.left = static_cast<LONG>(rect.x);
  scissor.top = static_cast<LONG>(rect.y);
  scissor.right = static_cast<LONG>(rect.x + rect.width);
  scissor.bottom = static_cast<LONG>(rect.y + rect.height);
  commandList_->RSSetScissorRects(1, &scissor);
}

void RenderCommandEncoder::bindRenderPipelineState(
    const std::shared_ptr<IRenderPipelineState>& pipelineState) {
  if (!pipelineState) {
    return;
  }

  auto* d3dPipelineState = static_cast<const RenderPipelineState*>(pipelineState.get());
  commandList_->SetPipelineState(d3dPipelineState->getPipelineState());
  commandList_->SetGraphicsRootSignature(d3dPipelineState->getRootSignature());
  commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void RenderCommandEncoder::bindDepthStencilState(
    const std::shared_ptr<IDepthStencilState>& /*depthStencilState*/) {}


void RenderCommandEncoder::bindVertexBuffer(uint32_t index,
                                            IBuffer& buffer,
                                            size_t bufferOffset) {
  auto* d3dBuffer = static_cast<Buffer*>(&buffer);

  D3D12_VERTEX_BUFFER_VIEW vbView = {};
  vbView.BufferLocation = d3dBuffer->gpuAddress(bufferOffset);
  vbView.SizeInBytes = static_cast<UINT>(d3dBuffer->getSizeInBytes() - bufferOffset);
  // TODO: Get stride from vertex input state
  // For now, hardcode for simple triangle (float3 position + float4 color = 28 bytes)
  vbView.StrideInBytes = 28;

  commandList_->IASetVertexBuffers(index, 1, &vbView);
}

void RenderCommandEncoder::bindIndexBuffer(IBuffer& buffer,
                                           IndexFormat format,
                                           size_t bufferOffset) {
  auto* d3dBuffer = static_cast<Buffer*>(&buffer);

  D3D12_INDEX_BUFFER_VIEW ibView = {};
  ibView.BufferLocation = d3dBuffer->gpuAddress(bufferOffset);
  ibView.SizeInBytes = static_cast<UINT>(d3dBuffer->getSizeInBytes() - bufferOffset);
  ibView.Format = (format == IndexFormat::UInt16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

  commandList_->IASetIndexBuffer(&ibView);
}

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

void RenderCommandEncoder::draw(size_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t firstVertex,
                                uint32_t baseInstance) {
  commandList_->DrawInstanced(static_cast<UINT>(vertexCount),
                              instanceCount,
                              firstVertex,
                              baseInstance);
}

void RenderCommandEncoder::drawIndexed(size_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t vertexOffset,
                                       uint32_t baseInstance) {
  commandList_->DrawIndexedInstanced(static_cast<UINT>(indexCount),
                                     instanceCount,
                                     firstIndex,
                                     vertexOffset,
                                     baseInstance);
}
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
