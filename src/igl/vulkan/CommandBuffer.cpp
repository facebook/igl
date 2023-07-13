/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/CommandBuffer.h>

#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/ComputePipelineState.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanPipelineLayout.h>
#include <igl/vulkan/VulkanTexture.h>

namespace igl::vulkan {

CommandBuffer::CommandBuffer(VulkanContext& ctx) :
  ctx_(ctx), wrapper_(ctx_.immediate_->acquire()) {}

CommandBuffer::~CommandBuffer() {
  IGL_ASSERT(!isRendering_); // did you forget to call cmdEndRendering()?
}

namespace {

void transitionColorAttachment(VkCommandBuffer buffer, const std::shared_ptr<ITexture>& colorTex) {
  // We really shouldn't get a null here, but just in case.
  if (!IGL_VERIFY(colorTex.get())) {
    return;
  }

  const auto& vkTex = static_cast<Texture&>(*colorTex);
  const auto& colorImg = vkTex.getVulkanTexture().getVulkanImage();
  if (!IGL_VERIFY(!colorImg.isDepthFormat_ && !colorImg.isStencilFormat_)) {
    IGL_ASSERT_MSG(false, "Color attachments cannot have depth/stencil formats");
    return;
  }
  IGL_ASSERT_MSG(colorImg.imageFormat_ != VK_FORMAT_UNDEFINED, "Invalid color attachment format");
  colorImg.transitionLayout(
      buffer,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // wait for all subsequent fragment/compute shaders
      VkImageSubresourceRange{
          VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
}

VkAttachmentLoadOp loadOpToVkAttachmentLoadOp(igl::LoadOp a) {
  using igl::LoadOp;
  switch (a) {
  case LoadOp_DontCare:
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  case LoadOp_Load:
    return VK_ATTACHMENT_LOAD_OP_LOAD;
  case LoadOp_Clear:
    return VK_ATTACHMENT_LOAD_OP_CLEAR;
  case LoadOp_None:
    return VK_ATTACHMENT_LOAD_OP_NONE_EXT;
  }
  IGL_ASSERT(false);
  return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

VkAttachmentStoreOp storeOpToVkAttachmentStoreOp(igl::StoreOp a) {
  using igl::StoreOp;
  switch (a) {
  case StoreOp_DontCare:
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
  case StoreOp_Store:
    return VK_ATTACHMENT_STORE_OP_STORE;
  case StoreOp_MsaaResolve:
    // for MSAA resolve, we have to store data into a special "resolve" attachment
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
  case StoreOp_None:
    return VK_ATTACHMENT_STORE_OP_NONE;
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

void CommandBuffer ::transitionToShaderReadOnly(const std::shared_ptr<ITexture>& surface) const {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT(surface.get());

  const auto& vkTex = static_cast<Texture&>(*surface);
  const VulkanTexture& tex = vkTex.getVulkanTexture();
  const VulkanImage& img = tex.getVulkanImage();

  IGL_ASSERT(!vkTex.isSwapchainTexture());

  // transition only non-multisampled images - MSAA images cannot be accessed from shaders
  if (img.samples_ == VK_SAMPLE_COUNT_1_BIT) {
    const VkImageAspectFlags flags =
        vkTex.getVulkanTexture().getVulkanImage().getImageAspectFlags();
    const VkPipelineStageFlags srcStage = lvk::isDepthOrStencilFormat(vkTex.getFormat())
                                              ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                                              : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // set the result of the previous render pass
    img.transitionLayout(
        wrapper_.cmdBuf_,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        srcStage,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // wait for subsequent fragment/compute shaders
        VkImageSubresourceRange{flags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }
}

void CommandBuffer::cmdBindComputePipelineState(
    const std::shared_ptr<IComputePipelineState>& pipelineState) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_VERIFY(pipelineState != nullptr)) {
    return;
  }

  const igl::vulkan::ComputePipelineState* cps =
      static_cast<igl::vulkan::ComputePipelineState*>(pipelineState.get());

  IGL_ASSERT(cps);

  VkPipeline pipeline = cps->getVkPipeline();

  if (lastPipelineBound_ != pipeline) {
    lastPipelineBound_ = pipeline;
    if (pipeline != VK_NULL_HANDLE) {
      vkCmdBindPipeline(wrapper_.cmdBuf_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    }
  }
}

void CommandBuffer::cmdDispatchThreadGroups(const Dimensions& threadgroupCount,
                                            const Dependencies& deps) {
  IGL_ASSERT(!isRendering_);

  for (uint32_t i = 0; i != Dependencies::IGL_MAX_SUBMIT_DEPENDENCIES && deps.textures[i]; i++) {
    useComputeTexture(deps.textures[i]);
  }

  ctx_.checkAndUpdateDescriptorSets();
  ctx_.bindDefaultDescriptorSets(wrapper_.cmdBuf_, VK_PIPELINE_BIND_POINT_COMPUTE);

  vkCmdDispatch(
      wrapper_.cmdBuf_, threadgroupCount.width, threadgroupCount.height, threadgroupCount.depth);
}

void CommandBuffer::cmdPushDebugGroupLabel(const char* label, const igl::Color& color) const {
  IGL_ASSERT(label);

  ivkCmdBeginDebugUtilsLabel(wrapper_.cmdBuf_, label, color.toFloatPtr());
}

void CommandBuffer::cmdInsertDebugEventLabel(const char* label, const igl::Color& color) const {
  IGL_ASSERT(label);

  ivkCmdInsertDebugUtilsLabel(wrapper_.cmdBuf_, label, color.toFloatPtr());
}

void CommandBuffer::cmdPopDebugGroupLabel() const {
  ivkCmdEndDebugUtilsLabel(wrapper_.cmdBuf_);
}

void CommandBuffer::useComputeTexture(ITexture* texture) {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT(texture);
  const igl::vulkan::Texture* tex = static_cast<igl::vulkan::Texture*>(texture);
  const igl::vulkan::VulkanTexture& vkTex = tex->getVulkanTexture();
  const igl::vulkan::VulkanImage& vkImage = vkTex.getVulkanImage();
  if (!vkImage.isStorageImage()) {
    IGL_ASSERT_MSG(false, "Did you forget to specify TextureUsageBits::Storage on your texture?");
    return;
  }

  // "frame graph" heuristics: if we are already in VK_IMAGE_LAYOUT_GENERAL, wait for the previous
  // compute shader
  const VkPipelineStageFlags srcStage = (vkImage.imageLayout_ == VK_IMAGE_LAYOUT_GENERAL)
                                            ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                            : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  vkImage.transitionLayout(
      wrapper_.cmdBuf_,
      VK_IMAGE_LAYOUT_GENERAL,
      srcStage,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VkImageSubresourceRange{
          vkImage.getImageAspectFlags(), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
}

void CommandBuffer::cmdBeginRendering(const igl::RenderPass& renderPass,
                                      const igl::Framebuffer& fb) {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT(!isRendering_);

  isRendering_ = true;

  const uint32_t numFbColorAttachments = fb.getNumColorAttachments();
  const uint32_t numPassColorAttachments = renderPass.getNumColorAttachments();

  IGL_ASSERT(numPassColorAttachments == numFbColorAttachments);

  framebuffer_ = fb;

  // transition all the color attachments
  for (uint32_t i = 0; i != numFbColorAttachments; i++) {
    if (const auto colorTex = fb.colorAttachments[i].texture) {
      transitionColorAttachment(wrapper_.cmdBuf_, colorTex);
    }
    // handle MSAA
    if (const auto colorResolveTex = fb.colorAttachments[i].resolveTexture) {
      transitionColorAttachment(wrapper_.cmdBuf_, colorResolveTex);
    }
  }
  // transition depth-stencil attachment
  const auto depthTex = fb.depthStencilAttachment.texture;
  if (depthTex) {
    const auto& vkDepthTex = static_cast<Texture&>(*depthTex);
    const auto& depthImg = vkDepthTex.getVulkanTexture().getVulkanImage();
    IGL_ASSERT_MSG(depthImg.imageFormat_ != VK_FORMAT_UNDEFINED, "Invalid depth attachment format");
    const VkImageAspectFlags flags =
        vkDepthTex.getVulkanTexture().getVulkanImage().getImageAspectFlags();
    depthImg.transitionLayout(
        wrapper_.cmdBuf_,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // wait for all subsequent
                                           // operations
        VkImageSubresourceRange{flags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }

  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
  uint32_t mipLevel = 0;
  uint32_t fbWidth = 0;
  uint32_t fbHeight = 0;

  // Process depth attachment
  dynamicState_.depthBiasEnable_ = false;

  VkRenderingAttachmentInfo colorAttachments[IGL_COLOR_ATTACHMENTS_MAX];

  for (uint32_t i = 0; i != numFbColorAttachments; i++) {
    auto& attachment = fb.colorAttachments[i];
    IGL_ASSERT(attachment.texture.get());

    const auto& colorTexture = static_cast<vulkan::Texture&>(*attachment.texture);
    const auto& descColor = renderPass.colorAttachments[i];
    if (mipLevel && descColor.level) {
      IGL_ASSERT_MSG(descColor.level == mipLevel,
                     "All color attachments should have the same mip-level");
    }
    const igl::Dimensions dim = colorTexture.getDimensions();
    if (fbWidth) {
      IGL_ASSERT_MSG(dim.width == fbWidth, "All attachments should have the save width");
    }
    if (fbHeight) {
      IGL_ASSERT_MSG(dim.height == fbHeight, "All attachments should have the save width");
    }
    mipLevel = descColor.level;
    fbWidth = dim.width;
    fbHeight = dim.height;
    samples = colorTexture.getVulkanTexture().getVulkanImage().samples_;
    colorAttachments[i] = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = colorTexture.getVkImageViewForFramebuffer(0),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode = (samples > 1) ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = loadOpToVkAttachmentLoadOp(descColor.loadOp),
        .storeOp = storeOpToVkAttachmentStoreOp(descColor.storeOp),
        .clearValue = ivkGetClearColorValue(descColor.clearColor.r,
                                            descColor.clearColor.g,
                                            descColor.clearColor.b,
                                            descColor.clearColor.a),
    };
    // handle MSAA
    if (descColor.storeOp == StoreOp_MsaaResolve) {
      IGL_ASSERT(samples > 1);
      IGL_ASSERT_MSG(attachment.resolveTexture != nullptr,
                     "Framebuffer attachment should contain a resolve texture");
      const auto& colorResolveTexture = static_cast<vulkan::Texture&>(*attachment.resolveTexture);
      colorAttachments[i].resolveImageView = colorResolveTexture.getVkImageViewForFramebuffer(0);
      colorAttachments[i].resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
  }

  VkRenderingAttachmentInfo depthAttachment = {};

  if (fb.depthStencilAttachment.texture) {
    const auto& depthTexture = static_cast<vulkan::Texture&>(*fb.depthStencilAttachment.texture);
    const auto& descDepth = renderPass.depthAttachment;
    IGL_ASSERT_MSG(descDepth.level == mipLevel,
                   "Depth attachment should have the same mip-level as color attachments");
    depthAttachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = depthTexture.getVkImageViewForFramebuffer(0),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = loadOpToVkAttachmentLoadOp(descDepth.loadOp),
        .storeOp = storeOpToVkAttachmentStoreOp(descDepth.storeOp),
        .clearValue = ivkGetClearDepthStencilValue(descDepth.clearDepth, descDepth.clearStencil),
    };
    const igl::Dimensions dim = depthTexture.getDimensions();
    if (fbWidth) {
      IGL_ASSERT_MSG(dim.width == fbWidth, "All attachments should have the save width");
    }
    if (fbHeight) {
      IGL_ASSERT_MSG(dim.height == fbHeight, "All attachments should have the save width");
    }
    mipLevel = descDepth.level;
    fbWidth = dim.width;
    fbHeight = dim.height;
  }

  const uint32_t width = std::max(fbWidth >> mipLevel, 1u);
  const uint32_t height = std::max(fbHeight >> mipLevel, 1u);
  const igl::Viewport viewport = {0.0f, 0.0f, (float)width, (float)height, 0.0f, +1.0f};
  const igl::ScissorRect scissor = {0, 0, width, height};

  VkRenderingAttachmentInfo stencilAttachment = depthAttachment;

  const bool isStencilFormat = renderPass.stencilAttachment.loadOp != igl::LoadOp_Invalid;

  const VkRenderingInfo renderingInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .pNext = nullptr,
      .flags = 0,
      .renderArea = {VkOffset2D{(int32_t)scissor.x, (int32_t)scissor.y},
                     VkExtent2D{scissor.width, scissor.height}},
      .layerCount = 1,
      .viewMask = 0,
      .colorAttachmentCount = numFbColorAttachments,
      .pColorAttachments = colorAttachments,
      .pDepthAttachment = depthTex ? &depthAttachment : nullptr,
      .pStencilAttachment = isStencilFormat ? &stencilAttachment : nullptr,
  };

  cmdBindViewport(viewport);
  cmdBindScissorRect(scissor);

  ctx_.checkAndUpdateDescriptorSets();
  ctx_.bindDefaultDescriptorSets(wrapper_.cmdBuf_, VK_PIPELINE_BIND_POINT_GRAPHICS);

  vkCmdBeginRendering(wrapper_.cmdBuf_, &renderingInfo);
}

void CommandBuffer::cmdEndRendering() {
  IGL_ASSERT(isRendering_);

  isRendering_ = false;

  vkCmdEndRendering(wrapper_.cmdBuf_);

  const uint32_t numFbColorAttachments = framebuffer_.getNumColorAttachments();

  // set image layouts after the render pass
  for (uint32_t i = 0; i != numFbColorAttachments; i++) {
    const auto& attachment = framebuffer_.colorAttachments[i];
    const vulkan::Texture& tex = static_cast<vulkan::Texture&>(*attachment.texture.get());
    // this must match the final layout of the render pass
    tex.getVulkanTexture().getVulkanImage().imageLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  if (framebuffer_.depthStencilAttachment.texture) {
    const vulkan::Texture& tex =
        static_cast<vulkan::Texture&>(*framebuffer_.depthStencilAttachment.texture.get());
    // this must match the final layout of the render pass
    tex.getVulkanTexture().getVulkanImage().imageLayout_ =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  }

  framebuffer_ = {};
}

void CommandBuffer::cmdBindViewport(const Viewport& viewport) {
  // https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
  const VkViewport vp = {
      .x = viewport.x, // float x;
      .y = viewport.height - viewport.y, // float y;
      .width = viewport.width, // float width;
      .height = -viewport.height, // float height;
      .minDepth = viewport.minDepth, // float minDepth;
      .maxDepth = viewport.maxDepth, // float maxDepth;
  };
  vkCmdSetViewport(wrapper_.cmdBuf_, 0, 1, &vp);
}

void CommandBuffer::cmdBindScissorRect(const ScissorRect& rect) {
  const VkRect2D scissor = {
      VkOffset2D{(int32_t)rect.x, (int32_t)rect.y},
      VkExtent2D{rect.width, rect.height},
  };
  vkCmdSetScissor(wrapper_.cmdBuf_, 0, 1, &scissor);
}

void CommandBuffer::cmdBindRenderPipelineState(
    const std::shared_ptr<IRenderPipelineState>& pipelineState) {
  if (!IGL_VERIFY(pipelineState != nullptr)) {
    return;
  }

  currentPipeline_ = pipelineState;

  const igl::vulkan::RenderPipelineState* rps =
      static_cast<igl::vulkan::RenderPipelineState*>(pipelineState.get());

  IGL_ASSERT(rps);

  const RenderPipelineDesc& desc = rps->getRenderPipelineDesc();

  const bool hasDepthAttachmentPipeline = desc.depthAttachmentFormat != TextureFormat::Invalid;
  const bool hasDepthAttachmentPass = framebuffer_.depthStencilAttachment.texture != nullptr;

  if (hasDepthAttachmentPipeline != hasDepthAttachmentPass) {
    IGL_ASSERT(false);
    LLOGW("Make sure your render pass and render pipeline both have matching depth attachments");
  }

  lastPipelineBound_ = VK_NULL_HANDLE;
}

void CommandBuffer::cmdBindDepthStencilState(const DepthStencilState& desc) {
  IGL_PROFILER_FUNCTION();

  dynamicState_.depthWriteEnable_ = desc.isDepthWriteEnabled;
  dynamicState_.setDepthCompareOp(compareOpToVkCompareOp(desc.compareOp));

  auto setStencilState = [this](VkStencilFaceFlagBits faceMask, const igl::StencilStateDesc& desc) {
    dynamicState_.setStencilStateOps(faceMask,
                                     stencilOpToVkStencilOp(desc.stencilFailureOp),
                                     stencilOpToVkStencilOp(desc.depthStencilPassOp),
                                     stencilOpToVkStencilOp(desc.depthFailureOp),
                                     compareOpToVkCompareOp(desc.stencilCompareOp));
    vkCmdSetStencilReference(wrapper_.cmdBuf_, faceMask, desc.readMask);
    vkCmdSetStencilCompareMask(wrapper_.cmdBuf_, faceMask, 0xFF);
    vkCmdSetStencilWriteMask(wrapper_.cmdBuf_, faceMask, desc.writeMask);
  };

  setStencilState(VK_STENCIL_FACE_FRONT_BIT, desc.frontFaceStencil);
  setStencilState(VK_STENCIL_FACE_BACK_BIT, desc.backFaceStencil);
}

void CommandBuffer::cmdBindVertexBuffer(uint32_t index,
                                        const std::shared_ptr<IBuffer>& buffer,
                                        size_t bufferOffset) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_VERIFY(buffer != nullptr)) {
    return;
  }

  auto* buf = static_cast<igl::vulkan::Buffer*>(buffer.get());

  VkBuffer vkBuf = buf->getVkBuffer();

  IGL_ASSERT(buf->desc_.usage & BufferUsageBits_Vertex);

  const VkDeviceSize offset = bufferOffset;
  vkCmdBindVertexBuffers(wrapper_.cmdBuf_, index, 1, &vkBuf, &offset);
}

void CommandBuffer::cmdPushConstants(size_t offset, const void* data, size_t length) {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT(length % 4 == 0); // VUID-vkCmdPushConstants-size-00369: size must be a multiple of 4

  // check push constant size is within max size
  const VkPhysicalDeviceLimits& limits = ctx_.getVkPhysicalDeviceProperties().limits;
  const size_t size = offset + length;
  if (!IGL_VERIFY(size <= limits.maxPushConstantsSize)) {
    LLOGW("Push constants size exceeded %u (max %u bytes)", size, limits.maxPushConstantsSize);
  }

  vkCmdPushConstants(wrapper_.cmdBuf_,
                     ctx_.pipelineLayout_->getVkPipelineLayout(),
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                         VK_SHADER_STAGE_COMPUTE_BIT,
                     (uint32_t)offset,
                     (uint32_t)length,
                     data);
}

void CommandBuffer::bindGraphicsPipeline() {
  const igl::vulkan::RenderPipelineState* rps =
      static_cast<igl::vulkan::RenderPipelineState*>(currentPipeline_.get());

  if (!IGL_VERIFY(rps)) {
    return;
  }

  VkPipeline pipeline = rps->getVkPipeline(dynamicState_);

  if (lastPipelineBound_ != pipeline) {
    lastPipelineBound_ = pipeline;
    if (pipeline != VK_NULL_HANDLE) {
      vkCmdBindPipeline(wrapper_.cmdBuf_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }
  }
}

void CommandBuffer::cmdDraw(PrimitiveType primitiveType, size_t vertexStart, size_t vertexCount) {
  IGL_PROFILER_FUNCTION();

  if (vertexCount == 0) {
    return;
  }

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  bindGraphicsPipeline();

  vkCmdDraw(wrapper_.cmdBuf_, (uint32_t)vertexCount, 1, (uint32_t)vertexStart, 0);
}

void CommandBuffer::cmdDrawIndexed(PrimitiveType primitiveType,
                                   size_t indexCount,
                                   IndexFormat indexFormat,
                                   IBuffer& indexBuffer,
                                   size_t indexBufferOffset) {
  IGL_PROFILER_FUNCTION();

  if (indexCount == 0) {
    return;
  }

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  bindGraphicsPipeline();

  const igl::vulkan::Buffer* buf = static_cast<igl::vulkan::Buffer*>(&indexBuffer);

  const VkIndexType type = indexFormatToVkIndexType(indexFormat);
  vkCmdBindIndexBuffer(wrapper_.cmdBuf_, buf->getVkBuffer(), indexBufferOffset, type);

  vkCmdDrawIndexed(wrapper_.cmdBuf_, (uint32_t)indexCount, 1, 0, 0, 0);
}

void CommandBuffer::cmdDrawIndirect(PrimitiveType primitiveType,
                                    IBuffer& indirectBuffer,
                                    size_t indirectBufferOffset,
                                    uint32_t drawCount,
                                    uint32_t stride) {
  IGL_PROFILER_FUNCTION();

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  bindGraphicsPipeline();

  const igl::vulkan::Buffer* bufIndirect = static_cast<igl::vulkan::Buffer*>(&indirectBuffer);

  vkCmdDrawIndirect(wrapper_.cmdBuf_,
                    bufIndirect->getVkBuffer(),
                    indirectBufferOffset,
                    drawCount,
                    stride ? stride : sizeof(VkDrawIndirectCommand));
}

void CommandBuffer::cmdDrawIndexedIndirect(PrimitiveType primitiveType,
                                           IndexFormat indexFormat,
                                           IBuffer& indexBuffer,
                                           IBuffer& indirectBuffer,
                                           size_t indirectBufferOffset,
                                           uint32_t drawCount,
                                           uint32_t stride) {
  IGL_PROFILER_FUNCTION();

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  bindGraphicsPipeline();

  const igl::vulkan::Buffer* bufIndex = static_cast<igl::vulkan::Buffer*>(&indexBuffer);
  const igl::vulkan::Buffer* bufIndirect = static_cast<igl::vulkan::Buffer*>(&indirectBuffer);

  const VkIndexType type = indexFormatToVkIndexType(indexFormat);
  vkCmdBindIndexBuffer(wrapper_.cmdBuf_, bufIndex->getVkBuffer(), 0, type);

  vkCmdDrawIndexedIndirect(wrapper_.cmdBuf_,
                           bufIndirect->getVkBuffer(),
                           indirectBufferOffset,
                           drawCount,
                           stride ? stride : sizeof(VkDrawIndexedIndirectCommand));
}

void CommandBuffer::cmdSetStencilReferenceValues(uint32_t frontValue, uint32_t backValue) {
  vkCmdSetStencilReference(wrapper_.cmdBuf_, VK_STENCIL_FACE_FRONT_BIT, frontValue);
  vkCmdSetStencilReference(wrapper_.cmdBuf_, VK_STENCIL_FACE_BACK_BIT, backValue);
}

void CommandBuffer::cmdSetBlendColor(Color color) {
  vkCmdSetBlendConstants(wrapper_.cmdBuf_, color.toFloatPtr());
}

void CommandBuffer::cmdSetDepthBias(float depthBias, float slopeScale, float clamp) {
  dynamicState_.depthBiasEnable_ = true;
  vkCmdSetDepthBias(wrapper_.cmdBuf_, depthBias, clamp, slopeScale);
}

} // namespace igl::vulkan
