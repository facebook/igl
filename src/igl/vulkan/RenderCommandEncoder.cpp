/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/RenderCommandEncoder.h>

#include <algorithm>

#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/Framebuffer.h>
#include <igl/vulkan/RenderPipelineState.h>
#include <igl/vulkan/SamplerState.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanBuffer.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanPipelineLayout.h>
#include <igl/vulkan/VulkanRenderPassBuilder.h>
#include <igl/vulkan/VulkanSwapchain.h>

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

VkStencilOp stencilOpToVkStencilOp(igl::StencilOp op) {
  switch (op) {
  case igl::StencilOp_Keep:
    return VK_STENCIL_OP_KEEP;
  case igl::StencilOp_Zero:
    return VK_STENCIL_OP_ZERO;
  case igl::StencilOp_Replace:
    return VK_STENCIL_OP_REPLACE;
  case igl::StencilOp_IncrementClamp:
    return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
  case igl::StencilOp_DecrementClamp:
    return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
  case igl::StencilOp_Invert:
    return VK_STENCIL_OP_INVERT;
  case igl::StencilOp_IncrementWrap:
    return VK_STENCIL_OP_INCREMENT_AND_WRAP;
  case igl::StencilOp_DecrementWrap:
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
                                           const VulkanContext& ctx) :
  IRenderCommandEncoder::IRenderCommandEncoder(commandBuffer),
  ctx_(ctx),
  cmdBuffer_(commandBuffer ? commandBuffer->getVkCommandBuffer() : VK_NULL_HANDLE) {
  IGL_PROFILER_FUNCTION();
  IGL_ASSERT(commandBuffer);
  IGL_ASSERT(cmdBuffer_ != VK_NULL_HANDLE);
}

void RenderCommandEncoder::initialize(const RenderPassDesc& renderPass,
                                      const std::shared_ptr<IFramebuffer>& framebuffer,
                                      Result* outResult) {
  IGL_PROFILER_FUNCTION();
  framebuffer_ = framebuffer;

  Result::setOk(outResult);

  if (!IGL_VERIFY(framebuffer)) {
    Result::setResult(outResult, Result::Code::ArgumentNull);
    return;
  }

  const FramebufferDesc& desc = static_cast<const Framebuffer&>((*framebuffer)).getDesc();

  IGL_ASSERT(desc.colorAttachments.size() <= IGL_COLOR_ATTACHMENTS_MAX);

  std::vector<VkClearValue> clearValues;
  uint32_t mipLevel = 0;

  VulkanRenderPassBuilder builder;

  // All attachments may not valid.  Track active attachments
  size_t largestIndexPlusOne = 0;
  for (const auto& attachment : desc.colorAttachments) {
    largestIndexPlusOne = largestIndexPlusOne < attachment.first ? attachment.first
                                                                 : largestIndexPlusOne;
  }

  largestIndexPlusOne += 1;

  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

  for (size_t i = 0; i < largestIndexPlusOne; ++i) {
    auto it = desc.colorAttachments.find(i);
    if (it == desc.colorAttachments.end()) {
      continue;
    }
    IGL_ASSERT(it->second.texture);

    const auto& colorTexture = static_cast<vulkan::Texture&>(*it->second.texture);

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
    if (descColor.mipmapLevel && mipLevel) {
      IGL_ASSERT_MSG(descColor.mipmapLevel == mipLevel,
                     "All color attachments should have the same mip-level");
    }
    mipLevel = descColor.mipmapLevel;
    const auto initialLayout = descColor.loadAction == igl::LoadAction::Load
                                   ? colorTexture.getVulkanTexture().getVulkanImage().imageLayout_
                                   : VK_IMAGE_LAYOUT_UNDEFINED;
    builder.addColor(textureFormatToVkFormat(colorTexture.getFormat()),
                     loadActionToVkAttachmentLoadOp(descColor.loadAction),
                     storeActionToVkAttachmentStoreOp(descColor.storeAction),
                     initialLayout,
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                     colorTexture.getVulkanTexture().getVulkanImage().samples_);
    // RenderPassBuilder ensures that all non-resolve attachments have the same number of samples
    samples = colorTexture.getVulkanTexture().getVulkanImage().samples_;
    // handle MSAA
    if (descColor.storeAction == StoreAction::MsaaResolve) {
      IGL_ASSERT_MSG(it->second.resolveTexture != nullptr,
                     "Framebuffer attachment should contain a resolve texture");
      const auto& colorResolveTexture = static_cast<vulkan::Texture&>(*it->second.resolveTexture);
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
  const igl::AttachmentDesc descDepth = renderPass.depthStencilAttachment;
  hasDepthAttachment_ = false;

  if (framebuffer->getDepthAttachment()) {
    const auto& depthTexture = static_cast<vulkan::Texture&>(*(framebuffer->getDepthAttachment()));
    hasDepthAttachment_ = true;
    IGL_ASSERT_MSG(descDepth.mipmapLevel == mipLevel,
                   "Depth attachment should have the same mip-level as color attachments");
    clearValues.push_back(ivkGetClearDepthStencilValue(descDepth.clearDepth, descDepth.clearStencil));
    const auto initialLayout = descDepth.loadAction == igl::LoadAction::Load
                                   ? depthTexture.getVulkanTexture().getVulkanImage().imageLayout_
                                   : VK_IMAGE_LAYOUT_UNDEFINED;
    builder.addDepth(depthTexture.getVkFormat(),
                     loadActionToVkAttachmentLoadOp(descDepth.loadAction),
                     storeActionToVkAttachmentStoreOp(descDepth.storeAction),
                     initialLayout,
                     VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                     depthTexture.getVulkanTexture().getVulkanImage().samples_);
    // RenderPassBuilder ensures that all non-resolve attachments have the same number of samples
    samples = depthTexture.getVulkanTexture().getVulkanImage().samples_;
  }

  const auto& fb = static_cast<vulkan::Framebuffer&>(*framebuffer);

  auto renderPassHandle = ctx_.findRenderPass(builder);

  dynamicState_.renderPassIndex_ = renderPassHandle.index;
  dynamicState_.depthBiasEnable_ = false;

  const VkRenderPassBeginInfo bi = fb.getRenderPassBeginInfo(
      renderPassHandle.pass, mipLevel, (uint32_t)clearValues.size(), clearValues.data());

  const uint32_t width = std::max(fb.getWidth() >> mipLevel, 1u);
  const uint32_t height = std::max(fb.getHeight() >> mipLevel, 1u);
  const igl::Viewport viewport = {0.0f, 0.0f, (float)width, (float)height, 0.0f, +1.0f};
  const igl::ScissorRect scissor = {0, 0, width, height};

  bindViewport(viewport);
  bindScissorRect(scissor);

  ctx_.checkAndUpdateDescriptorSets();
  ctx_.bindDefaultDescriptorSets(cmdBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS);

  vkCmdBeginRenderPass(cmdBuffer_, &bi, VK_SUBPASS_CONTENTS_INLINE);

  isEncoding_ = true;

  Result::setOk(outResult);
}

std::unique_ptr<RenderCommandEncoder> RenderCommandEncoder::create(
    const std::shared_ptr<CommandBuffer>& commandBuffer,
    const VulkanContext& ctx,
    const RenderPassDesc& renderPass,
    const std::shared_ptr<IFramebuffer>& framebuffer,
    Result* outResult) {
  IGL_PROFILER_FUNCTION();

  Result ret;

  std::unique_ptr<RenderCommandEncoder> encoder(new RenderCommandEncoder(commandBuffer, ctx));
  encoder->initialize(renderPass, framebuffer, &ret);

  Result::setResult(outResult, ret);
  return ret.isOk() ? std::move(encoder) : nullptr;
}

void RenderCommandEncoder::endEncoding() {
  IGL_PROFILER_FUNCTION();

  if (!isEncoding_) {
    return;
  }

  isEncoding_ = false;

  vkCmdEndRenderPass(cmdBuffer_);

  // set image layouts after the render pass
  const FramebufferDesc& desc = static_cast<const Framebuffer&>((*framebuffer_)).getDesc();

  for (const auto& attachment : desc.colorAttachments) {
    const vulkan::Texture& tex = static_cast<vulkan::Texture&>(*attachment.second.texture.get());
    // this must match the final layout of the render pass
    tex.getVulkanTexture().getVulkanImage().imageLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  if (desc.depthAttachment.texture) {
    const vulkan::Texture& tex = static_cast<vulkan::Texture&>(*desc.depthAttachment.texture.get());
    // this must match the final layout of the render pass
    tex.getVulkanTexture().getVulkanImage().imageLayout_ =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  }
}

void RenderCommandEncoder::pushDebugGroupLabel(const std::string& label,
                                               const igl::Color& color) const {
  ivkCmdBeginDebugUtilsLabel(cmdBuffer_, label.c_str(), color.toFloatPtr());
}

void RenderCommandEncoder::insertDebugEventLabel(const std::string& label,
                                                 const igl::Color& color) const {
  ivkCmdInsertDebugUtilsLabel(cmdBuffer_, label.c_str(), color.toFloatPtr());
}

void RenderCommandEncoder::popDebugGroupLabel() const {
  ivkCmdEndDebugUtilsLabel(cmdBuffer_);
}

void RenderCommandEncoder::bindViewport(const Viewport& viewport) {
  /**
    Using the negative viewport height Vulkan feature, we make the Vulkan "top-left" coordinate
  system to be "bottom-left" as in OpenGL. This way VK_FRONT_FACE_COUNTER_CLOCKWISE and
  VK_FRONT_FACE_CLOCKWISE use the same winding as in OpenGL. Part of VK_KHR_maintenance1 which is
  promoted to Vulkan 1.1.

  More details: https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
  **/
  const VkViewport vp = {
      .x = viewport.x, // float x;
      .y = viewport.height - viewport.y, // float y;
      .width = viewport.width, // float width;
      .height = -viewport.height, // float height;
      .minDepth = viewport.minDepth, // float minDepth;
      .maxDepth = viewport.maxDepth, // float maxDepth;
  };
  vkCmdSetViewport(cmdBuffer_, 0, 1, &vp);
}

void RenderCommandEncoder::bindScissorRect(const ScissorRect& rect) {
  const VkRect2D scissor = {
      VkOffset2D{(int32_t)rect.x, (int32_t)rect.y},
      VkExtent2D{rect.width, rect.height},
  };
  vkCmdSetScissor(cmdBuffer_, 0, 1, &scissor);
}

void RenderCommandEncoder::bindRenderPipelineState(
    const std::shared_ptr<IRenderPipelineState>& pipelineState) {
  if (!IGL_VERIFY(pipelineState != nullptr)) {
    return;
  }

  currentPipeline_ = pipelineState;

  const igl::vulkan::RenderPipelineState* rps =
      static_cast<igl::vulkan::RenderPipelineState*>(pipelineState.get());

  IGL_ASSERT(rps);

  const RenderPipelineDesc& desc = rps->getRenderPipelineDesc();

  const bool hasDepthAttachment = desc.targetDesc.depthAttachmentFormat != TextureFormat::Invalid;

  if (hasDepthAttachment != hasDepthAttachment_) {
    IGL_ASSERT(false);
    LLOGW("Make sure your render pass and render pipeline both have matching depth attachments");
  }

  lastPipelineBound_ = VK_NULL_HANDLE;
}

void RenderCommandEncoder::bindDepthStencilState(const DepthStencilState& desc) {
  IGL_PROFILER_FUNCTION();

  dynamicState_.depthWriteEnable_ = desc.isDepthWriteEnabled;
  dynamicState_.setDepthCompareOp(compareOpToVkCompareOp(desc.compareOp));

  auto setStencilState = [this](VkStencilFaceFlagBits faceMask, const igl::StencilStateDesc& desc) {
    dynamicState_.setStencilStateOps(faceMask,
                                     stencilOpToVkStencilOp(desc.stencilFailureOp),
                                     stencilOpToVkStencilOp(desc.depthStencilPassOp),
                                     stencilOpToVkStencilOp(desc.depthFailureOp),
                                     compareOpToVkCompareOp(desc.stencilCompareOp));
    vkCmdSetStencilReference(cmdBuffer_, faceMask, desc.readMask);
    vkCmdSetStencilCompareMask(cmdBuffer_, faceMask, 0xFF);
    vkCmdSetStencilWriteMask(cmdBuffer_, faceMask, desc.writeMask);
  };

  setStencilState(VK_STENCIL_FACE_FRONT_BIT, desc.frontFaceStencil);
  setStencilState(VK_STENCIL_FACE_BACK_BIT, desc.backFaceStencil);
}

void RenderCommandEncoder::bindVertexBuffer(uint32_t index,
                                            const std::shared_ptr<IBuffer>& buffer,
                                            size_t bufferOffset) {
  IGL_PROFILER_FUNCTION();

#if IGL_VULKAN_PRINT_COMMANDS
  LLOGL("%p  vkCmdBindVertexBuffers(%u, %u)\n", cmdBuffer_, index, (uint32_t)bufferOffset);
#endif // IGL_VULKAN_PRINT_COMMANDS

  if (!IGL_VERIFY(buffer != nullptr)) {
    return;
  }

  auto* buf = static_cast<igl::vulkan::Buffer*>(buffer.get());

  VkBuffer vkBuf = buf->getVkBuffer();

  IGL_ASSERT(buf->getBufferType() & BufferDesc::BufferTypeBits::Vertex);

  const VkDeviceSize offset = bufferOffset;
  vkCmdBindVertexBuffers(cmdBuffer_, index, 1, &vkBuf, &offset);
}

void RenderCommandEncoder::bindPushConstants(size_t offset, const void* data, size_t length) {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT(length % 4 == 0); // VUID-vkCmdPushConstants-size-00369: size must be a multiple of 4

  // check push constant size is within max size
  const VkPhysicalDeviceLimits& limits = ctx_.getVkPhysicalDeviceProperties().limits;
  const size_t size = offset + length;
  if (!IGL_VERIFY(size <= limits.maxPushConstantsSize)) {
    LLOGW(
        "Push constants size exceeded %u (max %u bytes)", size, limits.maxPushConstantsSize);
  }

  vkCmdPushConstants(cmdBuffer_,
                     ctx_.pipelineLayoutGraphics_->getVkPipelineLayout(),
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                     (uint32_t)offset,
                     (uint32_t)length,
                     data);
}

void RenderCommandEncoder::bindPipeline() {
  const igl::vulkan::RenderPipelineState* rps =
      static_cast<igl::vulkan::RenderPipelineState*>(currentPipeline_.get());

  if (!IGL_VERIFY(rps)) {
    return;
  }

  VkPipeline pipeline = rps->getVkPipeline(dynamicState_);

  if (lastPipelineBound_ != pipeline) {
    lastPipelineBound_ = pipeline;
    if (pipeline != VK_NULL_HANDLE) {
      vkCmdBindPipeline(cmdBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }
  }
}

void RenderCommandEncoder::draw(PrimitiveType primitiveType,
                                size_t vertexStart,
                                size_t vertexCount) {
  IGL_PROFILER_FUNCTION();

  if (vertexCount == 0) {
    return;
  }

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  bindPipeline();

#if IGL_VULKAN_PRINT_COMMANDS
  LLOGL("%p vkCmdDraw(%u, %u)\n", cmdBuffer_, (uint32_t)vertexCount, (uint32_t)vertexStart);
#endif // IGL_VULKAN_PRINT_COMMANDS

  vkCmdDraw(cmdBuffer_, (uint32_t)vertexCount, 1, (uint32_t)vertexStart, 0);
}

void RenderCommandEncoder::drawIndexed(PrimitiveType primitiveType,
                                       size_t indexCount,
                                       IndexFormat indexFormat,
                                       IBuffer& indexBuffer,
                                       size_t indexBufferOffset) {
  IGL_PROFILER_FUNCTION();

  if (indexCount == 0) {
    return;
  }

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  bindPipeline();

  const igl::vulkan::Buffer* buf = static_cast<igl::vulkan::Buffer*>(&indexBuffer);

  const VkIndexType type = indexFormatToVkIndexType(indexFormat);
#if IGL_VULKAN_PRINT_COMMANDS
  LLOGL("%p vkCmdBindIndexBuffer(%u)\n", cmdBuffer_, (uint32_t)indexBufferOffset);
#endif // IGL_VULKAN_PRINT_COMMANDS
  vkCmdBindIndexBuffer(cmdBuffer_, buf->getVkBuffer(), indexBufferOffset, type);

#if IGL_VULKAN_PRINT_COMMANDS
  LLOGL("%p vkCmdDrawIndexed(%u)\n", cmdBuffer_, (uint32_t)indexCount);
#endif // IGL_VULKAN_PRINT_COMMANDS
  vkCmdDrawIndexed(cmdBuffer_, (uint32_t)indexCount, 1, 0, 0, 0);
}

void RenderCommandEncoder::multiDrawIndirect(PrimitiveType primitiveType,
                                             IBuffer& indirectBuffer,
                                             size_t indirectBufferOffset,
                                             uint32_t drawCount,
                                             uint32_t stride) {
  IGL_PROFILER_FUNCTION();

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  bindPipeline();

  const igl::vulkan::Buffer* bufIndirect = static_cast<igl::vulkan::Buffer*>(&indirectBuffer);

  vkCmdDrawIndirect(cmdBuffer_,
                    bufIndirect->getVkBuffer(),
                    indirectBufferOffset,
                    drawCount,
                    stride ? stride : sizeof(VkDrawIndirectCommand));
}

void RenderCommandEncoder::multiDrawIndexedIndirect(PrimitiveType primitiveType,
                                                    IndexFormat indexFormat,
                                                    IBuffer& indexBuffer,
                                                    IBuffer& indirectBuffer,
                                                    size_t indirectBufferOffset,
                                                    uint32_t drawCount,
                                                    uint32_t stride) {
  IGL_PROFILER_FUNCTION();

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  bindPipeline();

  const igl::vulkan::Buffer* bufIndex = static_cast<igl::vulkan::Buffer*>(&indexBuffer);
  const igl::vulkan::Buffer* bufIndirect = static_cast<igl::vulkan::Buffer*>(&indirectBuffer);

  const VkIndexType type = indexFormatToVkIndexType(indexFormat);
  vkCmdBindIndexBuffer(cmdBuffer_, bufIndex->getVkBuffer(), 0, type);

  vkCmdDrawIndexedIndirect(cmdBuffer_,
                           bufIndirect->getVkBuffer(),
                           indirectBufferOffset,
                           drawCount,
                           stride ? stride : sizeof(VkDrawIndexedIndirectCommand));
}

void RenderCommandEncoder::setStencilReferenceValues(uint32_t frontValue, uint32_t backValue) {
  vkCmdSetStencilReference(cmdBuffer_, VK_STENCIL_FACE_FRONT_BIT, frontValue);
  vkCmdSetStencilReference(cmdBuffer_, VK_STENCIL_FACE_BACK_BIT, backValue);
}

void RenderCommandEncoder::setBlendColor(Color color) {
  vkCmdSetBlendConstants(cmdBuffer_, color.toFloatPtr());
}

void RenderCommandEncoder::setDepthBias(float depthBias, float slopeScale, float clamp) {
  dynamicState_.depthBiasEnable_ = true;
  vkCmdSetDepthBias(cmdBuffer_, depthBias, clamp, slopeScale);
}

} // namespace vulkan
} // namespace igl
