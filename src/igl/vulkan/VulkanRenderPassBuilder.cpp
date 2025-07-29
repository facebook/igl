/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanRenderPassBuilder.h"

// this cannot be put into namespace
#define CMP(field) (a.field == b.field)
bool operator==(const VkAttachmentDescription& a, const VkAttachmentDescription& b) {
  return CMP(flags) && CMP(format) && CMP(samples) && CMP(loadOp) && CMP(storeOp) &&
         CMP(stencilLoadOp) && CMP(stencilStoreOp) && CMP(initialLayout) && CMP(finalLayout);
}

bool operator==(const VkAttachmentReference& a, const VkAttachmentReference& b) {
  return a.attachment == b.attachment && a.layout == b.layout;
}

bool operator==(const VkAttachmentDescription2& a, const VkAttachmentDescription2& b) {
  return CMP(sType) && CMP(pNext) && CMP(flags) && CMP(format) && CMP(samples) && CMP(loadOp) &&
         CMP(storeOp) && CMP(stencilLoadOp) && CMP(stencilStoreOp) && CMP(initialLayout) &&
         CMP(finalLayout);
}

bool operator==(const VkAttachmentReference2& a, const VkAttachmentReference2& b) {
  return CMP(sType) && CMP(pNext) && CMP(attachment) && CMP(layout) && CMP(aspectMask);
}
#undef CMP

namespace igl::vulkan {

VkResult VulkanRenderPassBuilder::build(const VulkanFunctionTable& vf,
                                        VkDevice device,
                                        VkRenderPass* outRenderPass,
                                        const char* debugName) const noexcept {
  IGL_DEBUG_ASSERT(
      refsColorResolve2_.empty() || (refsColorResolve2_.size() == refsColor2_.size()),
      "If resolve attachments are used, there should be one color resolve attachment for each "
      "color attachment");

  const bool hasDepthStencilAttachment = refDepth2_.layout != VK_IMAGE_LAYOUT_UNDEFINED;

#if IGL_VULKAN_HAS_LEGACY_RENDERPASS
  const VkSubpassDescription subpass = {
      .flags = 0,
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = (uint32_t)refsColor_.size(),
      .pColorAttachments = refsColor_.data(),
      .pResolveAttachments = refsColorResolve_.data(),
      .pDepthStencilAttachment = hasDepthStencilAttachment ? &refDepth_ : nullptr,
  };

  const VkSubpassDependency dep = {
      .srcSubpass = 0,
      .dstSubpass = VK_SUBPASS_EXTERNAL,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
  };

  const VkRenderPassMultiviewCreateInfo renderPassMultiview = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
      .subpassCount = 1,
      .pViewMasks = &viewMask_,
      .correlationMaskCount = 1,
      .pCorrelationMasks = &correlationMask_,
  };

  const VkRenderPassCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .pNext = viewMask_ ? &renderPassMultiview : nullptr,
      .attachmentCount = (uint32_t)attachments_.size(),
      .pAttachments = attachments_.data(),
      .subpassCount = 1,
      .pSubpasses = &subpass,
      .dependencyCount = 1,
      .pDependencies = &dep,
  };
#endif // IGL_VULKAN_HAS_LEGACY_RENDERPASS

  const VkSubpassDescription2 subpass2 = {
      .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
      .flags = 0,
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .viewMask = viewMask_,
      .colorAttachmentCount = (uint32_t)refsColor2_.size(),
      .pColorAttachments = refsColor2_.data(),
      .pResolveAttachments = refsColorResolve2_.data(),
      .pDepthStencilAttachment = hasDepthStencilAttachment ? &refDepth2_ : nullptr,
  };

  const VkSubpassDependency2 dep2 = {
      .sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
      .srcSubpass = 0,
      .dstSubpass = VK_SUBPASS_EXTERNAL,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
      .dependencyFlags = 0,
      .viewOffset = 0,
  };

  const VkRenderPassCreateInfo2 ci2 = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
      .flags = 0,
      .attachmentCount = (uint32_t)attachments2_.size(),
      .pAttachments = attachments2_.data(),
      .subpassCount = 1,
      .pSubpasses = &subpass2,
      .dependencyCount = 1,
      .pDependencies = &dep2,
      .correlatedViewMaskCount = viewMask_ ? 1u : 0u,
      .pCorrelatedViewMasks = viewMask_ ? &correlationMask_ : nullptr,
  };
  const VkResult result =

#if IGL_VULKAN_HAS_LEGACY_RENDERPASS
      vf.vkCreateRenderPass2 ? vf.vkCreateRenderPass2(device, &ci2, nullptr, outRenderPass)
                             : vf.vkCreateRenderPass(device, &ci, nullptr, outRenderPass);
#else
      vf.vkCreateRenderPass2(device, &ci2, nullptr, outRenderPass);
#endif // IGL_VULKAN_HAS_LEGACY_RENDERPASS

  if (!IGL_DEBUG_VERIFY(result == VK_SUCCESS)) {
    return result;
  }

  // set debug name
  return ivkSetDebugObjectName(
      &vf, device, VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)*outRenderPass, debugName);
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addColor(VkFormat format,
                                                           VkAttachmentLoadOp loadOp,
                                                           VkAttachmentStoreOp storeOp,
                                                           VkImageLayout initialLayout,
                                                           VkImageLayout finalLayout,
                                                           VkSampleCountFlagBits samples) {
  IGL_DEBUG_ASSERT(format != VK_FORMAT_UNDEFINED, "Invalid color attachment format");
  if (!refsColor2_.empty()) {
    IGL_DEBUG_ASSERT(attachments2_[refsColor2_.back().attachment].samples == samples,
                     "All non-resolve attachments should have the sample number of samples");
  }

#if IGL_VULKAN_HAS_LEGACY_RENDERPASS
  refsColor_.push_back(VkAttachmentReference{
      .attachment = (uint32_t)attachments_.size(),
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  });
  attachments_.push_back(VkAttachmentDescription{
      .flags = 0,
      .format = format,
      .samples = samples,
      .loadOp = loadOp,
      .storeOp = storeOp,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = initialLayout,
      .finalLayout = finalLayout,
  });
#endif // IGL_VULKAN_HAS_LEGACY_RENDERPASS
  refsColor2_.push_back(VkAttachmentReference2{
      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
      .attachment = (uint32_t)attachments2_.size(),
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
  });

  attachments2_.push_back(VkAttachmentDescription2{
      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
      .flags = 0,
      .format = format,
      .samples = samples,
      .loadOp = loadOp,
      .storeOp = storeOp,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = initialLayout,
      .finalLayout = finalLayout,
  });

  return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addColorResolve(VkFormat format,
                                                                  VkAttachmentLoadOp loadOp,
                                                                  VkAttachmentStoreOp storeOp,
                                                                  VkImageLayout initialLayout,
                                                                  VkImageLayout finalLayout) {
  IGL_DEBUG_ASSERT(format != VK_FORMAT_UNDEFINED, "Invalid color resolve attachment format");

#if IGL_VULKAN_HAS_LEGACY_RENDERPASS
  refsColorResolve_.push_back(VkAttachmentReference{
      .attachment = (uint32_t)attachments_.size(),
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  });
  attachments_.push_back(VkAttachmentDescription{
      .flags = 0,
      .format = format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = loadOp,
      .storeOp = storeOp,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = initialLayout,
      .finalLayout = finalLayout,
  });
#endif // IGL_VULKAN_HAS_LEGACY_RENDERPASS

  refsColorResolve2_.push_back(VkAttachmentReference2{
      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
      .attachment = (uint32_t)attachments2_.size(),
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
  });

  attachments2_.push_back(VkAttachmentDescription2{
      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
      .flags = 0,
      .format = format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = loadOp,
      .storeOp = storeOp,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = initialLayout,
      .finalLayout = finalLayout,
  });

  return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addDepthStencil(
    VkFormat format,
    VkAttachmentLoadOp loadOp,
    VkAttachmentStoreOp storeOp,
    VkAttachmentLoadOp stencilLoadOp,
    VkAttachmentStoreOp stencilStoreOp,
    VkImageLayout initialLayout,
    VkImageLayout finalLayout,
    VkSampleCountFlagBits samples) {
  IGL_DEBUG_ASSERT(refDepth2_.layout == VK_IMAGE_LAYOUT_UNDEFINED,
                   "Can have only 1 depth attachment");
  IGL_DEBUG_ASSERT(format != VK_FORMAT_UNDEFINED, "Invalid depth attachment format");
  if (!refsColor2_.empty()) {
    IGL_DEBUG_ASSERT(attachments2_[refsColor2_.back().attachment].samples == samples,
                     "All non-resolve attachments should have the sample number of samples "
                     "(including a depth attachment)");
  }

#if IGL_VULKAN_HAS_LEGACY_RENDERPASS
  refDepth_ = VkAttachmentReference{
      .attachment = (uint32_t)attachments_.size(),
      .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  attachments_.push_back(VkAttachmentDescription{
      .flags = 0,
      .format = format,
      .samples = samples,
      .loadOp = loadOp,
      .storeOp = storeOp,
      .stencilLoadOp = stencilLoadOp,
      .stencilStoreOp = stencilStoreOp,
      .initialLayout = initialLayout,
      .finalLayout = finalLayout,
  });
#endif // IGL_VULKAN_HAS_LEGACY_RENDERPASS

  refDepth2_ = VkAttachmentReference2{
      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
      .attachment = (uint32_t)attachments2_.size(),
      .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .aspectMask = (hasDepth(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VkImageAspectFlags(0)) |
                    (hasStencil(format) ? VK_IMAGE_ASPECT_STENCIL_BIT : VkImageAspectFlags(0)),
  };

  attachments2_.push_back(VkAttachmentDescription2{
      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
      .flags = 0,
      .format = format,
      .samples = samples,
      .loadOp = loadOp,
      .storeOp = storeOp,
      .stencilLoadOp = stencilLoadOp,
      .stencilStoreOp = stencilStoreOp,
      .initialLayout = initialLayout,
      .finalLayout = finalLayout,
  });

  return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addDepthStencilResolve(
    VkFormat format,
    VkAttachmentLoadOp loadOp,
    VkAttachmentStoreOp storeOp,
    VkAttachmentLoadOp stencilLoadOp,
    VkAttachmentStoreOp stencilStoreOp,
    VkImageLayout initialLayout,
    VkImageLayout finalLayout) {
  IGL_DEBUG_ASSERT(refDepthResolve2_.layout == VK_IMAGE_LAYOUT_UNDEFINED,
                   "Can have only 1 depth resolve attachment");
  IGL_DEBUG_ASSERT(format != VK_FORMAT_UNDEFINED, "Invalid depth resolve attachment format");
#if IGL_VULKAN_HAS_LEGACY_RENDERPASS
  refDepthResolve_ = VkAttachmentReference{
      .attachment = (uint32_t)attachments_.size(),
      .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  attachments_.push_back(VkAttachmentDescription{
      .flags = 0,
      .format = format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = loadOp,
      .storeOp = storeOp,
      .stencilLoadOp = stencilLoadOp,
      .stencilStoreOp = stencilStoreOp,
      .initialLayout = initialLayout,
      .finalLayout = finalLayout,
  });
#endif // IGL_VULKAN_HAS_LEGACY_RENDERPASS

  refDepthResolve2_ = VkAttachmentReference2{
      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
      .attachment = (uint32_t)attachments2_.size(),
      .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .aspectMask = (hasDepth(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VkImageAspectFlags(0)) |
                    (hasStencil(format) ? VK_IMAGE_ASPECT_STENCIL_BIT : VkImageAspectFlags(0)),
  };

  attachments2_.push_back(VkAttachmentDescription2{
      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
      .flags = 0,
      .format = format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = loadOp,
      .storeOp = storeOp,
      .stencilLoadOp = stencilLoadOp,
      .stencilStoreOp = stencilStoreOp,
      .initialLayout = initialLayout,
      .finalLayout = finalLayout,
  });

  return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setMultiviewMasks(
    const uint32_t viewMask,
    const uint32_t correlationMask) {
  viewMask_ = viewMask;
  correlationMask_ = correlationMask;
  return *this;
}

bool VulkanRenderPassBuilder::operator==(const VulkanRenderPassBuilder& other) const {
  return attachments2_ == other.attachments2_ && refsColor2_ == other.refsColor2_ &&
         refsColorResolve2_ == other.refsColorResolve2_ && refDepth2_ == other.refDepth2_ &&
         refDepthResolve2_ == other.refDepthResolve2_ && viewMask_ == other.viewMask_ &&
         correlationMask_ == other.correlationMask_;
}

uint64_t VulkanRenderPassBuilder::HashFunction::operator()(
    const VulkanRenderPassBuilder& builder) const {
  uint64_t hash = 0;
  for (const auto& a : builder.attachments2_) {
    hash ^= std::hash<uint32_t>()(a.flags);
    hash ^= std::hash<uint32_t>()(a.format);
    hash ^= std::hash<uint32_t>()(a.samples);
    hash ^= std::hash<uint32_t>()(a.loadOp);
    hash ^= std::hash<uint32_t>()(a.storeOp);
    hash ^= std::hash<uint32_t>()(a.stencilLoadOp);
    hash ^= std::hash<uint32_t>()(a.stencilStoreOp);
    hash ^= std::hash<uint32_t>()(a.initialLayout);
    hash ^= std::hash<uint32_t>()(a.finalLayout);
  }
  for (const auto& r : builder.refsColor2_) {
    hash ^= std::hash<uint32_t>()(r.attachment);
    hash ^= std::hash<uint32_t>()(r.layout);
    hash ^= std::hash<uint32_t>()(r.aspectMask);
  }
  for (const auto& r : builder.refsColorResolve2_) {
    hash ^= std::hash<uint32_t>()(r.attachment);
    hash ^= std::hash<uint32_t>()(r.layout);
    hash ^= std::hash<uint32_t>()(r.aspectMask);
  }
  hash ^= std::hash<uint32_t>()(builder.refDepth2_.attachment);
  hash ^= std::hash<uint32_t>()(builder.refDepth2_.layout);
  hash ^= std::hash<uint32_t>()(builder.refDepth2_.aspectMask);
  hash ^= std::hash<uint32_t>()(builder.refDepthResolve2_.attachment);
  hash ^= std::hash<uint32_t>()(builder.refDepthResolve2_.layout);
  hash ^= std::hash<uint32_t>()(builder.refDepthResolve2_.aspectMask);
  hash ^= std::hash<uint32_t>()(builder.viewMask_);
  hash ^= std::hash<uint32_t>()(builder.correlationMask_);
  return hash;
}

} // namespace igl::vulkan
