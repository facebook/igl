/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#define IGL_VULKAN_HAS_LEGACY_RENDERPASS 1

#include <array>
#include <vector>
#include <igl/RenderPass.h>
#include <igl/vulkan/Common.h>

bool operator==(const VkAttachmentDescription2& a, const VkAttachmentDescription2& b);
bool operator==(const VkAttachmentReference2& a, const VkAttachmentReference2& b);

namespace igl::vulkan {

/// @brief A helper class to build VkRenderPass objects.
class VulkanRenderPassBuilder final {
 public:
  VulkanRenderPassBuilder() = default;

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

  /// @brief Splits the render pass into subpasses. Bit i of each mask refers to the i-th color
  /// attachment added with addColor(). `numSubpasses` must be 0 (a single subpass that writes
  /// every color attachment) or between 2 and IGL_SUBPASSES_MAX, any other count removes the
  /// subpasses. The masks must follow RenderPassDesc::validate(): a subpass cannot read what it
  /// writes, and only reads what an earlier subpass writes. A render pass with subpasses is only
  /// created with vkCreateRenderPass2() and cannot have resolve attachments.
  VulkanRenderPassBuilder& setSubpasses(const RenderPassDesc::SubpassDesc* IGL_NULLABLE subpasses,
                                        uint32_t numSubpasses);

  /// @brief The number of subpasses of the render pass: 1 unless setSubpasses() was called.
  [[nodiscard]] uint32_t getNumSubpasses() const {
    return numSubpasses_ > 0 ? numSubpasses_ : 1u;
  }

  /// @brief A copy of this builder without what render pass compatibility ignores: the load and
  /// store ops and the initial and final layouts of the attachments. The color attachments that
  /// the subpasses preserve, which depend on the store ops, are part of the key instead. Builders
  /// with equal keys describe compatible render passes. A key is not meant to be built.
  [[nodiscard]] VulkanRenderPassBuilder compatibilityKey() const;

  /// @brief The color attachments (bit i = i-th color attachment) that build() preserves in
  /// `subpass`: not used by `subpass`, used by an earlier subpass, and either used by a later
  /// subpass or stored at the end of the render pass.
  [[nodiscard]] uint32_t getPreserveMask(uint32_t subpass) const;

  /// @brief The number of subpass dependencies of the render pass: the ones between subpasses and
  /// the ones from the subpasses that are the last to use an attachment to VK_SUBPASS_EXTERNAL.
  [[nodiscard]] uint32_t getNumDependencies() const;

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
  VkResult buildWithSubpasses(const VulkanFunctionTable& vf,
                              VkDevice device,
                              VkRenderPass* outRenderPass,
                              const char* debugName) const noexcept;

  // at most one dependency for every pair of subpasses (src < dst), and one from every subpass to
  // VK_SUBPASS_EXTERNAL
  static constexpr uint32_t kMaxSubpassDependencies =
      IGL_SUBPASSES_MAX * (IGL_SUBPASSES_MAX - 1) / 2 + IGL_SUBPASSES_MAX;

  struct SubpassDependencyPair {
    uint32_t srcSubpass = 0;
    uint32_t dstSubpass = 0; // VK_SUBPASS_EXTERNAL for the dependencies of later commands
  };

  struct SubpassDependencies {
    std::array<SubpassDependencyPair, kMaxSubpassDependencies> pairs = {};
    uint32_t count = 0;
  };

  // The dependencies of a render pass with subpasses.
  [[nodiscard]] SubpassDependencies getSubpassDependencies() const;

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
  // the first numSubpasses_ entries are valid, the rest are zero
  std::array<RenderPassDesc::SubpassDesc, IGL_SUBPASSES_MAX> subpasses_ = {};
  uint32_t numSubpasses_ = 0;
  // Only compatibilityKey() sets these, to the preserve masks of the builder that it copies. They
  // are zero in all other builders.
  std::array<uint32_t, IGL_SUBPASSES_MAX> compatPreserveMasks_ = {};
};

} // namespace igl::vulkan
