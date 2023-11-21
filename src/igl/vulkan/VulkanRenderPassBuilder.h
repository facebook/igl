/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/vulkan/Common.h>

#include <igl/vulkan/VulkanFunctions.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <vector>

bool operator==(const VkAttachmentDescription& a, const VkAttachmentDescription& b);
bool operator==(const VkAttachmentReference& a, const VkAttachmentReference& b);

namespace igl {
namespace vulkan {

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
  VulkanRenderPassBuilder& addDepth(
      VkFormat format,
      VkAttachmentLoadOp loadOp,
      VkAttachmentStoreOp storeOp,
      VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
  VulkanRenderPassBuilder& addDepthResolve(
      VkFormat format,
      VkAttachmentLoadOp loadOp,
      VkAttachmentStoreOp storeOp,
      VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  VulkanRenderPassBuilder& setMultiviewMasks(const uint32_t viewMask,
                                             const uint32_t correlationMask);

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
  std::vector<VkAttachmentDescription> attachments_;
  std::vector<VkAttachmentReference> refsColor_;
  std::vector<VkAttachmentReference> refsColorResolve_;
  VkAttachmentReference refDepth_ = {};
  VkAttachmentReference refDepthResolve_ = {};
  uint32_t viewMask_ = 0;
  uint32_t correlationMask_ = 0;
};

} // namespace vulkan
} // namespace igl
