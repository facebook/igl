/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/RenderCommandEncoder.h>

namespace igl::d3d12 {

void RenderCommandEncoder::endEncoding() {}

void RenderCommandEncoder::pushDebugGroupLabel(const std::string& /*label*/,
                                               const igl::Color& /*color*/) const {}
void RenderCommandEncoder::insertDebugEventLabel(const std::string& /*label*/,
                                                 const igl::Color& /*color*/) const {}
void RenderCommandEncoder::popDebugGroupLabel() const {}

void RenderCommandEncoder::bindViewport(const Viewport& /*viewport*/) {}
void RenderCommandEncoder::bindScissorRect(const ScissorRect& /*rect*/) {}
void RenderCommandEncoder::bindRenderPipelineState(
    const std::shared_ptr<IRenderPipelineState>& /*pipelineState*/) {}
void RenderCommandEncoder::bindDepthStencilState(
    const std::shared_ptr<IDepthStencilState>& /*depthStencilState*/) {}

void RenderCommandEncoder::bindBuffer(uint32_t /*index*/,
                                      IBuffer* /*buffer*/,
                                      size_t /*bufferOffset*/) {}
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
void RenderCommandEncoder::bindBindGroup(BindGroupTextureHandle /*handle*/) {}
void RenderCommandEncoder::bindBindGroup(BindGroupBufferHandle /*handle*/,
                                         uint32_t /*dynamicOffset*/) {}

void RenderCommandEncoder::draw(size_t /*vertexCount*/,
                                uint32_t /*instanceCount*/,
                                uint32_t /*firstVertex*/,
                                uint32_t /*baseInstance*/) {}
void RenderCommandEncoder::drawIndexed(size_t /*indexCount*/,
                                       uint32_t /*instanceCount*/,
                                       uint32_t /*firstIndex*/,
                                       int32_t /*vertexOffset*/,
                                       uint32_t /*baseInstance*/) {}
void RenderCommandEncoder::drawIndexedIndirect(IBuffer& /*indirectBuffer*/,
                                               size_t /*indirectBufferOffset*/) {}
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

} // namespace igl::d3d12
