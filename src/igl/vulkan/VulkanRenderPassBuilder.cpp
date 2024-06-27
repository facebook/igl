/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanRenderPassBuilder.h"

// this cannot be put into namespace
bool operator==(const VkAttachmentDescription& a, const VkAttachmentDescription& b) {
#define CMP(field) (a.field == b.field)
  return CMP(flags) && CMP(format) && CMP(samples) && CMP(loadOp) && CMP(storeOp) &&
         CMP(stencilLoadOp) && CMP(stencilStoreOp) && CMP(initialLayout) && CMP(finalLayout);
#undef CMP
}

bool operator==(const VkAttachmentReference& a, const VkAttachmentReference& b) {
  return a.attachment == b.attachment && a.layout == b.layout;
}

namespace igl::vulkan {

VkResult VulkanRenderPassBuilder::build(const VulkanFunctionTable& vf,
                                        VkDevice device,
                                        VkRenderPass* outRenderPass,
                                        const char* debugName) const noexcept {
  IGL_ASSERT_MSG(
      refsColorResolve_.empty() || (refsColorResolve_.size() == refsColor_.size()),
      "If resolve attachments are used, there should be one color resolve attachment for each "
      "color attachment");

  const bool hasDepthStencilAttachment = refDepth_.layout != VK_IMAGE_LAYOUT_UNDEFINED;
  const VkSubpassDescription subpass =
      ivkGetSubpassDescription((uint32_t)refsColor_.size(),
                               refsColor_.data(),
                               refsColorResolve_.data(),
                               hasDepthStencilAttachment ? &refDepth_ : nullptr);
  const VkSubpassDependency dep = ivkGetSubpassDependency();
  const bool hasViewMask = viewMask_ != 0;

  const VkRenderPassMultiviewCreateInfo ci =
      ivkGetRenderPassMultiviewCreateInfo(&viewMask_, &correlationMask_);
  const VkResult result = ivkCreateRenderPass(&vf,
                                              device,
                                              (uint32_t)attachments_.size(),
                                              attachments_.data(),
                                              &subpass,
                                              &dep,
                                              hasViewMask ? &ci : nullptr,
                                              outRenderPass);
  if (!IGL_VERIFY(result == VK_SUCCESS)) {
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
  IGL_ASSERT_MSG(format != VK_FORMAT_UNDEFINED, "Invalid color attachment format");
  if (!refsColor_.empty()) {
    IGL_ASSERT_MSG(attachments_[refsColor_.back().attachment].samples == samples,
                   "All non-resolve attachments should have the sample number of samples");
  }
  refsColor_.push_back(ivkGetAttachmentReference((uint32_t)attachments_.size(),
                                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
  attachments_.push_back(
      ivkGetAttachmentDescription(format, loadOp, storeOp, initialLayout, finalLayout, samples));
  return *this;
}

VulkanRenderPassBuilder& VulkanRenderPassBuilder::addColorResolve(VkFormat format,
                                                                  VkAttachmentLoadOp loadOp,
                                                                  VkAttachmentStoreOp storeOp,
                                                                  VkImageLayout initialLayout,
                                                                  VkImageLayout finalLayout) {
  IGL_ASSERT_MSG(format != VK_FORMAT_UNDEFINED, "Invalid color resolve attachment format");
  refsColorResolve_.push_back(ivkGetAttachmentReference((uint32_t)attachments_.size(),
                                                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
  attachments_.push_back(ivkGetAttachmentDescription(
      format, loadOp, storeOp, initialLayout, finalLayout, VK_SAMPLE_COUNT_1_BIT));
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
  IGL_ASSERT_MSG(refDepth_.layout == VK_IMAGE_LAYOUT_UNDEFINED, "Can have only 1 depth attachment");
  IGL_ASSERT_MSG(format != VK_FORMAT_UNDEFINED, "Invalid depth attachment format");
  if (!refsColor_.empty()) {
    IGL_ASSERT_MSG(attachments_[refsColor_.back().attachment].samples == samples,
                   "All non-resolve attachments should have the sample number of samples "
                   "(including a depth attachment)");
  }
  refDepth_ = ivkGetAttachmentReference((uint32_t)attachments_.size(),
                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  auto desc =
      ivkGetAttachmentDescription(format, loadOp, storeOp, initialLayout, finalLayout, samples);
  desc.stencilLoadOp = stencilLoadOp;
  desc.stencilStoreOp = stencilStoreOp;
  attachments_.push_back(desc);
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
  IGL_ASSERT_MSG(refDepthResolve_.layout == VK_IMAGE_LAYOUT_UNDEFINED,
                 "Can have only 1 depth resolve attachment");
  IGL_ASSERT_MSG(format != VK_FORMAT_UNDEFINED, "Invalid depth resolve attachment format");
  refDepthResolve_ = ivkGetAttachmentReference((uint32_t)attachments_.size(),
                                               VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  auto desc = ivkGetAttachmentDescription(
      format, loadOp, storeOp, initialLayout, finalLayout, VK_SAMPLE_COUNT_1_BIT);
  desc.stencilLoadOp = stencilLoadOp;
  desc.stencilStoreOp = stencilStoreOp;
  attachments_.push_back(desc);
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
  return attachments_ == other.attachments_ && refsColor_ == other.refsColor_ &&
         refsColorResolve_ == other.refsColorResolve_ && refDepth_ == other.refDepth_ &&
         refDepthResolve_ == other.refDepthResolve_ && viewMask_ == other.viewMask_ &&
         correlationMask_ == other.correlationMask_;
}

uint64_t VulkanRenderPassBuilder::HashFunction::operator()(
    const VulkanRenderPassBuilder& builder) const {
  uint64_t hash = 0;
  for (const auto& a : builder.attachments_) {
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
  for (const auto& r : builder.refsColor_) {
    hash ^= std::hash<uint32_t>()(r.attachment);
    hash ^= std::hash<uint32_t>()(r.layout);
  }
  for (const auto& r : builder.refsColorResolve_) {
    hash ^= std::hash<uint32_t>()(r.attachment);
    hash ^= std::hash<uint32_t>()(r.layout);
  }
  hash ^= std::hash<uint32_t>()(builder.refDepth_.attachment);
  hash ^= std::hash<uint32_t>()(builder.refDepth_.layout);
  hash ^= std::hash<uint32_t>()(builder.refDepthResolve_.attachment);
  hash ^= std::hash<uint32_t>()(builder.refDepthResolve_.layout);
  hash ^= std::hash<uint32_t>()(builder.viewMask_);
  hash ^= std::hash<uint32_t>()(builder.correlationMask_);
  return hash;
}

} // namespace igl::vulkan
