/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#define IGL_VULKAN_HAS_LEGACY_RENDERPASS 1

#include <igl/vulkan/Common.h>

#include <vector>

bool operator==(const VkAttachmentDescription2& a, const VkAttachmentDescription2& b);
bool operator==(const VkAttachmentReference2& a, const VkAttachmentReference2& b);

namespace igl::vulkan {

/// @brief A helper class to build VkRenderPass objects.
class VulkanRenderPassBuilder final {
 public:
  VulkanRenderPassBuilder() = default;
  ~VulkanRenderPassBuilder() = default;

  VulkanRenderPassBuilder& addColor(
      VkFormat format,
      VkAttachmentLoadOp loadOp,
      VkAttachmentStoreOp storeOp,
      VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
  VulkanRenderPassBuilder& addColorResolve(
      VkFormat format,
      VkAttachmentLoadOp loadOp,
      VkAttachmentStoreOp storeOp,
      VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VulkanRenderPassBuilder& addDepthStencil(
      VkFormat format,
      VkAttachmentLoadOp loadOp,
      VkAttachmentStoreOp storeOp,
      VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
  VulkanRenderPassBuilder& addDepthStencilResolve(
      VkFormat format,
      VkAttachmentLoadOp loadOp,
      VkAttachmentStoreOp storeOp,
      VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  VulkanRenderPassBuilder& setMultiviewMasks(uint32_t viewMask, uint32_t correlationMask);

  // comparison operator and a hash function for std::unordered_map<>
  bool operator==(const VulkanRenderPassBuilder& other) const;

  struct HashFunction {
    uint64_t operator()(const VulkanRenderPassBuilder& builder) const;
  };

 private:
  // Only VulkanContext is allowed to create actual render passes. Use
  // VulkanContext::findRenderPass()
  friend class VulkanContext;
  VkResult build(const VulkanFunctionTable& vf,
                 VkDevice device,
                 VkRenderPass* outRenderPass,
                 const char* debugName = nullptr) const noexcept;

 private:
#if IGL_VULKAN_HAS_LEGACY_RENDERPASS
  // old code path for vkCreateRenderPass() - to be removed later
  std::vector<VkAttachmentDescription> attachments_;
  std::vector<VkAttachmentReference> refsColor_;
  std::vector<VkAttachmentReference> refsColorResolve_;
  VkAttachmentReference refDepth_ = {};
  VkAttachmentReference refDepthResolve_ = {};
#endif // IGL_VULKAN_HAS_LEGACY_RENDERPASS
  // new code path for vkCreateRenderPass2()
  std::vector<VkAttachmentDescription2> attachments2_;
  std::vector<VkAttachmentReference2> refsColor2_;
  std::vector<VkAttachmentReference2> refsColorResolve2_;
  VkAttachmentReference2 refDepth2_ = {};
  VkAttachmentReference2 refDepthResolve2_ = {};
  uint32_t viewMask_ = 0;
  uint32_t correlationMask_ = 0;
};

} // namespace igl::vulkan
