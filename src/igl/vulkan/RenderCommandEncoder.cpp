/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/RenderCommandEncoder.h>

#include <algorithm>

#include <igl/RenderPass.h>
#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/DepthStencilState.h>
#include <igl/vulkan/Framebuffer.h>
#include <igl/vulkan/RenderPipelineState.h>
#include <igl/vulkan/SamplerState.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VertexInputState.h>
#include <igl/vulkan/VulkanBuffer.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanPipelineLayout.h>
#include <igl/vulkan/VulkanRenderPassBuilder.h>
#include <igl/vulkan/VulkanShaderModule.h>
#include <igl/vulkan/VulkanSwapchain.h>
#include <igl/vulkan/util/SpvReflection.h>

namespace {

VkAttachmentLoadOp loadActionToVkAttachmentLoadOp(igl::LoadAction a) {
  using igl::LoadAction;
  switch (a) {
  case LoadAction::DontCare:
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  case LoadAction::Load:
    return VK_ATTACHMENT_LOAD_OP_LOAD;
  case LoadAction::Clear:
    return VK_ATTACHMENT_LOAD_OP_CLEAR;
  }
  IGL_ASSERT(false);
  return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

VkAttachmentStoreOp storeActionToVkAttachmentStoreOp(igl::StoreAction a) {
  using igl::StoreAction;
  switch (a) {
  case StoreAction::DontCare:
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
  case StoreAction::Store:
    return VK_ATTACHMENT_STORE_OP_STORE;
  case StoreAction::MsaaResolve:
    // for MSAA resolve, we have to store data into a special "resolve" attachment
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
  }
  IGL_ASSERT(false);
  return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

VkStencilOp stencilOperationToVkStencilOp(igl::StencilOperation op) {
  switch (op) {
  case igl::StencilOperation::Keep:
    return VK_STENCIL_OP_KEEP;
  case igl::StencilOperation::Zero:
    return VK_STENCIL_OP_ZERO;
  case igl::StencilOperation::Replace:
    return VK_STENCIL_OP_REPLACE;
  case igl::StencilOperation::IncrementClamp:
    return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
  case igl::StencilOperation::DecrementClamp:
    return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
  case igl::StencilOperation::Invert:
    return VK_STENCIL_OP_INVERT;
  case igl::StencilOperation::IncrementWrap:
    return VK_STENCIL_OP_INCREMENT_AND_WRAP;
  case igl::StencilOperation::DecrementWrap:
    return VK_STENCIL_OP_DECREMENT_AND_WRAP;
  }
  IGL_ASSERT(false);
  return VK_STENCIL_OP_KEEP;
}

VkIndexType indexFormatToVkIndexType(igl::IndexFormat fmt) {
  switch (fmt) {
  case igl::IndexFormat::UInt16:
    return VK_INDEX_TYPE_UINT16;
  case igl::IndexFormat::UInt32:
    return VK_INDEX_TYPE_UINT32;
  };
  IGL_ASSERT(false);
  return VK_INDEX_TYPE_NONE_KHR;
}

VkPrimitiveTopology primitiveTypeToVkPrimitiveTopology(igl::PrimitiveType t) {
  switch (t) {
  case igl::PrimitiveType::Point:
    return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  case igl::PrimitiveType::Line:
    return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  case igl::PrimitiveType::LineStrip:
    return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
  case igl::PrimitiveType::Triangle:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  case igl::PrimitiveType::TriangleStrip:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  }
  IGL_ASSERT_MSG(false, "Implement PrimitiveType = %u", (uint32_t)t);
  return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

} // namespace

namespace igl {
namespace vulkan {

RenderCommandEncoder::RenderCommandEncoder(const std::shared_ptr<CommandBuffer>& commandBuffer,
                                           VulkanContext& ctx) :
  IRenderCommandEncoder::IRenderCommandEncoder(commandBuffer),
  ctx_(ctx),
  cmdBuffer_(commandBuffer ? commandBuffer->getVkCommandBuffer() : VK_NULL_HANDLE),
  binder_(commandBuffer, ctx, VK_PIPELINE_BIND_POINT_GRAPHICS) {
  IGL_PROFILER_FUNCTION();
  IGL_ASSERT(commandBuffer);
  IGL_ASSERT(cmdBuffer_ != VK_NULL_HANDLE);
}

void RenderCommandEncoder::initialize(const RenderPassDesc& renderPass,
                                      const std::shared_ptr<IFramebuffer>& framebuffer,
                                      const Dependencies& dependencies,
                                      Result* outResult) {
  IGL_PROFILER_FUNCTION();

  processDependencies(dependencies);

  framebuffer_ = framebuffer;
  dependencies_ = dependencies;

  Result::setOk(outResult);

  if (!IGL_VERIFY(framebuffer)) {
    Result::setResult(outResult, Result::Code::ArgumentNull);
    return;
  }

  const FramebufferDesc& desc = static_cast<const Framebuffer&>((*framebuffer)).getDesc();

  std::vector<VkClearValue> clearValues;
  uint32_t mipLevel = 0;
  uint32_t layer = 0;

  VulkanRenderPassBuilder builder;

  if (desc.mode != FramebufferMode::Mono) {
    if (desc.mode == FramebufferMode::Stereo) {
      builder.setMultiviewMasks(0x00000003, 0x00000003);
    } else {
      IGL_ASSERT_MSG(0, "FramebufferMode::Multiview is not implemented.");
    }
  }

  for (size_t i = 0; i != IGL_COLOR_ATTACHMENTS_MAX; i++) {
    const auto& attachment = desc.colorAttachments[i];
    if (!attachment.texture) {
      continue;
    }

    const auto& colorTexture = static_cast<vulkan::Texture&>(*attachment.texture);

    // Specifically using renderPass.colorAttachments.size() in case we somehow
    // get into this loop even when renderPass.colorAttachments.empty() == true
    if (i >= renderPass.colorAttachments.size()) {
      IGL_ASSERT(false);
      Result::setResult(
          outResult,
          Result::Code::ArgumentInvalid,
          "Framebuffer color attachment count larger than renderPass color attachment count");
      return;
    }

    const auto& descColor = renderPass.colorAttachments[i];
    clearValues.push_back(ivkGetClearColorValue(descColor.clearColor.r,
                                                descColor.clearColor.g,
                                                descColor.clearColor.b,
                                                descColor.clearColor.a));
    const auto colorLayer = getVkLayer(colorTexture.getType(), descColor.face, descColor.layer);
    if (mipLevel) {
      IGL_ASSERT_MSG(descColor.mipLevel == mipLevel,
                     "All color attachments should have the same mip-level");
    }
    if (layer) {
      IGL_ASSERT_MSG(colorLayer == layer,
                     "All color attachments should have the same face or layer");
    }
    mipLevel = descColor.mipLevel;
    layer = colorLayer;
    const auto initialLayout = descColor.loadAction == igl::LoadAction::Load
                                   ? colorTexture.getVulkanTexture().getVulkanImage().imageLayout_
                                   : VK_IMAGE_LAYOUT_UNDEFINED;
    builder.addColor(textureFormatToVkFormat(colorTexture.getFormat()),
                     loadActionToVkAttachmentLoadOp(descColor.loadAction),
                     storeActionToVkAttachmentStoreOp(descColor.storeAction),
                     initialLayout,
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                     colorTexture.getVulkanTexture().getVulkanImage().samples_);
    // handle MSAA
    if (descColor.storeAction == StoreAction::MsaaResolve) {
      IGL_ASSERT_MSG(attachment.resolveTexture,
                     "Framebuffer attachment should contain a resolve texture");
      const auto& colorResolveTexture = static_cast<vulkan::Texture&>(*attachment.resolveTexture);
      builder.addColorResolve(textureFormatToVkFormat(colorResolveTexture.getFormat()),
                              VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                              VK_ATTACHMENT_STORE_OP_STORE);
      clearValues.push_back(ivkGetClearColorValue(descColor.clearColor.r,
                                                  descColor.clearColor.g,
                                                  descColor.clearColor.b,
                                                  descColor.clearColor.a));
    }
  }

  // Process depth attachment
  const RenderPassDesc::DepthAttachmentDesc descDepth = renderPass.depthAttachment;
  const RenderPassDesc::StencilAttachmentDesc descStencil = renderPass.stencilAttachment;
  hasDepthAttachment_ = false;

  if (framebuffer->getDepthAttachment()) {
    const auto& depthTexture = static_cast<vulkan::Texture&>(*(framebuffer->getDepthAttachment()));
    hasDepthAttachment_ = true;
    IGL_ASSERT_MSG(descDepth.mipLevel == mipLevel,
                   "Depth attachment should have the same mip-level as color attachments");
    IGL_ASSERT_MSG(getVkLayer(depthTexture.getType(), descDepth.face, descDepth.layer) == layer,
                   "Depth attachment should have the same face or layer as color attachments");
    clearValues.push_back(
        ivkGetClearDepthStencilValue(descDepth.clearDepth, descStencil.clearStencil));
    const auto initialLayout = descDepth.loadAction == igl::LoadAction::Load
                                   ? depthTexture.getVulkanTexture().getVulkanImage().imageLayout_
                                   : VK_IMAGE_LAYOUT_UNDEFINED;
    builder.addDepthStencil(depthTexture.getVkFormat(),
                            loadActionToVkAttachmentLoadOp(descDepth.loadAction),
                            storeActionToVkAttachmentStoreOp(descDepth.storeAction),
                            loadActionToVkAttachmentLoadOp(descStencil.loadAction),
                            storeActionToVkAttachmentStoreOp(descStencil.storeAction),
                            initialLayout,
                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                            depthTexture.getVulkanTexture().getVulkanImage().samples_);
  }

  const auto& fb = static_cast<vulkan::Framebuffer&>(*framebuffer);

  auto renderPassHandle = ctx_.findRenderPass(builder);

  dynamicState_.renderPassIndex_ = renderPassHandle.index;
  dynamicState_.depthBiasEnable_ = false;

  const VkRenderPassBeginInfo bi = fb.getRenderPassBeginInfo(
      renderPassHandle.pass, mipLevel, layer, (uint32_t)clearValues.size(), clearValues.data());

  const uint32_t width = std::max(fb.getWidth() >> mipLevel, 1u);
  const uint32_t height = std::max(fb.getHeight() >> mipLevel, 1u);
  const igl::Viewport viewport = {0.0f, 0.0f, (float)width, (float)height, 0.0f, +1.0f};
  const igl::ScissorRect scissor = {0, 0, width, height};

  bindViewport(viewport);
  bindScissorRect(scissor);

  ctx_.checkAndUpdateDescriptorSets();

  ctx_.vf_.vkCmdBeginRenderPass(cmdBuffer_, &bi, VK_SUBPASS_CONTENTS_INLINE);

  isEncoding_ = true;

  Result::setOk(outResult);
}

std::unique_ptr<RenderCommandEncoder> RenderCommandEncoder::create(
    const std::shared_ptr<CommandBuffer>& commandBuffer,
    VulkanContext& ctx,
    const RenderPassDesc& renderPass,
    const std::shared_ptr<IFramebuffer>& framebuffer,
    const Dependencies& dependencies,
    Result* outResult) {
  IGL_PROFILER_FUNCTION();

  Result ret;

  std::unique_ptr<RenderCommandEncoder> encoder(new RenderCommandEncoder(commandBuffer, ctx));
  encoder->initialize(renderPass, framebuffer, dependencies, &ret);

  Result::setResult(outResult, ret);
  return ret.isOk() ? std::move(encoder) : nullptr;
}

void RenderCommandEncoder::endEncoding() {
  IGL_PROFILER_FUNCTION();

  if (!isEncoding_) {
    return;
  }

  isEncoding_ = false;

  ctx_.vf_.vkCmdEndRenderPass(cmdBuffer_);

  for (ITexture* IGL_NULLABLE tex : dependencies_.textures) {
    // TODO: at some point we might want to know in which layout a dependent texture wants to be. We
    // can implement that by adding a notion of image layouts to IGL.
    if (!tex) {
      continue;
    }

    // Retrieve the VulkanImage to check its usage
    const auto& vkTex = static_cast<Texture&>(*tex);
    const auto& img = vkTex.getVulkanTexture().getVulkanImage();

    if (tex->getProperties().isDepthOrStencil()) {
      // If the texture has not been marked as a depth/stencil attachment
      // (TextureDesc::TextureUsageBits::Attachment), don't transition it to a depth/stencil
      // attchment
      if (img.usageFlags_ & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        transitionToDepthStencilAttachment(cmdBuffer_, tex);
      }
    } else {
      // If the texture has not been marked as a color attachment
      // (TextureDesc::TextureUsageBits::Attachment), don't transition it to a color attchment
      if (img.usageFlags_ & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
        transitionToColorAttachment(cmdBuffer_, tex);
      }
    }
  }
  dependencies_ = {};

  // set image layouts after the render pass
  const FramebufferDesc& desc = static_cast<const Framebuffer&>((*framebuffer_)).getDesc();

  for (const auto& attachment : desc.colorAttachments) {
    // the image layouts of color attachments must match the final layout of the render pass, which
    // is always VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL (check VulkanRenderPassBuilder.cpp)
    overrideImageLayout(attachment.texture.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    overrideImageLayout(attachment.resolveTexture.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    transitionToShaderReadOnly(cmdBuffer_, attachment.texture.get());
    transitionToShaderReadOnly(cmdBuffer_, attachment.resolveTexture.get());
  }

  // this must match the final layout of the render pass, which is always
  // VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL (check VulkanRenderPassBuilder.cpp)
  overrideImageLayout(desc.depthAttachment.texture.get(),
                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  transitionToShaderReadOnly(cmdBuffer_, desc.depthAttachment.texture.get());

#if defined(IGL_WITH_TRACY_GPU)
  TracyVkCollect(ctx_.tracyCtx_, cmdBuffer_);
#endif
}

void RenderCommandEncoder::pushDebugGroupLabel(const char* label, const igl::Color& color) const {
  IGL_ASSERT(label != nullptr && *label);
  ivkCmdBeginDebugUtilsLabel(&ctx_.vf_, cmdBuffer_, label, color.toFloatPtr());
}

void RenderCommandEncoder::insertDebugEventLabel(const char* label, const igl::Color& color) const {
  IGL_ASSERT(label != nullptr && *label);
  ivkCmdInsertDebugUtilsLabel(&ctx_.vf_, cmdBuffer_, label, color.toFloatPtr());
}

void RenderCommandEncoder::popDebugGroupLabel() const {
  ivkCmdEndDebugUtilsLabel(&ctx_.vf_, cmdBuffer_);
}

void RenderCommandEncoder::bindViewport(const Viewport& viewport) {
  IGL_PROFILER_FUNCTION();
  IGL_PROFILER_ZONE_GPU_VK("bindViewport()", ctx_.tracyCtx_, cmdBuffer_);

  /**
    Using the negative viewport height Vulkan feature, we make the Vulkan "top-left" coordinate
  system to be "bottom-left" as in OpenGL. This way VK_FRONT_FACE_COUNTER_CLOCKWISE and
  VK_FRONT_FACE_CLOCKWISE use the same winding as in OpenGL. Part of VK_KHR_maintenance1 which is
  promoted to Vulkan 1.1.

  More details: https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
  **/
  const VkViewport vp = {
      viewport.x, // float x;
      viewport.height - viewport.y, // float y;
      viewport.width, // float width;
      -viewport.height, // float height;
      viewport.minDepth, // float minDepth;
      viewport.maxDepth, // float maxDepth;
  };
  ctx_.vf_.vkCmdSetViewport(cmdBuffer_, 0, 1, &vp);
}

void RenderCommandEncoder::bindScissorRect(const ScissorRect& rect) {
  const VkRect2D scissor = {
      VkOffset2D{(int32_t)rect.x, (int32_t)rect.y},
      VkExtent2D{rect.width, rect.height},
  };
  ctx_.vf_.vkCmdSetScissor(cmdBuffer_, 0, 1, &scissor);
}

void RenderCommandEncoder::bindRenderPipelineState(
    const std::shared_ptr<IRenderPipelineState>& pipelineState) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_VERIFY(pipelineState != nullptr)) {
    return;
  }

  rps_ = static_cast<igl::vulkan::RenderPipelineState*>(pipelineState.get());

  IGL_ASSERT(rps_);

  const RenderPipelineDesc& desc = rps_->getRenderPipelineDesc();

  ensureShaderModule(desc.shaderStages->getVertexModule().get());
  ensureShaderModule(desc.shaderStages->getFragmentModule().get());

  const bool hasDepthAttachment = desc.targetDesc.depthAttachmentFormat != TextureFormat::Invalid;

  if (hasDepthAttachment != hasDepthAttachment_) {
    IGL_ASSERT(false);
    IGL_LOG_ERROR(
        "Make sure your render pass and render pipeline both have matching depth attachments");
  }

  binder_.bindPipeline(VK_NULL_HANDLE, nullptr);
}

void RenderCommandEncoder::bindDepthStencilState(
    const std::shared_ptr<IDepthStencilState>& depthStencilState) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_VERIFY(depthStencilState != nullptr)) {
    return;
  }
  const igl::vulkan::DepthStencilState* state =
      static_cast<igl::vulkan::DepthStencilState*>(depthStencilState.get());

  const igl::DepthStencilStateDesc& desc = state->getDepthStencilStateDesc();

  dynamicState_.depthWriteEnable_ = desc.isDepthWriteEnabled;
  dynamicState_.setDepthCompareOp(compareFunctionToVkCompareOp(desc.compareFunction));

  auto setStencilState = [this](VkStencilFaceFlagBits faceMask, const igl::StencilStateDesc& desc) {
    if (desc == igl::StencilStateDesc()) {
      // do not update anything if we don't have an actual state
      return;
    }
    dynamicState_.setStencilStateOps(faceMask,
                                     stencilOperationToVkStencilOp(desc.stencilFailureOperation),
                                     stencilOperationToVkStencilOp(desc.depthStencilPassOperation),
                                     stencilOperationToVkStencilOp(desc.depthFailureOperation),
                                     compareFunctionToVkCompareOp(desc.stencilCompareFunction));
    // this is what the IGL/OGL backend does with masks
    ctx_.vf_.vkCmdSetStencilReference(cmdBuffer_, faceMask, desc.readMask);
    ctx_.vf_.vkCmdSetStencilCompareMask(cmdBuffer_, faceMask, 0xFF);
    ctx_.vf_.vkCmdSetStencilWriteMask(cmdBuffer_, faceMask, desc.writeMask);
  };

  setStencilState(VK_STENCIL_FACE_FRONT_BIT, desc.frontFaceStencil);
  setStencilState(VK_STENCIL_FACE_BACK_BIT, desc.backFaceStencil);
}

void RenderCommandEncoder::bindBuffer(int index,
                                      const std::shared_ptr<IBuffer>& buffer,
                                      size_t bufferOffset,
                                      size_t bufferSize) {
  IGL_PROFILER_FUNCTION();
  IGL_PROFILER_ZONE_GPU_VK("bindBuffer()", ctx_.tracyCtx_, cmdBuffer_);

#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("%p  bindBuffer(%i, %u)\n", cmdBuffer_, index, (uint32_t)bufferOffset);
#endif // IGL_VULKAN_PRINT_COMMANDS

  if (!IGL_VERIFY(buffer != nullptr)) {
    return;
  }

  auto* buf = static_cast<igl::vulkan::Buffer*>(buffer.get());

  const bool isUniformBuffer = (buf->getBufferType() & BufferDesc::BufferTypeBits::Uniform) > 0;
  const bool isStorageBuffer = (buf->getBufferType() & BufferDesc::BufferTypeBits::Storage) > 0;
  const bool isUniformOrStorageBuffer = isUniformBuffer || isStorageBuffer;

  IGL_ASSERT_MSG(isUniformOrStorageBuffer, "Must be a uniform or a storage buffer");

  if (!IGL_VERIFY(isUniformOrStorageBuffer)) {
    return;
  }
  if (isUniformBuffer) {
    binder_.bindUniformBuffer(index, buf, bufferOffset, bufferSize);
  }
  if (isStorageBuffer) {
    if (ctx_.enhancedShaderDebuggingStore_) {
      IGL_ASSERT_MSG(index < (IGL_UNIFORM_BLOCKS_BINDING_MAX - 1),
                     "The last buffer index is reserved for enhanced debugging features");
    }
    binder_.bindStorageBuffer(index, buf, bufferOffset, bufferSize);
  }
}

void RenderCommandEncoder::bindVertexBuffer(uint32_t index, IBuffer& buffer, size_t bufferOffset) {
  IGL_PROFILER_FUNCTION();
  IGL_PROFILER_ZONE_GPU_VK("bindVertexBuffer()", ctx_.tracyCtx_, cmdBuffer_);

#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO(
      "%p  bindVertexBuffer(%u, %p, %u)\n", cmdBuffer_, index, &buffer, (uint32_t)bufferOffset);
#endif // IGL_VULKAN_PRINT_COMMANDS

  const bool isVertexBuffer = (buffer.getBufferType() & BufferDesc::BufferTypeBits::Vertex) != 0;

  if (!IGL_VERIFY(isVertexBuffer)) {
    return;
  }

  if (IGL_VERIFY(index < IGL_ARRAY_NUM_ELEMENTS(isVertexBufferBound_))) {
    isVertexBufferBound_[index] = true;
  }
  VkBuffer vkBuf = static_cast<igl::vulkan::Buffer&>(buffer).getVkBuffer();
  const VkDeviceSize offset = bufferOffset;
  ctx_.vf_.vkCmdBindVertexBuffers(cmdBuffer_, index, 1, &vkBuf, &offset);
}

void RenderCommandEncoder::bindIndexBuffer(IBuffer& buffer,
                                           IndexFormat format,
                                           size_t bufferOffset) {
  const auto& buf = static_cast<igl::vulkan::Buffer&>(buffer);

  IGL_ASSERT_MSG(buf.getBufferUsageFlags() & VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 "Did you forget to specify BufferTypeBits::Index on your buffer?");

  const VkIndexType type = indexFormatToVkIndexType(format);

  ctx_.vf_.vkCmdBindIndexBuffer(cmdBuffer_, buf.getVkBuffer(), bufferOffset, type);
}

void RenderCommandEncoder::bindBytes(size_t /*index*/,
                                     uint8_t /*target*/,
                                     const void* /*data*/,
                                     size_t /*length*/) {
  IGL_ASSERT_NOT_IMPLEMENTED();
}

void RenderCommandEncoder::bindPushConstants(const void* data, size_t length, size_t offset) {
  IGL_PROFILER_FUNCTION();
  IGL_PROFILER_ZONE_GPU_VK("bindPushConstants()", ctx_.tracyCtx_, cmdBuffer_);

  IGL_ASSERT(length % 4 == 0); // VUID-vkCmdPushConstants-size-00369: size must be a multiple of 4

  IGL_ASSERT_MSG(rps_, "Did you forget to call bindRenderPipelineState()?");
  IGL_ASSERT_MSG(rps_->pushConstantRange_.size,
                 "Currently bound render pipeline state has no push constants");
  IGL_ASSERT_MSG(offset + length <= rps_->pushConstantRange_.offset + rps_->pushConstantRange_.size,
                 "Push constants size exceeded");

  if (!rps_->pipelineLayout_) {
    // bring a pipeline layout into existence - we don't really care about the dynamic state here
    (void)rps_->getVkPipeline(dynamicState_);
  }

#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("%p vkCmdPushConstants(%u) - GRAPHICS\n", cmdBuffer_, length);
#endif // IGL_VULKAN_PRINT_COMMANDS
  ctx_.vf_.vkCmdPushConstants(cmdBuffer_,
                              rps_->getVkPipelineLayout(),
                              rps_->pushConstantRange_.stageFlags,
                              (uint32_t)offset,
                              (uint32_t)length,
                              data);
}

void RenderCommandEncoder::bindSamplerState(size_t index,
                                            uint8_t target,
                                            ISamplerState* samplerState) {
  IGL_PROFILER_FUNCTION();

#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("%p  bindSamplerState(%u, %u)\n", cmdBuffer_, (uint32_t)index, (uint32_t)target);
#endif // IGL_VULKAN_PRINT_COMMANDS

  if (!IGL_VERIFY(target == igl::BindTarget::kFragment || target == igl::BindTarget::kVertex ||
                  target == igl::BindTarget::kAllGraphics)) {
    IGL_ASSERT_MSG(false, "Invalid sampler target");
    return;
  }

  binder_.bindSamplerState(index, static_cast<igl::vulkan::SamplerState*>(samplerState));
}

void RenderCommandEncoder::bindTexture(size_t index, uint8_t target, ITexture* texture) {
  IGL_PROFILER_FUNCTION();

#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("%p  bindTexture(%u, %u)\n", cmdBuffer_, (uint32_t)index, (uint32_t)target);
#endif // IGL_VULKAN_PRINT_COMMANDS

  if (!IGL_VERIFY(target == igl::BindTarget::kFragment || target == igl::BindTarget::kVertex ||
                  target == igl::BindTarget::kAllGraphics)) {
    IGL_ASSERT_MSG(false, "Invalid texture target");
    return;
  }

  binder_.bindTexture(index, static_cast<igl::vulkan::Texture*>(texture));
}

void RenderCommandEncoder::bindUniform(const UniformDesc& /*uniformDesc*/, const void* /*data*/) {
  // DO NOT IMPLEMENT!
  // This is only for backends that MUST use single uniforms in some situations.
  IGL_ASSERT_NOT_IMPLEMENTED();
}

void RenderCommandEncoder::draw(PrimitiveType primitiveType,
                                size_t vertexStart,
                                size_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t baseInstance) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DRAW);
  IGL_PROFILER_ZONE_GPU_COLOR_VK("draw()", ctx_.tracyCtx_, cmdBuffer_, IGL_PROFILER_COLOR_DRAW);

  ctx_.drawCallCount_ += drawCallCountEnabled_;

  if (vertexCount == 0) {
    return;
  }

  IGL_ASSERT_MSG(rps_, "Did you forget to call bindRenderPipelineState()?");

  ensureVertexBuffers();

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  flushDynamicState();

#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("%p vkCmdDraw(%u, %u, %u, %u)\n",
               cmdBuffer_,
               (uint32_t)vertexCount,
               instanceCount,
               (uint32_t)vertexStart,
               baseInstance);
#endif // IGL_VULKAN_PRINT_COMMANDS

  ctx_.vf_.vkCmdDraw(
      cmdBuffer_, (uint32_t)vertexCount, instanceCount, (uint32_t)vertexStart, baseInstance);
}

void RenderCommandEncoder::draw(size_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t firstVertex,
                                uint32_t baseInstance) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DRAW);
  IGL_PROFILER_ZONE_GPU_COLOR_VK("draw()", ctx_.tracyCtx_, cmdBuffer_, IGL_PROFILER_COLOR_DRAW);

  ctx_.drawCallCount_ += drawCallCountEnabled_;

  if (vertexCount == 0) {
    // IGL/OpenGL tests rely on this behavior due to how state caching is organized over there.
    // If we do not return here, Validation Layers will complain.
    return;
  }

  IGL_ASSERT_MSG(rps_, "Did you forget to call bindRenderPipelineState()?");

  ensureVertexBuffers();

  dynamicState_.setTopology(
      primitiveTypeToVkPrimitiveTopology(rps_->getRenderPipelineDesc().topology));
  flushDynamicState();

#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("%p vkCmdDraw(%u, %u, %u, %u)\n",
               cmdBuffer_,
               (uint32_t)vertexCount,
               instanceCount,
               firstVertex,
               baseInstance);
#endif // IGL_VULKAN_PRINT_COMMANDS

  ctx_.vf_.vkCmdDraw(cmdBuffer_, (uint32_t)vertexCount, instanceCount, firstVertex, baseInstance);
}

void RenderCommandEncoder::drawIndexed(PrimitiveType primitiveType,
                                       size_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t vertexOffset,
                                       uint32_t baseInstance) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DRAW);
  IGL_PROFILER_ZONE_GPU_COLOR_VK(
      "drawIndexed()", ctx_.tracyCtx_, cmdBuffer_, IGL_PROFILER_COLOR_DRAW);

  ctx_.drawCallCount_ += drawCallCountEnabled_;

  if (indexCount == 0) {
    // IGL/OpenGL tests rely on this behavior due to how state caching is organized over there.
    // If we do not return here, Validation Layers will complain.
    return;
  }

  IGL_ASSERT_MSG(rps_, "Did you forget to call bindRenderPipelineState()?");

  ensureVertexBuffers();

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  flushDynamicState();

#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("%p vkCmdDrawIndexed(%u, %u, %u, %i, %u)\n",
               cmdBuffer_,
               (uint32_t)indexCount,
               instanceCount,
               firstIndex,
               vertexOffset,
               baseInstance);
#endif // IGL_VULKAN_PRINT_COMMANDS
  ctx_.vf_.vkCmdDrawIndexed(
      cmdBuffer_, (uint32_t)indexCount, instanceCount, firstIndex, vertexOffset, baseInstance);
}

void RenderCommandEncoder::drawIndexed(size_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t vertexOffset,
                                       uint32_t baseInstance) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DRAW);
  IGL_PROFILER_ZONE_GPU_COLOR_VK(
      "drawIndexed()", ctx_.tracyCtx_, cmdBuffer_, IGL_PROFILER_COLOR_DRAW);

  ctx_.drawCallCount_ += drawCallCountEnabled_;

  if (indexCount == 0) {
    // IGL/OpenGL tests rely on this behavior due to how state caching is organized over there.
    // If we do not return here, Validation Layers will complain.
    return;
  }

  IGL_ASSERT_MSG(rps_, "Did you forget to call bindRenderPipelineState()?");

  ensureVertexBuffers();

  dynamicState_.setTopology(
      primitiveTypeToVkPrimitiveTopology(rps_->getRenderPipelineDesc().topology));
  flushDynamicState();

#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("%p vkCmdDrawIndexed(%u, %u, %u, %i, %u)\n",
               cmdBuffer_,
               (uint32_t)indexCount,
               instanceCount,
               firstIndex,
               vertexOffset,
               baseInstance);
#endif // IGL_VULKAN_PRINT_COMMANDS
  ctx_.vf_.vkCmdDrawIndexed(
      cmdBuffer_, (uint32_t)indexCount, instanceCount, firstIndex, vertexOffset, baseInstance);
}

void RenderCommandEncoder::multiDrawIndirect(IBuffer& indirectBuffer,
                                             size_t indirectBufferOffset,
                                             uint32_t drawCount,
                                             uint32_t stride) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DRAW);
  IGL_PROFILER_ZONE_GPU_COLOR_VK(
      "multiDrawIndirect()", ctx_.tracyCtx_, cmdBuffer_, IGL_PROFILER_COLOR_DRAW);

  IGL_ASSERT_MSG(rps_, "Did you forget to call bindRenderPipelineState()?");

  ensureVertexBuffers();

  dynamicState_.setTopology(
      primitiveTypeToVkPrimitiveTopology(rps_->getRenderPipelineDesc().topology));
  flushDynamicState();

  ctx_.drawCallCount_ += drawCallCountEnabled_;

  const igl::vulkan::Buffer* bufIndirect = static_cast<igl::vulkan::Buffer*>(&indirectBuffer);

  ctx_.vf_.vkCmdDrawIndirect(cmdBuffer_,
                             bufIndirect->getVkBuffer(),
                             indirectBufferOffset,
                             drawCount,
                             stride ? stride : sizeof(VkDrawIndirectCommand));
}

void RenderCommandEncoder::multiDrawIndexedIndirect(IBuffer& indirectBuffer,
                                                    size_t indirectBufferOffset,
                                                    uint32_t drawCount,
                                                    uint32_t stride) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DRAW);
  IGL_PROFILER_ZONE_GPU_COLOR_VK(
      "multiDrawIndexedIndirect()", ctx_.tracyCtx_, cmdBuffer_, IGL_PROFILER_COLOR_DRAW);

  IGL_ASSERT_MSG(rps_, "Did you forget to call bindRenderPipelineState()?");

  ensureVertexBuffers();

  dynamicState_.setTopology(
      primitiveTypeToVkPrimitiveTopology(rps_->getRenderPipelineDesc().topology));
  flushDynamicState();

  ctx_.drawCallCount_ += drawCallCountEnabled_;

  const igl::vulkan::Buffer* bufIndirect = static_cast<igl::vulkan::Buffer*>(&indirectBuffer);

  ctx_.vf_.vkCmdDrawIndexedIndirect(cmdBuffer_,
                                    bufIndirect->getVkBuffer(),
                                    indirectBufferOffset,
                                    drawCount,
                                    stride ? stride : sizeof(VkDrawIndexedIndirectCommand));
}

void RenderCommandEncoder::setStencilReferenceValue(uint32_t value) {
  IGL_PROFILER_FUNCTION();

  setStencilReferenceValues(value, value);
}

void RenderCommandEncoder::setStencilReferenceValues(uint32_t frontValue, uint32_t backValue) {
  IGL_PROFILER_FUNCTION();

  ctx_.vf_.vkCmdSetStencilReference(cmdBuffer_, VK_STENCIL_FACE_FRONT_BIT, frontValue);
  ctx_.vf_.vkCmdSetStencilReference(cmdBuffer_, VK_STENCIL_FACE_BACK_BIT, backValue);
}

void RenderCommandEncoder::setBlendColor(Color color) {
  IGL_PROFILER_FUNCTION();

  ctx_.vf_.vkCmdSetBlendConstants(cmdBuffer_, color.toFloatPtr());
}

void RenderCommandEncoder::setDepthBias(float depthBias, float slopeScale, float clamp) {
  IGL_PROFILER_FUNCTION();

  dynamicState_.depthBiasEnable_ = true;
  ctx_.vf_.vkCmdSetDepthBias(cmdBuffer_, depthBias, clamp, slopeScale);
}

bool RenderCommandEncoder::setDrawCallCountEnabled(bool value) {
  IGL_PROFILER_FUNCTION();

  const auto returnVal = drawCallCountEnabled_ > 0;
  drawCallCountEnabled_ = value;
  return returnVal;
}

void RenderCommandEncoder::flushDynamicState() {
  binder_.bindPipeline(rps_->getVkPipeline(dynamicState_), &rps_->getSpvModuleInfo());
  binder_.updateBindings(rps_->getVkPipelineLayout(), *rps_);

  if (ctx_.config_.enableDescriptorIndexing) {
    VkDescriptorSet dset = ctx_.getBindlessVkDescriptorSet();

#if IGL_VULKAN_PRINT_COMMANDS
    IGL_LOG_INFO("%p vkCmdBindDescriptorSets(GRAPHICS) - bindless\n", cmdBuffer_);
#endif // IGL_VULKAN_PRINT_COMMANDS
    ctx_.vf_.vkCmdBindDescriptorSets(cmdBuffer_,
                                     VK_PIPELINE_BIND_POINT_GRAPHICS,
                                     rps_->getVkPipelineLayout(),
                                     kBindPoint_Bindless,
                                     1,
                                     &dset,
                                     0,
                                     nullptr);
  }
}

void RenderCommandEncoder::ensureVertexBuffers() {
  IGL_PROFILER_FUNCTION();

  if (!IGL_VERIFY(rps_)) {
    return;
  }

  const igl::vulkan::VertexInputState* vi = static_cast<igl::vulkan::VertexInputState*>(
      rps_->getRenderPipelineDesc().vertexInputState.get());

  if (!vi) {
    // no vertex input is perfectly valid
    return;
  }

  const VertexInputStateDesc& desc = vi->desc_;

  IGL_ASSERT(desc.numInputBindings <= IGL_ARRAY_NUM_ELEMENTS(isVertexBufferBound_));

  const size_t numBindings =
      std::min(desc.numInputBindings, IGL_ARRAY_NUM_ELEMENTS(isVertexBufferBound_));

  for (size_t i = 0; i != numBindings; i++) {
    if (!isVertexBufferBound_[i]) {
      // TODO: fix client apps and uncomment
      // IGL_ASSERT_MSG(false,
      //                "Did you forget to call bindBuffer() for one of your vertex input
      //                buffers?");
      IGL_LOG_ERROR_ONCE(
          "Did you forget to call bindBuffer() for one of your vertex input buffers?");
    }
  }
}

void RenderCommandEncoder::blitColorImage(const igl::vulkan::VulkanImage& srcImage,
                                          const igl::vulkan::VulkanImage& destImage,
                                          const igl::TextureRangeDesc& srcRange,
                                          const igl::TextureRangeDesc& destRange) {
  const auto& wrapper = ctx_.immediate_->acquire();
  const VkImageSubresourceRange srcResourceRange = {
      srcImage.getImageAspectFlags(),
      static_cast<uint32_t>(srcRange.mipLevel),
      static_cast<uint32_t>(srcRange.numMipLevels),
      static_cast<uint32_t>(srcRange.layer),
      static_cast<uint32_t>(srcRange.numLayers),
  };
  const VkImageSubresourceRange destSubresourceRange = {
      destImage.getImageAspectFlags(),
      static_cast<uint32_t>(destRange.mipLevel),
      static_cast<uint32_t>(destRange.numMipLevels),
      static_cast<uint32_t>(destRange.layer),
      static_cast<uint32_t>(destRange.numLayers),
  };
  srcImage.transitionLayout(wrapper.cmdBuf_,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            srcResourceRange);

  destImage.transitionLayout(wrapper.cmdBuf_,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             destSubresourceRange);

  const std::array<VkOffset3D, 2> srcOffsets = {
      {{static_cast<int32_t>(srcRange.x), static_cast<int32_t>(srcRange.y), 0},
       {static_cast<int32_t>(srcRange.width + srcRange.x),
        static_cast<int32_t>(srcRange.height + srcRange.y),
        1}}};
  const std::array<VkOffset3D, 2> dstOffsets = {
      {{static_cast<int32_t>(destRange.x), static_cast<int32_t>(destRange.y), 0},
       {static_cast<int32_t>(destRange.width + destRange.x),
        static_cast<int32_t>(destRange.height + destRange.y),
        1}}};
  ivkCmdBlitImage(&ctx_.vf_,
                  wrapper.cmdBuf_,
                  srcImage.getVkImage(),
                  destImage.getVkImage(),
                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  srcOffsets.data(),
                  dstOffsets.data(),
                  VkImageSubresourceLayers{
                      VK_IMAGE_ASPECT_COLOR_BIT, static_cast<uint32_t>(srcRange.mipLevel), 0, 1},
                  VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                  VK_FILTER_LINEAR);

  const bool isSampled = (destImage.getVkImageUsageFlags() & VK_IMAGE_USAGE_SAMPLED_BIT) != 0;
  const bool isStorage = (destImage.getVkImageUsageFlags() & VK_IMAGE_USAGE_STORAGE_BIT) != 0;
  const bool isColorAttachment =
      (destImage.getVkImageUsageFlags() & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0;
  const bool isDepthStencilAttachment =
      (destImage.getVkImageUsageFlags() & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;

  // a ternary cascade...
  const VkImageLayout targetLayout =
      isSampled ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                : (isStorage ? VK_IMAGE_LAYOUT_GENERAL
                             : (isColorAttachment
                                    ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                    : (isDepthStencilAttachment
                                           ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                           : VK_IMAGE_LAYOUT_UNDEFINED)));

  IGL_ASSERT_MSG(targetLayout != VK_IMAGE_LAYOUT_UNDEFINED, "Missing usage flags");

  // 3. Transition TRANSFER_DST_OPTIMAL into `targetLayout`
  destImage.transitionLayout(wrapper.cmdBuf_,
                             targetLayout,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             destSubresourceRange);

  destImage.imageLayout_ = targetLayout;
}

void RenderCommandEncoder::processDependencies(const Dependencies& dependencies) {
  // 1. Process all textures
  {
    const Dependencies* deps = &dependencies;

    while (deps) {
      for (ITexture* IGL_NULLABLE tex : deps->textures) {
        if (!tex) {
          break;
        }
        transitionToShaderReadOnly(cmdBuffer_, tex);
      }
      deps = deps->next;
    }
  }

  // 2. Process all buffers
  {
    const Dependencies* deps = &dependencies;

    while (deps) {
      for (IBuffer* IGL_NULLABLE buf : deps->buffers) {
        if (!buf) {
          break;
        }
        VkPipelineStageFlags dstStageFlags =
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        const auto* vkBuf = static_cast<const igl::vulkan::Buffer*>(buf);
        const VkBufferUsageFlags flags = vkBuf->getBufferUsageFlags();
        if ((flags & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) ||
            (flags & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)) {
          dstStageFlags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        }
        if (flags & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) {
          dstStageFlags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        }
        // compute-to-graphics barrier
        ivkBufferBarrier(&ctx_.vf_,
                         cmdBuffer_,
                         vkBuf->getVkBuffer(),
                         vkBuf->getBufferUsageFlags(),
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         dstStageFlags);
      }
      deps = deps->next;
    }
  }
}

} // namespace vulkan
} // namespace igl
