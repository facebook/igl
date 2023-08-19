/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/CommandBuffer.h>

#include <lvk/vulkan/VulkanClasses.h>
#include <igl/vulkan/VulkanContext.h>

static bool isDepthOrStencilVkFormat(VkFormat format) {
  switch (format) {
  case VK_FORMAT_D16_UNORM:
  case VK_FORMAT_X8_D24_UNORM_PACK32:
  case VK_FORMAT_D32_SFLOAT:
  case VK_FORMAT_S8_UINT:
  case VK_FORMAT_D16_UNORM_S8_UINT:
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return true;
  default:
    return false;
  }
  return false;
}

namespace lvk::vulkan {

CommandBuffer::CommandBuffer(VulkanContext* ctx) : ctx_(ctx), wrapper_(&ctx_->immediate_->acquire()) {}

CommandBuffer::~CommandBuffer() {
  LVK_ASSERT(!isRendering_); // did you forget to call cmdEndRendering()?
}

namespace {

void transitionColorAttachment(VkCommandBuffer buffer, lvk::VulkanTexture* colorTex) {
  if (!LVK_VERIFY(colorTex)) {
    return;
  }

  const lvk::VulkanImage& colorImg = *colorTex->image_.get();
  if (!LVK_VERIFY(!colorImg.isDepthFormat_ && !colorImg.isStencilFormat_)) {
    LVK_ASSERT_MSG(false, "Color attachments cannot have depth/stencil formats");
    return;
  }
  LVK_ASSERT_MSG(colorImg.vkImageFormat_ != VK_FORMAT_UNDEFINED, "Invalid color attachment format");
  colorImg.transitionLayout(buffer,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // wait for all subsequent
                                                                                                          // fragment/compute shaders
                            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
}

VkAttachmentLoadOp loadOpToVkAttachmentLoadOp(lvk::LoadOp a) {
  using lvk::LoadOp;
  switch (a) {
  case LoadOp_Invalid:
    LVK_ASSERT(false);
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  case LoadOp_DontCare:
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  case LoadOp_Load:
    return VK_ATTACHMENT_LOAD_OP_LOAD;
  case LoadOp_Clear:
    return VK_ATTACHMENT_LOAD_OP_CLEAR;
  case LoadOp_None:
    return VK_ATTACHMENT_LOAD_OP_NONE_EXT;
  }
  LVK_ASSERT(false);
  return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

VkAttachmentStoreOp storeOpToVkAttachmentStoreOp(lvk::StoreOp a) {
  using lvk::StoreOp;
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
  LVK_ASSERT(false);
  return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

VkIndexType indexFormatToVkIndexType(lvk::IndexFormat fmt) {
  switch (fmt) {
  case lvk::IndexFormat_UI16:
    return VK_INDEX_TYPE_UINT16;
  case lvk::IndexFormat_UI32:
    return VK_INDEX_TYPE_UINT32;
  };
  LVK_ASSERT(false);
  return VK_INDEX_TYPE_NONE_KHR;
}

VkPrimitiveTopology primitiveTypeToVkPrimitiveTopology(lvk::PrimitiveType t) {
  switch (t) {
  case lvk::Primitive_Point:
    return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  case lvk::Primitive_Line:
    return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  case lvk::Primitive_LineStrip:
    return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
  case lvk::Primitive_Triangle:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  case lvk::Primitive_TriangleStrip:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  }
  LVK_ASSERT_MSG(false, "Implement PrimitiveType = %u", (uint32_t)t);
  return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

} // namespace

void CommandBuffer ::transitionToShaderReadOnly(TextureHandle handle) const {
  LVK_PROFILER_FUNCTION();

  const lvk::VulkanTexture& tex = *ctx_->texturesPool_.get(handle);
  const lvk::VulkanImage* img = tex.image_.get();

  LVK_ASSERT(!img->isSwapchainImage_);

  // transition only non-multisampled images - MSAA images cannot be accessed from shaders
  if (img->vkSamples_ == VK_SAMPLE_COUNT_1_BIT) {
    const VkImageAspectFlags flags = tex.image_->getImageAspectFlags();
    const VkPipelineStageFlags srcStage = isDepthOrStencilVkFormat(tex.image_->vkImageFormat_)
                                              ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                                              : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // set the result of the previous render pass
    img->transitionLayout(wrapper_->cmdBuf_,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          srcStage,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // wait for subsequent
                                                                                                        // fragment/compute shaders
                          VkImageSubresourceRange{flags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }
}

void CommandBuffer::cmdBindComputePipeline(lvk::ComputePipelineHandle handle) {
  LVK_PROFILER_FUNCTION();

  if (!LVK_VERIFY(!handle.empty())) {
    return;
  }

  VkPipeline* pipeline = ctx_->computePipelinesPool_.get(handle);

  LVK_ASSERT(pipeline);
  LVK_ASSERT(*pipeline != VK_NULL_HANDLE);

  if (lastPipelineBound_ != *pipeline) {
    lastPipelineBound_ = *pipeline;
    if (pipeline != VK_NULL_HANDLE) {
      vkCmdBindPipeline(wrapper_->cmdBuf_, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    }
  }
}

void CommandBuffer::cmdDispatchThreadGroups(const Dimensions& threadgroupCount, const Dependencies& deps) {
  LVK_ASSERT(!isRendering_);

  for (uint32_t i = 0; i != Dependencies::LVK_MAX_SUBMIT_DEPENDENCIES && deps.textures[i]; i++) {
    useComputeTexture(deps.textures[i]);
  }

  ctx_->checkAndUpdateDescriptorSets();
  ctx_->bindDefaultDescriptorSets(wrapper_->cmdBuf_, VK_PIPELINE_BIND_POINT_COMPUTE);

  vkCmdDispatch(wrapper_->cmdBuf_, threadgroupCount.width, threadgroupCount.height, threadgroupCount.depth);
}

void CommandBuffer::cmdPushDebugGroupLabel(const char* label, uint32_t colorRGBA) const {
  LVK_ASSERT(label);

  if (!label) {
    return;
  }
  const VkDebugUtilsLabelEXT utilsLabel = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pNext = nullptr,
      .pLabelName = label,
      .color = {float((colorRGBA >> 0) & 0xff) / 255.0f,
                float((colorRGBA >> 8) & 0xff) / 255.0f,
                float((colorRGBA >> 16) & 0xff) / 255.0f,
                float((colorRGBA >> 24) & 0xff) / 255.0f},
  };
  vkCmdBeginDebugUtilsLabelEXT(wrapper_->cmdBuf_, &utilsLabel);
}

void CommandBuffer::cmdInsertDebugEventLabel(const char* label, uint32_t colorRGBA) const {
  LVK_ASSERT(label);

  if (!label) {
    return;
  }
  const VkDebugUtilsLabelEXT utilsLabel = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pNext = nullptr,
      .pLabelName = label,
      .color = {float((colorRGBA >> 0) & 0xff) / 255.0f,
                float((colorRGBA >> 8) & 0xff) / 255.0f,
                float((colorRGBA >> 16) & 0xff) / 255.0f,
                float((colorRGBA >> 24) & 0xff) / 255.0f},
  };
  vkCmdInsertDebugUtilsLabelEXT(wrapper_->cmdBuf_, &utilsLabel);
}

void CommandBuffer::cmdPopDebugGroupLabel() const {
  vkCmdEndDebugUtilsLabelEXT(wrapper_->cmdBuf_);
}

void CommandBuffer::useComputeTexture(TextureHandle handle) {
  LVK_PROFILER_FUNCTION();

  LVK_ASSERT(!handle.empty());
  lvk::VulkanTexture* tex = ctx_->texturesPool_.get(handle);
  const lvk::VulkanImage& vkImage = *tex->image_.get();
  if (!vkImage.isStorageImage()) {
    LVK_ASSERT_MSG(false, "Did you forget to specify TextureUsageBits::Storage on your texture?");
    return;
  }

  // "frame graph" heuristics: if we are already in VK_IMAGE_LAYOUT_GENERAL, wait for the previous
  // compute shader
  const VkPipelineStageFlags srcStage = (vkImage.vkImageLayout_ == VK_IMAGE_LAYOUT_GENERAL) ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                                                                            : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  vkImage.transitionLayout(
      wrapper_->cmdBuf_,
      VK_IMAGE_LAYOUT_GENERAL,
      srcStage,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VkImageSubresourceRange{vkImage.getImageAspectFlags(), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
}

void CommandBuffer::cmdBeginRendering(const lvk::RenderPass& renderPass, const lvk::Framebuffer& fb) {
  LVK_PROFILER_FUNCTION();

  LVK_ASSERT(!isRendering_);

  isRendering_ = true;

  const uint32_t numFbColorAttachments = fb.getNumColorAttachments();
  const uint32_t numPassColorAttachments = renderPass.getNumColorAttachments();

  LVK_ASSERT(numPassColorAttachments == numFbColorAttachments);

  framebuffer_ = fb;

  // transition all the color attachments
  for (uint32_t i = 0; i != numFbColorAttachments; i++) {
    if (const auto handle = fb.color[i].texture) {
      lvk::VulkanTexture* colorTex = ctx_->texturesPool_.get(handle);
      transitionColorAttachment(wrapper_->cmdBuf_, colorTex);
    }
    // handle MSAA
    if (TextureHandle handle = fb.color[i].resolveTexture) {
      lvk::VulkanTexture* colorResolveTex = ctx_->texturesPool_.get(handle);
      transitionColorAttachment(wrapper_->cmdBuf_, colorResolveTex);
    }
  }
  // transition depth-stencil attachment
  TextureHandle depthTex = fb.depthStencil.texture;
  if (depthTex) {
    lvk::VulkanTexture& vkDepthTex = *ctx_->texturesPool_.get(depthTex);
    const lvk::VulkanImage* depthImg = vkDepthTex.image_.get();
    LVK_ASSERT_MSG(depthImg->vkImageFormat_ != VK_FORMAT_UNDEFINED, "Invalid depth attachment format");
    const VkImageAspectFlags flags = vkDepthTex.image_->getImageAspectFlags();
    depthImg->transitionLayout(wrapper_->cmdBuf_,
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

  VkRenderingAttachmentInfo colorAttachments[LVK_MAX_COLOR_ATTACHMENTS];

  for (uint32_t i = 0; i != numFbColorAttachments; i++) {
    const lvk::Framebuffer::AttachmentDesc& attachment = fb.color[i];
    LVK_ASSERT(!attachment.texture.empty());

    lvk::VulkanTexture& colorTexture = *ctx_->texturesPool_.get(attachment.texture);
    const auto& descColor = renderPass.color[i];
    if (mipLevel && descColor.level) {
      LVK_ASSERT_MSG(descColor.level == mipLevel, "All color attachments should have the same mip-level");
    }
    const VkExtent3D dim = colorTexture.getExtent();
    if (fbWidth) {
      LVK_ASSERT_MSG(dim.width == fbWidth, "All attachments should have the save width");
    }
    if (fbHeight) {
      LVK_ASSERT_MSG(dim.height == fbHeight, "All attachments should have the save width");
    }
    mipLevel = descColor.level;
    fbWidth = dim.width;
    fbHeight = dim.height;
    samples = colorTexture.image_->vkSamples_;
    colorAttachments[i] = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = colorTexture.getOrCreateVkImageViewForFramebuffer(descColor.level),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode = (samples > 1) ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = loadOpToVkAttachmentLoadOp(descColor.loadOp),
        .storeOp = storeOpToVkAttachmentStoreOp(descColor.storeOp),
        .clearValue =
            {.color = {.float32 = {descColor.clearColor[0], descColor.clearColor[1], descColor.clearColor[2], descColor.clearColor[3]}}},
    };
    // handle MSAA
    if (descColor.storeOp == StoreOp_MsaaResolve) {
      LVK_ASSERT(samples > 1);
      LVK_ASSERT_MSG(!attachment.resolveTexture.empty(), "Framebuffer attachment should contain a resolve texture");
      lvk::VulkanTexture& colorResolveTexture = *ctx_->texturesPool_.get(attachment.resolveTexture);
      colorAttachments[i].resolveImageView = colorResolveTexture.getOrCreateVkImageViewForFramebuffer(descColor.level);
      colorAttachments[i].resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
  }

  VkRenderingAttachmentInfo depthAttachment = {};

  if (fb.depthStencil.texture) {
    auto& depthTexture = *ctx_->texturesPool_.get(fb.depthStencil.texture);
    const auto& descDepth = renderPass.depth;
    LVK_ASSERT_MSG(descDepth.level == mipLevel, "Depth attachment should have the same mip-level as color attachments");
    depthAttachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = depthTexture.getOrCreateVkImageViewForFramebuffer(descDepth.level),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = loadOpToVkAttachmentLoadOp(descDepth.loadOp),
        .storeOp = storeOpToVkAttachmentStoreOp(descDepth.storeOp),
        .clearValue = {.depthStencil = {.depth = descDepth.clearDepth, .stencil = descDepth.clearStencil}},
    };
    const VkExtent3D dim = depthTexture.getExtent();
    if (fbWidth) {
      LVK_ASSERT_MSG(dim.width == fbWidth, "All attachments should have the save width");
    }
    if (fbHeight) {
      LVK_ASSERT_MSG(dim.height == fbHeight, "All attachments should have the save width");
    }
    mipLevel = descDepth.level;
    fbWidth = dim.width;
    fbHeight = dim.height;
  }

  const uint32_t width = std::max(fbWidth >> mipLevel, 1u);
  const uint32_t height = std::max(fbHeight >> mipLevel, 1u);
  const lvk::Viewport viewport = {0.0f, 0.0f, (float)width, (float)height, 0.0f, +1.0f};
  const lvk::ScissorRect scissor = {0, 0, width, height};

  VkRenderingAttachmentInfo stencilAttachment = depthAttachment;

  const bool isStencilFormat = renderPass.stencil.loadOp != lvk::LoadOp_Invalid;

  const VkRenderingInfo renderingInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .pNext = nullptr,
      .flags = 0,
      .renderArea = {VkOffset2D{(int32_t)scissor.x, (int32_t)scissor.y}, VkExtent2D{scissor.width, scissor.height}},
      .layerCount = 1,
      .viewMask = 0,
      .colorAttachmentCount = numFbColorAttachments,
      .pColorAttachments = colorAttachments,
      .pDepthAttachment = depthTex ? &depthAttachment : nullptr,
      .pStencilAttachment = isStencilFormat ? &stencilAttachment : nullptr,
  };

  cmdBindViewport(viewport);
  cmdBindScissorRect(scissor);

  ctx_->checkAndUpdateDescriptorSets();
  ctx_->bindDefaultDescriptorSets(wrapper_->cmdBuf_, VK_PIPELINE_BIND_POINT_GRAPHICS);

  vkCmdBeginRendering(wrapper_->cmdBuf_, &renderingInfo);
}

void CommandBuffer::cmdEndRendering() {
  LVK_ASSERT(isRendering_);

  isRendering_ = false;

  vkCmdEndRendering(wrapper_->cmdBuf_);

  const uint32_t numFbColorAttachments = framebuffer_.getNumColorAttachments();

  // set image layouts after the render pass
  for (uint32_t i = 0; i != numFbColorAttachments; i++) {
    const auto& attachment = framebuffer_.color[i];
    const VulkanTexture& tex = *ctx_->texturesPool_.get(attachment.texture);
    // this must match the final layout of the render pass
    tex.image_->vkImageLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  if (framebuffer_.depthStencil.texture) {
    const VulkanTexture& tex = *ctx_->texturesPool_.get(framebuffer_.depthStencil.texture);
    // this must match the final layout of the render pass
    tex.image_->vkImageLayout_ = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
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
  vkCmdSetViewport(wrapper_->cmdBuf_, 0, 1, &vp);
}

void CommandBuffer::cmdBindScissorRect(const ScissorRect& rect) {
  const VkRect2D scissor = {
      VkOffset2D{(int32_t)rect.x, (int32_t)rect.y},
      VkExtent2D{rect.width, rect.height},
  };
  vkCmdSetScissor(wrapper_->cmdBuf_, 0, 1, &scissor);
}

void CommandBuffer::cmdBindRenderPipeline(lvk::RenderPipelineHandle handle) {
  if (!LVK_VERIFY(!handle.empty())) {
    return;
  }

  currentPipeline_ = handle;

  const lvk::vulkan::RenderPipelineState* rps = ctx_->renderPipelinesPool_.get(handle);

  LVK_ASSERT(rps);

  const RenderPipelineDesc& desc = rps->getRenderPipelineDesc();

  const bool hasDepthAttachmentPipeline = desc.depthFormat != Format_Invalid;
  const bool hasDepthAttachmentPass = !framebuffer_.depthStencil.texture.empty();

  if (hasDepthAttachmentPipeline != hasDepthAttachmentPass) {
    LVK_ASSERT(false);
    LLOGW("Make sure your render pass and render pipeline both have matching depth attachments");
  }

  lastPipelineBound_ = VK_NULL_HANDLE;
}

void CommandBuffer::cmdBindDepthState(const DepthState& desc) {
  LVK_PROFILER_FUNCTION();

  dynamicState_.depthWriteEnable_ = desc.isDepthWriteEnabled;
  dynamicState_.setDepthCompareOp(compareOpToVkCompareOp(desc.compareOp));
}

void CommandBuffer::cmdBindVertexBuffer(uint32_t index, BufferHandle buffer, size_t bufferOffset) {
  LVK_PROFILER_FUNCTION();

  if (!LVK_VERIFY(!buffer.empty())) {
    return;
  }

  lvk::VulkanBuffer* buf = ctx_->buffersPool_.get(buffer);

  VkBuffer vkBuf = buf->vkBuffer_;

  LVK_ASSERT(buf->vkUsageFlags_ & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

  const VkDeviceSize offset = bufferOffset;
  vkCmdBindVertexBuffers(wrapper_->cmdBuf_, index, 1, &vkBuf, &offset);
}

void CommandBuffer::cmdBindIndexBuffer(BufferHandle indexBuffer, IndexFormat indexFormat, size_t indexBufferOffset) {
  lvk::VulkanBuffer* buf = ctx_->buffersPool_.get(indexBuffer);

  const VkIndexType type = indexFormatToVkIndexType(indexFormat);
  vkCmdBindIndexBuffer(wrapper_->cmdBuf_, buf->vkBuffer_, indexBufferOffset, type);
}

void CommandBuffer::cmdPushConstants(const void* data, size_t size, size_t offset) {
  LVK_PROFILER_FUNCTION();

  LVK_ASSERT(size % 4 == 0); // VUID-vkCmdPushConstants-size-00369: size must be a multiple of 4

  // check push constant size is within max size
  const VkPhysicalDeviceLimits& limits = ctx_->getVkPhysicalDeviceProperties().limits;
  if (!LVK_VERIFY(size + offset <= limits.maxPushConstantsSize)) {
    LLOGW("Push constants size exceeded %u (max %u bytes)", size + offset, limits.maxPushConstantsSize);
  }

  vkCmdPushConstants(wrapper_->cmdBuf_,
                     ctx_->vkPipelineLayout_,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                     (uint32_t)offset,
                     (uint32_t)size,
                     data);
}

void CommandBuffer::bindGraphicsPipeline() {
  const lvk::vulkan::RenderPipelineState* rps = ctx_->renderPipelinesPool_.get(currentPipeline_);

  if (!LVK_VERIFY(rps)) {
    return;
  }

  VkPipeline pipeline = rps->getVkPipeline(dynamicState_);

  if (lastPipelineBound_ != pipeline) {
    lastPipelineBound_ = pipeline;
    if (pipeline != VK_NULL_HANDLE) {
      vkCmdBindPipeline(wrapper_->cmdBuf_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }
  }
}

void CommandBuffer::cmdDraw(PrimitiveType primitiveType, size_t vertexStart, size_t vertexCount) {
  LVK_PROFILER_FUNCTION();

  if (vertexCount == 0) {
    return;
  }

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  bindGraphicsPipeline();

  vkCmdDraw(wrapper_->cmdBuf_, (uint32_t)vertexCount, 1, (uint32_t)vertexStart, 0);
}

void CommandBuffer::cmdDrawIndexed(PrimitiveType primitiveType,
                                   uint32_t indexCount,
                                   uint32_t instanceCount,
                                   uint32_t firstIndex,
                                   int32_t vertexOffset,
                                   uint32_t baseInstance) {
  LVK_PROFILER_FUNCTION();

  if (indexCount == 0) {
    return;
  }

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  bindGraphicsPipeline();

  vkCmdDrawIndexed(wrapper_->cmdBuf_, indexCount, instanceCount, firstIndex, vertexOffset, baseInstance);
}

void CommandBuffer::cmdDrawIndirect(PrimitiveType primitiveType,
                                    BufferHandle indirectBuffer,
                                    size_t indirectBufferOffset,
                                    uint32_t drawCount,
                                    uint32_t stride) {
  LVK_PROFILER_FUNCTION();

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  bindGraphicsPipeline();

  lvk::VulkanBuffer* bufIndirect = ctx_->buffersPool_.get(indirectBuffer);

  vkCmdDrawIndirect(
      wrapper_->cmdBuf_, bufIndirect->vkBuffer_, indirectBufferOffset, drawCount, stride ? stride : sizeof(VkDrawIndirectCommand));
}

void CommandBuffer::cmdDrawIndexedIndirect(PrimitiveType primitiveType,
                                           BufferHandle indirectBuffer,
                                           size_t indirectBufferOffset,
                                           uint32_t drawCount,
                                           uint32_t stride) {
  LVK_PROFILER_FUNCTION();

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  bindGraphicsPipeline();

  lvk::VulkanBuffer* bufIndirect = ctx_->buffersPool_.get(indirectBuffer);

  vkCmdDrawIndexedIndirect(wrapper_->cmdBuf_,
                           bufIndirect->vkBuffer_,
                           indirectBufferOffset,
                           drawCount,
                           stride ? stride : sizeof(VkDrawIndexedIndirectCommand));
}

void CommandBuffer::cmdSetBlendColor(const float color[4]) {
  vkCmdSetBlendConstants(wrapper_->cmdBuf_, color);
}

void CommandBuffer::cmdSetDepthBias(float depthBias, float slopeScale, float clamp) {
  dynamicState_.depthBiasEnable_ = true;
  vkCmdSetDepthBias(wrapper_->cmdBuf_, depthBias, clamp, slopeScale);
}

} // namespace lvk::vulkan
