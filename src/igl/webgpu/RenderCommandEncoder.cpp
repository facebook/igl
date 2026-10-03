/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/RenderCommandEncoder.h>

#include <algorithm>
#include <utility>
#include <igl/Framebuffer.h>
#include <igl/webgpu/Buffer.h>
#include <igl/webgpu/CommandBuffer.h>
#include <igl/webgpu/DepthStencilState.h>
#include <igl/webgpu/Device.h>
#include <igl/webgpu/RenderPipelineState.h>
#include <igl/webgpu/SamplerState.h>
#include <igl/webgpu/Texture.h>

namespace igl::webgpu {

namespace {

// Sizes of the WebGPU (and Vulkan/Metal) indirect draw argument records.
constexpr uint32_t kDrawIndirectSize = 4 * sizeof(uint32_t);
constexpr uint32_t kDrawIndexedIndirectSize = 5 * sizeof(uint32_t);

bool isStrip(PrimitiveType topology) {
  return topology == PrimitiveType::LineStrip || topology == PrimitiveType::TriangleStrip;
}

// Index among the texture's attachment views of `desc`'s layer and face.
uint32_t getAttachmentLayer(const ITexture& texture, const RenderPassDesc::AttachmentDesc& desc) {
  return texture.getType() == TextureType::Cube ? desc.layer * 6u + desc.face : desc.layer;
}

} // namespace

std::unique_ptr<RenderCommandEncoder> RenderCommandEncoder::create(
    const std::shared_ptr<CommandBuffer>& commandBuffer,
    const RenderPassDesc& renderPass,
    const std::shared_ptr<IFramebuffer>& framebuffer,
    Result* IGL_NULLABLE outResult) {
  if (!framebuffer) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "A framebuffer is required");
    return nullptr;
  }
  auto encoder = std::unique_ptr<RenderCommandEncoder>(
      new RenderCommandEncoder(commandBuffer, commandBuffer->getDevice()));
  Result result = encoder->begin(renderPass, *framebuffer);
  if (!result.isOk()) {
    Result::setResult(outResult, std::move(result));
    return nullptr;
  }
  Result::setOk(outResult);
  return encoder;
}

RenderCommandEncoder::RenderCommandEncoder(const std::shared_ptr<CommandBuffer>& commandBuffer,
                                           Device& device) :
  IRenderCommandEncoder(commandBuffer),
  commandBuffer_(*commandBuffer),
  device_(device),
  binder_(device.getContext(), device.getDeviceFeatureSet()) {}

RenderCommandEncoder::~RenderCommandEncoder() {
  // WebGPU cannot finish a command encoder with an open pass.
  endEncoding();
}

Result RenderCommandEncoder::begin(const RenderPassDesc& renderPass,
                                   const IFramebuffer& framebuffer) {
  const uint64_t serial = commandBuffer_.getSerial();
  std::vector<WGPURenderPassColorAttachment> colors;
  for (const size_t index : framebuffer.getColorAttachmentIndices()) {
    const std::shared_ptr<ITexture> texture = framebuffer.getColorAttachment(index);
    if (!texture) {
      continue;
    }
    const RenderPassDesc::AttachmentDesc desc = index < renderPass.colorAttachments.size()
                                                    ? renderPass.colorAttachments[index]
                                                    : RenderPassDesc::AttachmentDesc{};
    const auto& wgpuTexture = static_cast<const Texture&>(*texture);
    const uint32_t layer = getAttachmentLayer(*texture, desc);
    const bool is3D = texture->getType() == TextureType::ThreeD;
    colors.resize(std::max(colors.size(), index + 1), WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT);
    WGPURenderPassColorAttachment& color = colors[index];
    color.view = wgpuTexture.getAttachmentView(desc.mipLevel, is3D ? 0 : layer);
    if (color.view == nullptr) {
      return Result(Result::Code::RuntimeError, "A color attachment has no texture to render to");
    }
    color.depthSlice = is3D ? layer : WGPU_DEPTH_SLICE_UNDEFINED;
    color.loadOp = loadActionToWGPULoadOp(desc.loadAction);
    color.storeOp = storeActionToWGPUStoreOp(desc.storeAction);
    color.clearValue = {.r = desc.clearColor.r,
                        .g = desc.clearColor.g,
                        .b = desc.clearColor.b,
                        .a = desc.clearColor.a};
    if (const std::shared_ptr<ITexture> resolve = framebuffer.getResolveColorAttachment(index)) {
      const auto& resolveTexture = static_cast<const Texture&>(*resolve);
      // Multisampled textures are 2D, so the face of a cube resolve target comes from its own type.
      color.resolveTarget =
          resolveTexture.getAttachmentView(desc.mipLevel, getAttachmentLayer(*resolve, desc));
      if (color.resolveTarget == nullptr) {
        return Result(Result::Code::RuntimeError,
                      "A resolve attachment has no texture to resolve to");
      }
      resolveTexture.recordUse(serial);
    } else if (desc.storeAction == StoreAction::MsaaResolve) {
      return Result(Result::Code::ArgumentInvalid, "MsaaResolve needs a resolve attachment");
    }
    wgpuTexture.recordUse(serial);
    attachments_.push_back({.texture = wgpuTexture.getWGPUTexture(),
                            .mipLevel = wgpuTexture.getBaseMipLevel() + desc.mipLevel,
                            .layer = wgpuTexture.getWGPULayer(desc.layer, desc.face)});
    targetWidth_ = std::max(texture->getDimensions().width >> desc.mipLevel, 1u);
    targetHeight_ = std::max(texture->getDimensions().height >> desc.mipLevel, 1u);
  }

  const std::shared_ptr<ITexture> depth = framebuffer.getDepthAttachment();
  const std::shared_ptr<ITexture> stencil = framebuffer.getStencilAttachment();
  if (depth && stencil && depth != stencil) {
    return Result(Result::Code::Unsupported,
                  "WebGPU has one depth/stencil attachment; use one texture for both");
  }
  WGPURenderPassDepthStencilAttachment depthStencil =
      WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
  const std::shared_ptr<ITexture>& depthStencilTexture = depth ? depth : stencil;
  if (depthStencilTexture) {
    const auto& wgpuTexture = static_cast<const Texture&>(*depthStencilTexture);
    const TextureFormatProperties& props = depthStencilTexture->getProperties();
    const RenderPassDesc::AttachmentDesc& depthDesc = renderPass.depthAttachment;
    depthStencil.view = wgpuTexture.getAttachmentView(
        depthDesc.mipLevel, getAttachmentLayer(*depthStencilTexture, depthDesc));
    if (depthStencil.view == nullptr) {
      return Result(Result::Code::RuntimeError, "The depth attachment has no texture to render to");
    }
    if (props.hasDepth()) {
      depthStencil.depthLoadOp = loadActionToWGPULoadOp(depthDesc.loadAction);
      depthStencil.depthStoreOp = storeActionToWGPUStoreOp(depthDesc.storeAction);
      depthStencil.depthClearValue = depthDesc.clearDepth;
    }
    if (props.hasStencil()) {
      const RenderPassDesc::AttachmentDesc& stencilDesc = renderPass.stencilAttachment;
      depthStencil.stencilLoadOp = loadActionToWGPULoadOp(stencilDesc.loadAction);
      depthStencil.stencilStoreOp = storeActionToWGPUStoreOp(stencilDesc.storeAction);
      depthStencil.stencilClearValue = stencilDesc.clearStencil;
    }
    wgpuTexture.recordUse(serial);
    attachments_.push_back({.texture = wgpuTexture.getWGPUTexture(),
                            .mipLevel = wgpuTexture.getBaseMipLevel() + depthDesc.mipLevel,
                            .layer = wgpuTexture.getWGPULayer(depthDesc.layer, depthDesc.face)});
    if (targetWidth_ == 0) {
      targetWidth_ = std::max(depthStencilTexture->getDimensions().width >> depthDesc.mipLevel, 1u);
      targetHeight_ =
          std::max(depthStencilTexture->getDimensions().height >> depthDesc.mipLevel, 1u);
    }
  }
  if (colors.empty() && !depthStencilTexture) {
    return Result(Result::Code::ArgumentInvalid, "The framebuffer has no attachments");
  }

  const std::optional<WGPUPassTimestampWrites> timestampWrites =
      commandBuffer_.getPassTimestampWrites(renderPass.timestampQuery.queries,
                                            renderPass.timestampQuery.slotIndex);
  WGPURenderPassDescriptor passDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
  passDesc.timestampWrites = timestampWrites ? &*timestampWrites : nullptr;
  passDesc.colorAttachmentCount = colors.size();
  passDesc.colorAttachments = colors.data();
  passDesc.depthStencilAttachment = depthStencilTexture ? &depthStencil : nullptr;
  pass_.reset(wgpuCommandEncoderBeginRenderPass(commandBuffer_.getWGPUCommandEncoder(), &passDesc));
  return pass_ ? Result()
               : Result(Result::Code::RuntimeError, "wgpuCommandEncoderBeginRenderPass() failed");
}

void RenderCommandEncoder::endEncoding() {
  if (pass_) {
    wgpuRenderPassEncoderEnd(pass_.get());
    pass_ = nullptr;
  }
}

void RenderCommandEncoder::pushDebugGroupLabel(const char* IGL_NONNULL label,
                                               const Color& /*color*/) const {
  if (pass_) {
    wgpuRenderPassEncoderPushDebugGroup(pass_.get(), toWGPUStringView(label));
  }
}

void RenderCommandEncoder::insertDebugEventLabel(const char* IGL_NONNULL label,
                                                 const Color& /*color*/) const {
  if (pass_) {
    wgpuRenderPassEncoderInsertDebugMarker(pass_.get(), toWGPUStringView(label));
  }
}

void RenderCommandEncoder::popDebugGroupLabel() const {
  if (pass_) {
    wgpuRenderPassEncoderPopDebugGroup(pass_.get());
  }
}

void RenderCommandEncoder::bindViewport(const Viewport& viewport) {
  if (!pass_) {
    return;
  }
  if (viewport.width < 0.0f || viewport.height < 0.0f) {
    IGL_LOG_ERROR_ONCE("WebGPU viewports cannot have a negative size\n");
    return;
  }
  wgpuRenderPassEncoderSetViewport(pass_.get(),
                                   viewport.x,
                                   viewport.y,
                                   viewport.width,
                                   viewport.height,
                                   std::clamp(viewport.minDepth, 0.0f, 1.0f),
                                   std::clamp(viewport.maxDepth, 0.0f, 1.0f));
}

void RenderCommandEncoder::bindScissorRect(const ScissorRect& rect) {
  if (!pass_) {
    return;
  }
  // WebGPU requires the scissor rectangle to lie within the render targets.
  const uint32_t x = std::min(rect.x, targetWidth_);
  const uint32_t y = std::min(rect.y, targetHeight_);
  wgpuRenderPassEncoderSetScissorRect(pass_.get(),
                                      x,
                                      y,
                                      std::min(rect.width, targetWidth_ - x),
                                      std::min(rect.height, targetHeight_ - y));
}

void RenderCommandEncoder::bindRenderPipelineState(
    const std::shared_ptr<IRenderPipelineState>& pipelineState) {
  pipeline_ = std::static_pointer_cast<RenderPipelineState>(pipelineState);
  // As on Metal, Vulkan and OpenGL, a newly bound pipeline brings back its own cull mode and
  // winding.
  cullMode_.reset();
  frontFaceWinding_.reset();
}

void RenderCommandEncoder::bindDepthStencilState(
    const std::shared_ptr<IDepthStencilState>& depthStencilState) {
  depthStencilState_ = depthStencilState
                           ? static_cast<const DepthStencilState&>(*depthStencilState).getDesc()
                           : DepthStencilStateDesc{};
}

void RenderCommandEncoder::bindBuffer(uint32_t index,
                                      uint8_t /*bindTarget*/,
                                      IBuffer* IGL_NULLABLE buffer,
                                      size_t bufferOffset,
                                      size_t bufferSize) {
  bindBuffer(index, buffer, bufferOffset, bufferSize);
}

void RenderCommandEncoder::bindBuffer(uint32_t index,
                                      IBuffer* IGL_NULLABLE buffer,
                                      size_t bufferOffset,
                                      size_t bufferSize) {
  binder_.bindBuffer(index, static_cast<Buffer*>(buffer), bufferOffset, bufferSize);
}

void RenderCommandEncoder::bindVertexBuffer(uint32_t index,
                                            IBuffer& buffer,
                                            size_t bufferOffset,
                                            size_t /*attributeStride*/) {
  if (!pass_ || index >= IGL_BUFFER_BINDINGS_MAX) {
    return;
  }
  auto& wgpuBuffer = static_cast<Buffer&>(buffer);
  wgpuRenderPassEncoderSetVertexBuffer(
      pass_.get(), index, wgpuBuffer.getWGPUBuffer(), bufferOffset, WGPU_WHOLE_SIZE);
  wgpuBuffer.recordUse(commandBuffer_.getSerial(), /*gpuWrite=*/false);
  boundVertexBuffers_ |= 1u << index;
}

void RenderCommandEncoder::bindIndexBuffer(IBuffer& buffer,
                                           IndexFormat format,
                                           size_t bufferOffset) {
  if (!pass_) {
    return;
  }
  const std::optional<WGPUIndexFormat> wgpuFormat = indexFormatToWGPUIndexFormat(format);
  if (!wgpuFormat) {
    IGL_LOG_ERROR_ONCE("WebGPU has no 8-bit indices\n");
    indexFormat_.reset();
    return;
  }
  auto& wgpuBuffer = static_cast<Buffer&>(buffer);
  wgpuRenderPassEncoderSetIndexBuffer(
      pass_.get(), wgpuBuffer.getWGPUBuffer(), *wgpuFormat, bufferOffset, WGPU_WHOLE_SIZE);
  wgpuBuffer.recordUse(commandBuffer_.getSerial(), /*gpuWrite=*/false);
  indexFormat_ = format;
}

void RenderCommandEncoder::bindBytes(size_t index,
                                     uint8_t /*bindTarget*/,
                                     const void* data,
                                     size_t length) {
  if (data == nullptr || length == 0 || index >= IGL_BUFFER_BINDINGS_MAX) {
    return;
  }
  const UniformArena::Slice slice = commandBuffer_.getUniformArena().allocate(data, length);
  if (slice.buffer == nullptr) {
    if (length > UniformArena::kMaxAllocationSize) {
      IGL_LOG_ERROR("bindBytes(): %zu bytes is more than WebGPU's %zu\n",
                    length,
                    UniformArena::kMaxAllocationSize);
    } else {
      IGL_LOG_ERROR("bindBytes(): cannot allocate uniform memory for %zu bytes\n", length);
    }
    return;
  }
  binder_.bindBuffer(static_cast<uint32_t>(index), slice.buffer, slice.offset, slice.size);
}

void RenderCommandEncoder::bindPushConstants(const void* data, size_t length, size_t offset) {
  const Result result = binder_.updatePushConstants(data, length, offset);
  if (!result.isOk()) {
    IGL_LOG_ERROR("bindPushConstants(): %s\n", result.message.c_str());
  }
}

void RenderCommandEncoder::bindSamplerState(size_t index,
                                            uint8_t /*target*/,
                                            ISamplerState* IGL_NULLABLE samplerState) {
  binder_.bindSampler(static_cast<uint32_t>(index), static_cast<SamplerState*>(samplerState));
}

void RenderCommandEncoder::bindTexture(size_t index,
                                       uint8_t /*target*/,
                                       ITexture* IGL_NULLABLE texture) {
  bindTexture(index, texture);
}

void RenderCommandEncoder::bindTexture(size_t index, ITexture* IGL_NULLABLE texture) {
  if (index >= kMaxTextureUnits) {
    IGL_LOG_ERROR("bindTexture(): WebGPU has %u texture units\n", kMaxTextureUnits);
    return;
  }
  textures_[index] = static_cast<Texture*>(texture);
  binder_.bindTexture(static_cast<uint32_t>(index), textures_[index]);
}

void RenderCommandEncoder::bindUniform(const UniformDesc& /*uniformDesc*/, const void* /*data*/) {
  IGL_LOG_ERROR_ONCE("bindUniform() is OpenGL-only; use uniform buffers on WebGPU\n");
}

void RenderCommandEncoder::bindBindGroup(BindGroupTextureHandle handle) {
  if (handle.empty()) {
    return;
  }
  const BindGroupTextureDesc* desc = device_.getBindGroupTextureDesc(handle);
  for (uint32_t i = 0; i != IGL_TEXTURE_SAMPLERS_MAX; i++) {
    if (desc->textures[i]) {
      bindTexture(i, desc->textures[i].get());
      bindSamplerState(i, BindTarget::kAllGraphics, desc->samplers[i].get());
    }
  }
}

void RenderCommandEncoder::bindBindGroup(BindGroupBufferHandle handle,
                                         uint32_t numDynamicOffsets,
                                         const uint32_t* IGL_NULLABLE dynamicOffsets) {
  if (handle.empty()) {
    return;
  }
  const BindGroupBufferDesc* desc = device_.getBindGroupBufferDesc(handle);
  uint32_t dynamicOffset = 0;
  for (uint32_t i = 0; i != IGL_UNIFORM_BLOCKS_BINDING_MAX; i++) {
    if (!desc->buffers[i]) {
      continue;
    }
    size_t offset = desc->offset[i];
    if ((desc->isDynamicBufferMask & (1u << i)) != 0) {
      IGL_DEBUG_ASSERT(dynamicOffsets && dynamicOffset < numDynamicOffsets,
                       "Not enough dynamic offsets provided");
      if (dynamicOffsets == nullptr || dynamicOffset >= numDynamicOffsets) {
        IGL_LOG_ERROR("bindBindGroup(): no dynamic offset for buffer %u; it is not bound\n", i);
        continue;
      }
      offset += dynamicOffsets[dynamicOffset++];
    }
    bindBuffer(i, desc->buffers[i].get(), offset, desc->size[i]);
  }
}

Result RenderCommandEncoder::checkSampledAttachments() const {
  for (const Texture* texture : textures_) {
    if (texture == nullptr) {
      continue;
    }
    for (const Attachment& attachment : attachments_) {
      const uint32_t numLayers = texture->getType() == TextureType::Cube
                                     ? texture->getNumLayers() * 6
                                     : texture->getNumLayers();
      const bool mipOverlaps =
          attachment.mipLevel >= texture->getBaseMipLevel() &&
          attachment.mipLevel < texture->getBaseMipLevel() + texture->getNumMipLevels();
      const bool layerOverlaps = texture->getType() == TextureType::ThreeD ||
                                 (attachment.layer >= texture->getBaseLayer() &&
                                  attachment.layer < texture->getBaseLayer() + numLayers);
      if (attachment.texture == texture->getWGPUTexture() && mipOverlaps && layerOverlaps) {
        return Result(Result::Code::ArgumentInvalid,
                      "A texture is sampled and attached in the same render pass");
      }
    }
  }
  return Result();
}

bool RenderCommandEncoder::prepareDraw(bool indexed) {
  if (!pass_) {
    IGL_LOG_ERROR_ONCE("Draw after endEncoding()\n");
    return false;
  }
  if (!pipeline_) {
    IGL_LOG_ERROR_ONCE("Draw without a render pipeline\n");
    return false;
  }
  if ((pipeline_->getRequiredVertexBufferMask() & ~boundVertexBuffers_) != 0) {
    IGL_LOG_ERROR_ONCE("Draw without all the vertex buffers the pipeline reads\n");
    return false;
  }
  if (indexed && !indexFormat_) {
    IGL_LOG_ERROR_ONCE("Indexed draw without an index buffer\n");
    return false;
  }
  SampleClasses classes = 0;
  Result result = checkSampledAttachments();
  if (result.isOk()) {
    result = binder_.getSampleClasses(*pipeline_, classes);
  }
  if (!result.isOk()) {
    IGL_LOG_ERROR("Draw skipped: %s\n", result.message.c_str());
    return false;
  }

  const RenderPipelineDesc& desc = pipeline_->getRenderPipelineDesc();
  RenderPipelineDynamicState state = pipeline_->getDefaultDynamicState();
  if (pipeline_->hasDepthStencilAttachment()) {
    state.setDepthStencilState(depthStencilState_);
  }
  state.cullMode = static_cast<uint8_t>(cullMode_.value_or(desc.cullMode));
  state.frontFaceWinding = static_cast<uint8_t>(frontFaceWinding_.value_or(desc.frontFaceWinding));
  state.depthBias = depthBias_;
  state.depthBiasSlopeScale = depthBiasSlopeScale_;
  state.depthBiasClamp = depthBiasClamp_;
  state.stripIndexFormat = indexed && isStrip(desc.topology)
                               ? static_cast<uint8_t>(static_cast<uint8_t>(*indexFormat_) + 1)
                               : 0;
  state.sampleClasses = classes;

  WGPURenderPipeline pipeline = pipeline_->getPipeline(state, &result);
  if (pipeline == nullptr) {
    IGL_LOG_ERROR("Draw skipped: %s\n", result.message.c_str());
    return false;
  }
  if (pipeline != boundPipeline_) {
    wgpuRenderPassEncoderSetPipeline(pass_.get(), pipeline);
    boundPipeline_ = pipeline;
  }
  binder_.stagePushConstants(commandBuffer_.getUniformArena());
  result = binder_.flush(pass_.get(), *pipeline_, classes, commandBuffer_.getSerial());
  if (!result.isOk()) {
    IGL_LOG_ERROR("Draw skipped: %s\n", result.message.c_str());
    return false;
  }
  return true;
}

void RenderCommandEncoder::draw(size_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t firstVertex,
                                uint32_t baseInstance) {
  IGL_PROFILER_FUNCTION();
  commandBuffer_.incrementCurrentDrawCount();
  if (vertexCount == 0 || instanceCount == 0 || !prepareDraw(/*indexed=*/false)) {
    return;
  }
  wgpuRenderPassEncoderDraw(
      pass_.get(), static_cast<uint32_t>(vertexCount), instanceCount, firstVertex, baseInstance);
}

void RenderCommandEncoder::drawIndexed(size_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t vertexOffset,
                                       uint32_t baseInstance) {
  IGL_PROFILER_FUNCTION();
  commandBuffer_.incrementCurrentDrawCount();
  if (indexCount == 0 || instanceCount == 0 || !prepareDraw(/*indexed=*/true)) {
    return;
  }
  wgpuRenderPassEncoderDrawIndexed(pass_.get(),
                                   static_cast<uint32_t>(indexCount),
                                   instanceCount,
                                   firstIndex,
                                   vertexOffset,
                                   baseInstance);
}

void RenderCommandEncoder::drawMeshTasks(const Dimensions& /*threadgroupsPerGrid*/,
                                         const Dimensions& /*threadsPerTaskThreadgroup*/,
                                         const Dimensions& /*threadsPerMeshThreadgroup*/) {
  IGL_LOG_ERROR_ONCE("WebGPU has no mesh shaders\n");
}

void RenderCommandEncoder::multiDrawIndirect(IBuffer& indirectBuffer,
                                             size_t indirectBufferOffset,
                                             uint32_t drawCount,
                                             uint32_t stride) {
  IGL_PROFILER_FUNCTION();
  drawIndirect(indirectBuffer, indirectBufferOffset, drawCount, stride, /*indexed=*/false);
}

void RenderCommandEncoder::multiDrawIndexedIndirect(IBuffer& indirectBuffer,
                                                    size_t indirectBufferOffset,
                                                    uint32_t drawCount,
                                                    uint32_t stride) {
  IGL_PROFILER_FUNCTION();
  drawIndirect(indirectBuffer, indirectBufferOffset, drawCount, stride, /*indexed=*/true);
}

void RenderCommandEncoder::drawIndirect(IBuffer& indirectBuffer,
                                        size_t indirectBufferOffset,
                                        uint32_t drawCount,
                                        uint32_t stride,
                                        bool indexed) {
  // WebGPU has no core multi-draw; each record becomes one draw{Indexed}Indirect call.
  const uint32_t recordSize = indexed ? kDrawIndexedIndirectSize : kDrawIndirectSize;
  stride = stride != 0 ? stride : recordSize;
  // Every draw call counts, issued or not, as on the other backends.
  for (uint32_t i = 0; i < drawCount; ++i) {
    commandBuffer_.incrementCurrentDrawCount();
  }
  if (drawCount == 0) {
    return;
  }
  auto& buffer = static_cast<Buffer&>(indirectBuffer);
  if ((buffer.getBufferType() & BufferDesc::BufferTypeBits::Indirect) == 0 ||
      indirectBufferOffset % 4 != 0 || stride % 4 != 0) {
    IGL_LOG_ERROR("Indirect draws need an Indirect buffer, and 4-byte offset and stride\n");
    return;
  }
  // 64-bit, so the bounds check cannot wrap on wasm32.
  if (uint64_t{indirectBufferOffset} + uint64_t{stride} * (drawCount - 1) + recordSize >
      buffer.getSizeInBytes()) {
    IGL_LOG_ERROR("Indirect draw records extend past the end of the buffer\n");
    return;
  }
  if (!prepareDraw(indexed)) {
    return;
  }
  for (uint32_t i = 0; i < drawCount; ++i) {
    const uint64_t offset = indirectBufferOffset + static_cast<uint64_t>(stride) * i;
    if (indexed) {
      wgpuRenderPassEncoderDrawIndexedIndirect(pass_.get(), buffer.getWGPUBuffer(), offset);
    } else {
      wgpuRenderPassEncoderDrawIndirect(pass_.get(), buffer.getWGPUBuffer(), offset);
    }
  }
  buffer.recordUse(commandBuffer_.getSerial(), /*gpuWrite=*/false);
}

void RenderCommandEncoder::setStencilReferenceValue(uint32_t value) {
  if (pass_) {
    wgpuRenderPassEncoderSetStencilReference(pass_.get(), value);
  }
}

void RenderCommandEncoder::setBlendColor(const Color& color) {
  if (pass_) {
    const WGPUColor wgpuColor = {.r = color.r, .g = color.g, .b = color.b, .a = color.a};
    wgpuRenderPassEncoderSetBlendConstant(pass_.get(), &wgpuColor);
  }
}

void RenderCommandEncoder::setCullMode(CullMode cullMode) {
  cullMode_ = cullMode;
}

void RenderCommandEncoder::setDepthBias(float depthBias, float slopeScale, float clamp) {
  depthBias_ = depthBias;
  depthBiasSlopeScale_ = slopeScale;
  depthBiasClamp_ = clamp;
}

void RenderCommandEncoder::setFrontFacingWinding(WindingMode frontFaceWinding) {
  frontFaceWinding_ = frontFaceWinding;
}

} // namespace igl::webgpu
