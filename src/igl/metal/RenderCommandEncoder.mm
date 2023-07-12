/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/RenderCommandEncoder.h>

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <igl/RenderPass.h>
#include <igl/metal/Buffer.h>
#include <igl/metal/DepthStencilState.h>
#include <igl/metal/Framebuffer.h>
#include <igl/metal/RenderPipelineState.h>
#include <igl/metal/SamplerState.h>
#include <igl/metal/Texture.h>

namespace igl {
namespace metal {

RenderCommandEncoder::RenderCommandEncoder(const std::shared_ptr<CommandBuffer>& commandBuffer) :
  IRenderCommandEncoder::IRenderCommandEncoder(commandBuffer) {}

void RenderCommandEncoder::initialize(const std::shared_ptr<CommandBuffer>& commandBuffer,
                                      const RenderPassDesc& renderPass,
                                      const std::shared_ptr<IFramebuffer>& framebuffer,
                                      Result* outResult) {
  Result::setOk(outResult);
  if (!IGL_VERIFY(framebuffer)) {
    Result::setResult(outResult, Result::Code::ArgumentNull);
    return;
  }
  MTLRenderPassDescriptor* metalRenderPassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
  const FramebufferDesc& desc = static_cast<const Framebuffer&>(*framebuffer).get();

  // Colors
  for (const auto& attachment : desc.colorAttachments) {
    size_t index = attachment.first;

    if (index >= renderPass.colorAttachments.size() || index >= IGL_COLOR_ATTACHMENTS_MAX) {
      static const char* kNotEnoughRenderPassColorAttachments =
          "Framebuffer color attachment count larger than renderPass color attachment count";
      IGL_ASSERT_MSG(false, kNotEnoughRenderPassColorAttachments);
      Result::setResult(
          outResult, Result::Code::ArgumentInvalid, kNotEnoughRenderPassColorAttachments);
      break;
    }

    auto& iglTexture = attachment.second.texture;
    MTLRenderPassColorAttachmentDescriptor* metalColorAttachment =
        metalRenderPassDesc.colorAttachments[index];

    static const char* kNullColorAttachmentMsg = "Render pass color attachment cannot be null";
    IGL_ASSERT_MSG(iglTexture, kNullColorAttachmentMsg);
    if (iglTexture) {
      metalColorAttachment.texture = static_cast<Texture&>(*iglTexture).get();
    } else {
      Result::setResult(outResult, Result::Code::ArgumentNull, kNullColorAttachmentMsg);
    }

    auto& iglResolveTexture = attachment.second.resolveTexture;
    if (iglResolveTexture &&
        renderPass.colorAttachments[index].storeAction == igl::StoreAction::MsaaResolve) {
      metalColorAttachment.resolveTexture = static_cast<Texture&>(*iglResolveTexture).get();
    }

    auto& iglColorAttachment = renderPass.colorAttachments[index];
    metalColorAttachment.loadAction = convertLoadAction(iglColorAttachment.loadAction);
    metalColorAttachment.storeAction = convertStoreAction(iglColorAttachment.storeAction);
    metalColorAttachment.clearColor = convertClearColor(iglColorAttachment.clearColor);
    metalColorAttachment.slice = iglColorAttachment.layer;
    metalColorAttachment.level = iglColorAttachment.mipmapLevel;
  }

  // Depth
  if (desc.depthAttachment.texture) {
    metalRenderPassDesc.depthAttachment.texture =
        static_cast<Texture&>(*desc.depthAttachment.texture).get();
    metalRenderPassDesc.depthAttachment.loadAction =
        convertLoadAction(renderPass.depthAttachment.loadAction);
    metalRenderPassDesc.depthAttachment.storeAction =
        convertStoreAction(renderPass.depthAttachment.storeAction);
    metalRenderPassDesc.depthAttachment.clearDepth = renderPass.depthAttachment.clearDepth;

    if (desc.depthAttachment.resolveTexture &&
        renderPass.depthAttachment.storeAction == igl::StoreAction::MsaaResolve) {
      metalRenderPassDesc.depthAttachment.resolveTexture =
          static_cast<Texture&>(*desc.depthAttachment.resolveTexture).get();
    }
  }

  // Stencil
  if (desc.stencilAttachment.texture) {
    metalRenderPassDesc.stencilAttachment.texture =
        static_cast<Texture&>(*desc.stencilAttachment.texture).get();
    metalRenderPassDesc.stencilAttachment.loadAction =
        convertLoadAction(renderPass.stencilAttachment.loadAction);
    metalRenderPassDesc.stencilAttachment.storeAction =
        convertStoreAction(renderPass.stencilAttachment.storeAction);
    metalRenderPassDesc.stencilAttachment.clearStencil = renderPass.stencilAttachment.clearStencil;

    if (desc.stencilAttachment.resolveTexture &&
        renderPass.stencilAttachment.storeAction == igl::StoreAction::MsaaResolve) {
      metalRenderPassDesc.stencilAttachment.resolveTexture =
          static_cast<Texture&>(*desc.stencilAttachment.resolveTexture).get();
    }
  }

  encoder_ = [commandBuffer->get() renderCommandEncoderWithDescriptor:metalRenderPassDesc];
}

std::unique_ptr<RenderCommandEncoder> RenderCommandEncoder::create(
    const std::shared_ptr<CommandBuffer>& commandBuffer,
    const RenderPassDesc& renderPass,
    const std::shared_ptr<IFramebuffer>& framebuffer,
    Result* outResult) {
  std::unique_ptr<RenderCommandEncoder> encoder(new RenderCommandEncoder(commandBuffer));
  encoder->initialize(commandBuffer, renderPass, framebuffer, outResult);
  return encoder;
}

void RenderCommandEncoder::endEncoding() {
  // @fb-only
  // @fb-only
  [encoder_ endEncoding];
  encoder_ = nil;
}

void RenderCommandEncoder::pushDebugGroupLabel(const std::string& label,
                                               const igl::Color& /*color*/) const {
  IGL_ASSERT(encoder_);
  IGL_ASSERT(!label.empty());
  [encoder_ pushDebugGroup:[NSString stringWithUTF8String:label.c_str()] ?: @""];
}

void RenderCommandEncoder::insertDebugEventLabel(const std::string& label,
                                                 const igl::Color& /*color*/) const {
  IGL_ASSERT(encoder_);
  IGL_ASSERT(!label.empty());
  [encoder_ insertDebugSignpost:[NSString stringWithUTF8String:label.c_str()] ?: @""];
}

void RenderCommandEncoder::popDebugGroupLabel() const {
  IGL_ASSERT(encoder_);
  [encoder_ popDebugGroup];
}

void RenderCommandEncoder::bindViewport(const Viewport& viewport) {
  IGL_ASSERT(encoder_);
  MTLViewport metalViewport = {viewport.x,
                               viewport.y,
                               viewport.width,
                               viewport.height,
                               viewport.minDepth,
                               viewport.maxDepth};
  [encoder_ setViewport:metalViewport];
}

void RenderCommandEncoder::bindScissorRect(const ScissorRect& rect) {
  IGL_ASSERT(encoder_);
  MTLScissorRect scissorRect = {rect.x, rect.y, rect.width, rect.height};
  [encoder_ setScissorRect:scissorRect];
}

void RenderCommandEncoder::bindCullMode(const CullMode& cullMode) {
  IGL_ASSERT(encoder_);
  MTLCullMode mode = MTLCullModeNone;
  switch (cullMode) {
  case CullMode::Disabled:
    mode = MTLCullModeNone;
    break;
  case CullMode::Front:
    mode = MTLCullModeFront;
    break;
  case CullMode::Back:
    mode = MTLCullModeBack;
    break;
  }
  [encoder_ setCullMode:mode];
}

void RenderCommandEncoder::bindFrontFacingWinding(const WindingMode& frontFaceWinding) {
  IGL_ASSERT(encoder_);
  MTLWinding mode = (frontFaceWinding == WindingMode::Clockwise) ? MTLWindingClockwise
                                                                 : MTLWindingCounterClockwise;

  [encoder_ setFrontFacingWinding:mode];
}

void RenderCommandEncoder::bindPolygonFillMode(const PolygonFillMode& polygonFillMode) {
  IGL_ASSERT(encoder_);

  if (polygonFillMode == PolygonFillMode::Fill) {
    return;
  }

  [encoder_ setTriangleFillMode:MTLTriangleFillModeLines];
}

void RenderCommandEncoder::bindRenderPipelineState(
    const std::shared_ptr<IRenderPipelineState>& pipelineState) {
  IGL_ASSERT(encoder_);
  IGL_ASSERT(pipelineState);
  if (!pipelineState) {
    return;
  }
  auto& metalPipelineState = static_cast<RenderPipelineState&>(*pipelineState);

  [encoder_ setRenderPipelineState:metalPipelineState.get()];

  bindCullMode(metalPipelineState.getCullMode());
  bindFrontFacingWinding(metalPipelineState.getWindingMode());
  bindPolygonFillMode(metalPipelineState.getPolygonFillMode());
}

void RenderCommandEncoder::bindDepthStencilState(
    const std::shared_ptr<IDepthStencilState>& depthStencilState) {
  IGL_ASSERT(encoder_);
  if (depthStencilState) {
    [encoder_ setDepthStencilState:static_cast<DepthStencilState&>(*depthStencilState).get()];
  }
}

void RenderCommandEncoder::setBlendColor(Color color) {
  [encoder_ setBlendColorRed:color.r green:color.g blue:color.b alpha:color.a];
}

void RenderCommandEncoder::setDepthBias(float depthBias, float slopeScale, float clamp) {
  IGL_ASSERT(encoder_);
  [encoder_ setDepthBias:depthBias slopeScale:slopeScale clamp:clamp];
}

void RenderCommandEncoder::setStencilReferenceValue(uint32_t value) {
  IGL_ASSERT(encoder_);
  [encoder_ setStencilReferenceValue:value];
}

void RenderCommandEncoder::setStencilReferenceValues(uint32_t frontValue, uint32_t backValue) {
  IGL_ASSERT(encoder_);
  [encoder_ setStencilFrontReferenceValue:frontValue backReferenceValue:backValue];
}

void RenderCommandEncoder::bindBuffer(int index,
                                      uint8_t bindTarget,
                                      const std::shared_ptr<IBuffer>& buffer,
                                      size_t offset) {
  IGL_ASSERT(encoder_);
  IGL_ASSERT_MSG(bindTarget == BindTarget::kVertex || bindTarget == BindTarget::kFragment ||
                     bindTarget == BindTarget::kAllGraphics,
                 "Bind target is not valid: %d",
                 bindTarget);
  if (buffer) {
    auto& metalBuffer = static_cast<Buffer&>(*buffer);
    if ((bindTarget & BindTarget::kVertex) != 0) {
      [encoder_ setVertexBuffer:metalBuffer.get() offset:offset atIndex:index];
    }
    if ((bindTarget & BindTarget::kFragment) != 0) {
      [encoder_ setFragmentBuffer:metalBuffer.get() offset:offset atIndex:index];
    }
  }
}

void RenderCommandEncoder::bindBytes(size_t index,
                                     uint8_t bindTarget,
                                     const void* data,
                                     size_t length) {
  IGL_ASSERT(encoder_);
  IGL_ASSERT_MSG(bindTarget == BindTarget::kVertex || bindTarget == BindTarget::kFragment ||
                     bindTarget == BindTarget::kAllGraphics,
                 "Bind target is not valid: %d",
                 bindTarget);
  if (data) {
    if (length > MAX_RECOMMENDED_BYTES) {
      IGL_LOG_INFO(
          "It is recommended to use bindBuffer instead of bindBytes when binding > 4kb: %u",
          length);
    }
    if ((bindTarget & BindTarget::kVertex) != 0) {
      [encoder_ setVertexBytes:data length:length atIndex:index];
    }
    if ((bindTarget & BindTarget::kFragment) != 0) {
      [encoder_ setFragmentBytes:data length:length atIndex:index];
    }
  }
}

void RenderCommandEncoder::bindPushConstants(size_t /*offset*/,
                                             const void* /*data*/,
                                             size_t /*length*/) {
  IGL_ASSERT_NOT_IMPLEMENTED();
}

void RenderCommandEncoder::bindTexture(size_t index, uint8_t bindTarget, ITexture* texture) {
  IGL_ASSERT(encoder_);
  IGL_ASSERT_MSG(bindTarget == BindTarget::kVertex || bindTarget == BindTarget::kFragment ||
                     bindTarget == BindTarget::kAllGraphics,
                 "Bind target is not valid: %d",
                 bindTarget);

  auto* iglTexture = static_cast<Texture*>(texture);
  auto metalTexture = iglTexture ? iglTexture->get() : nil;

  if ((bindTarget & BindTarget::kVertex) != 0) {
    [encoder_ setVertexTexture:metalTexture atIndex:index];
  }
  if ((bindTarget & BindTarget::kFragment) != 0) {
    [encoder_ setFragmentTexture:metalTexture atIndex:index];
  }
}

void RenderCommandEncoder::bindUniform(const UniformDesc& /*uniformDesc*/, const void* /*data*/) {
  // DO NOT IMPLEMENT!
  // This is only for backends that MUST use single uniforms in some situations.
  IGL_ASSERT_NOT_IMPLEMENTED();
}

void RenderCommandEncoder::bindSamplerState(size_t index,
                                            uint8_t bindTarget,
                                            const std::shared_ptr<ISamplerState>& samplerState) {
  IGL_ASSERT(encoder_);
  IGL_ASSERT_MSG(bindTarget == BindTarget::kVertex || bindTarget == BindTarget::kFragment ||
                     bindTarget == BindTarget::kAllGraphics,
                 "Bind target is not valid: %d",
                 bindTarget);

  auto* metalSamplerState = static_cast<SamplerState*>(samplerState.get());

  if ((bindTarget & BindTarget::kVertex) != 0) {
    [encoder_ setVertexSamplerState:metalSamplerState->get() atIndex:index];
  }
  if ((bindTarget & BindTarget::kFragment) != 0) {
    [encoder_ setFragmentSamplerState:metalSamplerState->get() atIndex:index];
  }
}

void RenderCommandEncoder::draw(PrimitiveType primitiveType,
                                size_t vertexStart,
                                size_t vertexCount) {
  getCommandBuffer().incrementCurrentDrawCount();
  IGL_ASSERT(encoder_);
  MTLPrimitiveType metalPrimitive = convertPrimitiveType(primitiveType);
  [encoder_ drawPrimitives:metalPrimitive vertexStart:vertexStart vertexCount:vertexCount];
}

void RenderCommandEncoder::drawIndexed(PrimitiveType primitiveType,
                                       size_t indexCount,
                                       IndexFormat indexFormat,
                                       IBuffer& indexBuffer,
                                       size_t indexBufferOffset) {
  getCommandBuffer().incrementCurrentDrawCount();
  IGL_ASSERT(encoder_);
  auto& buffer = (Buffer&)(indexBuffer);
  MTLPrimitiveType metalPrimitive = convertPrimitiveType(primitiveType);
  MTLIndexType indexType = convertIndexType(indexFormat);

  [encoder_ drawIndexedPrimitives:metalPrimitive
                       indexCount:indexCount
                        indexType:indexType
                      indexBuffer:buffer.get()
                indexBufferOffset:indexBufferOffset];
}

void RenderCommandEncoder::drawIndexedIndirect(PrimitiveType primitiveType,
                                               IndexFormat indexFormat,
                                               IBuffer& indexBuffer,
                                               IBuffer& indirectBuffer,
                                               size_t indirectBufferOffset) {
  getCommandBuffer().incrementCurrentDrawCount();
  IGL_ASSERT(encoder_);
  auto& indexBufferRef = (Buffer&)(indexBuffer);
  auto& indirectBufferRef = (Buffer&)(indirectBuffer);
  MTLPrimitiveType metalPrimitive = convertPrimitiveType(primitiveType);
  MTLIndexType indexType = convertIndexType(indexFormat);

  [encoder_ drawIndexedPrimitives:metalPrimitive
                        indexType:indexType
                      indexBuffer:indexBufferRef.get()
                indexBufferOffset:0
                   indirectBuffer:indirectBufferRef.get()
             indirectBufferOffset:indirectBufferOffset];
}

void RenderCommandEncoder::multiDrawIndirect(PrimitiveType primitiveType,
                                             IBuffer& indirectBuffer,
                                             // Ignore bugprone-easily-swappable-parameters
                                             // @lint-ignore CLANGTIDY
                                             size_t indirectBufferOffset,
                                             uint32_t drawCount,
                                             uint32_t stride) {
  IGL_ASSERT(encoder_);
  stride = stride ? stride : sizeof(MTLDrawPrimitivesIndirectArguments);
  auto& indirectBufferRef = (Buffer&)(indirectBuffer);
  MTLPrimitiveType metalPrimitive = convertPrimitiveType(primitiveType);

  for (uint32_t drawIndex = 0; drawIndex < drawCount; drawIndex++) {
    getCommandBuffer().incrementCurrentDrawCount();
    [encoder_ drawPrimitives:metalPrimitive
              indirectBuffer:indirectBufferRef.get()
        indirectBufferOffset:indirectBufferOffset + static_cast<size_t>(stride) * drawIndex];
  }
}

void RenderCommandEncoder::multiDrawIndexedIndirect(PrimitiveType primitiveType,
                                                    IndexFormat indexFormat,
                                                    // Ignore bugprone-easily-swappable-parameters
                                                    // @lint-ignore CLANGTIDY
                                                    IBuffer& indexBuffer,
                                                    IBuffer& indirectBuffer,
                                                    // Ignore bugprone-easily-swappable-parameters
                                                    // @lint-ignore CLANGTIDY
                                                    size_t indirectBufferOffset,
                                                    uint32_t drawCount,
                                                    uint32_t stride) {
  IGL_ASSERT(encoder_);
  stride = stride ? stride : sizeof(MTLDrawIndexedPrimitivesIndirectArguments);
  auto& indexBufferRef = (Buffer&)(indexBuffer);
  auto& indirectBufferRef = (Buffer&)(indirectBuffer);
  MTLPrimitiveType metalPrimitive = convertPrimitiveType(primitiveType);
  MTLIndexType indexType = convertIndexType(indexFormat);

  for (uint32_t drawIndex = 0; drawIndex < drawCount; drawIndex++) {
    getCommandBuffer().incrementCurrentDrawCount();
    [encoder_ drawIndexedPrimitives:metalPrimitive
                          indexType:indexType
                        indexBuffer:indexBufferRef.get()
                  indexBufferOffset:0
                     indirectBuffer:indirectBufferRef.get()
               indirectBufferOffset:indirectBufferOffset + static_cast<size_t>(stride) * drawIndex];
  }
}

MTLPrimitiveType RenderCommandEncoder::convertPrimitiveType(PrimitiveType value) {
  switch (value) {
  case PrimitiveType::Point:
    return MTLPrimitiveTypePoint;
  case PrimitiveType::Line:
    return MTLPrimitiveTypeLine;
  case PrimitiveType::LineStrip:
    return MTLPrimitiveTypeLineStrip;
  case PrimitiveType::Triangle:
    return MTLPrimitiveTypeTriangle;
  case PrimitiveType::TriangleStrip:
    return MTLPrimitiveTypeTriangleStrip;
  }
}

MTLIndexType RenderCommandEncoder::convertIndexType(IndexFormat value) {
  switch (value) {
  case IndexFormat::UInt16:
    return MTLIndexTypeUInt16;
  case IndexFormat::UInt32:
    return MTLIndexTypeUInt32;
  }
}

MTLLoadAction RenderCommandEncoder::convertLoadAction(LoadAction value) {
  switch (value) {
  case LoadAction::DontCare:
    return MTLLoadActionDontCare;
  case LoadAction::Clear:
    return MTLLoadActionClear;
  case LoadAction::Load:
    return MTLLoadActionLoad;
  }
}

MTLStoreAction RenderCommandEncoder::convertStoreAction(StoreAction value) {
  switch (value) {
  case StoreAction::DontCare:
    return MTLStoreActionDontCare;
  case StoreAction::Store:
    return MTLStoreActionStore;
  case StoreAction::MsaaResolve:
    return MTLStoreActionMultisampleResolve;
  }
}

MTLClearColor RenderCommandEncoder::convertClearColor(Color value) {
  return MTLClearColorMake(value.r, value.g, value.b, value.a);
}

} // namespace metal
} // namespace igl
