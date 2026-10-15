/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanRenderPassBuilder.h"

#include <algorithm>
#include <igl/Macros.h>

// this cannot be put into namespace
#define CMP(field) (a.field == b.field)
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

namespace {

constexpr bool hasBit(uint32_t mask, uint32_t bit) {
  return ((mask >> bit) & 1u) != 0;
}

// the mask with the lowest `numBits` bits set
constexpr uint32_t lowBitsMask(uint32_t numBits) {
  return (1u << numBits) - 1u;
}

// Whether the subpasses follow the rules of RenderPassDesc::validate(): a subpass does not read
// what it writes, and only reads what an earlier subpass wrote.
[[maybe_unused]] bool hasValidSubpassMasks(
    const std::array<RenderPassDesc::SubpassDesc, IGL_SUBPASSES_MAX>& subpasses,
    uint32_t numSubpasses) {
  uint32_t writtenMask = 0;
  for (uint32_t k = 0; k != numSubpasses; ++k) {
    const uint32_t colorMask = subpasses[k].colorAttachmentMask;
    const uint32_t inputMask = subpasses[k].inputAttachmentMask;
    if ((colorMask & inputMask) != 0 || (inputMask & ~writtenMask) != 0) {
      return false;
    }
    writtenMask |= colorMask;
  }
  return true;
}

} // namespace

VkResult VulkanRenderPassBuilder::build(const VulkanFunctionTable& vf,
                                        VkDevice device,
                                        VkRenderPass* outRenderPass,
                                        const char* debugName) const noexcept {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  IGL_DEBUG_ASSERT(
      refsColorResolve2_.empty() || (refsColorResolve2_.size() == refsColor2_.size()),
      "If resolve attachments are used, there should be one color resolve attachment for each "
      "color attachment");

  if (numSubpasses_ > 1) {
    return buildWithSubpasses(vf, device, outRenderPass, debugName);
  }

  const bool hasDepthStencilAttachment = refDepth2_.layout != VK_IMAGE_LAYOUT_UNDEFINED;

#if IGL_VULKAN_HAS_LEGACY_RENDERPASS
  const VkSubpassDescription subpass = {
      .flags = 0,
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = static_cast<uint32_t>(refsColor_.size()),
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
      .attachmentCount = static_cast<uint32_t>(attachments_.size()),
      .pAttachments = attachments_.data(),
      .subpassCount = 1,
      .pSubpasses = &subpass,
      .dependencyCount = 1,
      .pDependencies = &dep,
  };
#endif // IGL_VULKAN_HAS_LEGACY_RENDERPASS

  const bool hasDepthStencilResolve = refDepthResolve2_.layout != VK_IMAGE_LAYOUT_UNDEFINED;

  const VkSubpassDescriptionDepthStencilResolve depthStencilResolve = {
      .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE,
      .pNext = nullptr,
      .depthResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,
      .stencilResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,
      .pDepthStencilResolveAttachment = hasDepthStencilResolve ? &refDepthResolve2_ : nullptr,
  };

  const VkSubpassDescription2 subpass2 = {
      .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
      .pNext = hasDepthStencilResolve ? &depthStencilResolve : nullptr,
      .flags = 0,
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .viewMask = viewMask_,
      .colorAttachmentCount = static_cast<uint32_t>(refsColor2_.size()),
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
      .attachmentCount = static_cast<uint32_t>(attachments2_.size()),
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

VkResult VulkanRenderPassBuilder::buildWithSubpasses(const VulkanFunctionTable& vf,
                                                     VkDevice device,
                                                     VkRenderPass* outRenderPass,
                                                     const char* debugName) const noexcept {
  if (!IGL_DEBUG_VERIFY(vf.vkCreateRenderPass2 != nullptr,
                        "Render passes with subpasses require vkCreateRenderPass2()")) {
    return VK_ERROR_FEATURE_NOT_PRESENT;
  }
  const uint32_t numColorAttachments = static_cast<uint32_t>(refsColor2_.size());
  if (!IGL_DEBUG_VERIFY(numColorAttachments <= IGL_COLOR_ATTACHMENTS_MAX,
                        "Too many color attachments")) {
    return VK_ERROR_TOO_MANY_OBJECTS;
  }
  if (!refsColorResolve2_.empty() || refDepthResolve2_.layout != VK_IMAGE_LAYOUT_UNDEFINED) {
    IGL_LOG_ERROR("Render passes with subpasses cannot have resolve attachments\n");
    return VK_ERROR_INITIALIZATION_FAILED;
  }

  const bool hasDepthStencilAttachment = refDepth2_.layout != VK_IMAGE_LAYOUT_UNDEFINED;
  const uint32_t colorAttachmentsMask = lowBitsMask(numColorAttachments);

  const VkAttachmentReference2 unusedRef = {
      .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
      .attachment = VK_ATTACHMENT_UNUSED,
      .layout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  // color and input attachments have one entry per color attachment, so that the number of color
  // attachments is the same in every subpass and matches the blend states of the pipelines
  std::array<std::array<VkAttachmentReference2, IGL_COLOR_ATTACHMENTS_MAX>, IGL_SUBPASSES_MAX>
      colorRefs = {};
  std::array<std::array<VkAttachmentReference2, IGL_COLOR_ATTACHMENTS_MAX>, IGL_SUBPASSES_MAX>
      inputRefs = {};
  std::array<std::array<uint32_t, IGL_COLOR_ATTACHMENTS_MAX>, IGL_SUBPASSES_MAX> preserveRefs = {};
  std::array<VkSubpassDescription2, IGL_SUBPASSES_MAX> subpassDescs = {};

  for (uint32_t k = 0; k != numSubpasses_; ++k) {
    const uint32_t colorMask = subpasses_[k].colorAttachmentMask & colorAttachmentsMask;
    const uint32_t inputMask = subpasses_[k].inputAttachmentMask & colorAttachmentsMask;
    const uint32_t preserveMask = getPreserveMask(k);
    uint32_t numPreserveRefs = 0;

    for (uint32_t i = 0; i != numColorAttachments; ++i) {
      const VkAttachmentReference2 inputRef = {
          .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
          .attachment = refsColor2_[i].attachment,
          .layout = kInputAttachmentImageLayout,
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      };
      colorRefs[k][i] = hasBit(colorMask, i) ? refsColor2_[i] : unusedRef;
      inputRefs[k][i] = hasBit(inputMask, i) ? inputRef : unusedRef;
      if (hasBit(preserveMask, i)) {
        preserveRefs[k][numPreserveRefs++] = refsColor2_[i].attachment;
      }
    }

    subpassDescs[k] = VkSubpassDescription2{
        .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
        .pNext = nullptr,
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .viewMask = viewMask_,
        .inputAttachmentCount = inputMask != 0 ? numColorAttachments : 0u,
        .pInputAttachments = inputMask != 0 ? inputRefs[k].data() : nullptr,
        .colorAttachmentCount = numColorAttachments,
        .pColorAttachments = colorRefs[k].data(),
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = hasDepthStencilAttachment ? &refDepth2_ : nullptr,
        .preserveAttachmentCount = numPreserveRefs,
        .pPreserveAttachments = numPreserveRefs != 0 ? preserveRefs[k].data() : nullptr,
    };
  }

  const SubpassDependencies subpassDependencies = getSubpassDependencies();
  std::array<VkSubpassDependency2, kMaxSubpassDependencies> dependencies = {};
  for (uint32_t d = 0; d != subpassDependencies.count; ++d) {
    const SubpassDependencyPair& pair = subpassDependencies.pairs[d];
    if (pair.dstSubpass != VK_SUBPASS_EXTERNAL) {
      dependencies[d] = VkSubpassDependency2{
          .sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
          .srcSubpass = pair.srcSubpass,
          .dstSubpass = pair.dstSubpass,
          .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
          .srcAccessMask =
              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          .dstAccessMask =
              VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          .dependencyFlags = getDependencyFlags(pair),
          .viewOffset = 0,
      };
    } else {
      // The source stages are the ones that write the attachments. They are logically after the
      // fragment shader, so the transition to finalLayout also waits for its input attachment
      // reads. Not BY_REGION, because later passes can sample any texel of the attachments.
      dependencies[d] = VkSubpassDependency2{
          .sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
          .srcSubpass = pair.srcSubpass,
          .dstSubpass = VK_SUBPASS_EXTERNAL,
          .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
          .srcAccessMask =
              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
          .dependencyFlags = 0,
          .viewOffset = 0,
      };
    }
  }

  const VkRenderPassCreateInfo2 ci2 = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
      .flags = 0,
      .attachmentCount = static_cast<uint32_t>(attachments2_.size()),
      .pAttachments = attachments2_.data(),
      .subpassCount = numSubpasses_,
      .pSubpasses = subpassDescs.data(),
      .dependencyCount = subpassDependencies.count,
      .pDependencies = dependencies.data(),
      .correlatedViewMaskCount = viewMask_ ? 1u : 0u,
      .pCorrelatedViewMasks = viewMask_ ? &correlationMask_ : nullptr,
  };
  const VkResult result = vf.vkCreateRenderPass2(device, &ci2, nullptr, outRenderPass);
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
  IGL_PROFILER_FUNCTION();
  IGL_DEBUG_ASSERT(format != VK_FORMAT_UNDEFINED, "Invalid color attachment format");
  if (!refsColor2_.empty()) {
    IGL_DEBUG_ASSERT(attachments2_[refsColor2_.back().attachment].samples == samples,
                     "All non-resolve attachments should have the sample number of samples");
  }

#if IGL_VULKAN_HAS_LEGACY_RENDERPASS
  refsColor_.push_back(VkAttachmentReference{
      .attachment = static_cast<uint32_t>(attachments_.size()),
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
      .attachment = static_cast<uint32_t>(attachments2_.size()),
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
  IGL_PROFILER_FUNCTION();
  IGL_DEBUG_ASSERT(format != VK_FORMAT_UNDEFINED, "Invalid color resolve attachment format");

#if IGL_VULKAN_HAS_LEGACY_RENDERPASS
  refsColorResolve_.push_back(VkAttachmentReference{
      .attachment = static_cast<uint32_t>(attachments_.size()),
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
      .attachment = static_cast<uint32_t>(attachments2_.size()),
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
  IGL_PROFILER_FUNCTION();
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
      .attachment = static_cast<uint32_t>(attachments_.size()),
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
      .attachment = static_cast<uint32_t>(attachments2_.size()),
      .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .aspectMask =
          (hasDepth(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : static_cast<VkImageAspectFlags>(0)) |
          (hasStencil(format) ? VK_IMAGE_ASPECT_STENCIL_BIT : static_cast<VkImageAspectFlags>(0)),
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
  IGL_PROFILER_FUNCTION();
  IGL_DEBUG_ASSERT(refDepthResolve2_.layout == VK_IMAGE_LAYOUT_UNDEFINED,
                   "Can have only 1 depth resolve attachment");
  IGL_DEBUG_ASSERT(format != VK_FORMAT_UNDEFINED, "Invalid depth resolve attachment format");
#if IGL_VULKAN_HAS_LEGACY_RENDERPASS
  refDepthResolve_ = VkAttachmentReference{
      .attachment = static_cast<uint32_t>(attachments_.size()),
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
      .attachment = static_cast<uint32_t>(attachments2_.size()),
      .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .aspectMask =
          (hasDepth(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : static_cast<VkImageAspectFlags>(0)) |
          (hasStencil(format) ? VK_IMAGE_ASPECT_STENCIL_BIT : static_cast<VkImageAspectFlags>(0)),
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

VulkanRenderPassBuilder& VulkanRenderPassBuilder::setSubpasses(
    const RenderPassDesc::SubpassDesc* IGL_NULLABLE subpasses,
    uint32_t numSubpasses) {
  IGL_PROFILER_FUNCTION();
  IGL_DEBUG_ASSERT(numSubpasses == 0 || (numSubpasses >= 2 && numSubpasses <= IGL_SUBPASSES_MAX),
                   "A render pass has either no subpasses or between 2 and IGL_SUBPASSES_MAX");
  IGL_DEBUG_ASSERT(numSubpasses == 0 || subpasses != nullptr, "Subpasses are missing");

  subpasses_ = {};
  numSubpasses_ = 0;
  if (subpasses != nullptr && numSubpasses >= 2 && numSubpasses <= IGL_SUBPASSES_MAX) {
    std::copy_n(subpasses, numSubpasses, subpasses_.begin());
    numSubpasses_ = numSubpasses;
    IGL_DEBUG_ASSERT(hasValidSubpassMasks(subpasses_, numSubpasses_),
                     "A subpass cannot read an attachment that it writes, and can only read "
                     "attachments that an earlier subpass writes");
  }
  return *this;
}

VulkanRenderPassBuilder VulkanRenderPassBuilder::compatibilityKey() const {
  IGL_PROFILER_FUNCTION();
  VulkanRenderPassBuilder key = *this;
  // The preserve attachments are part of the subpass descriptions that compatible render passes
  // share. They depend on the store ops, so the key records them before it erases the store ops.
  for (uint32_t k = 0; k != numSubpasses_; ++k) {
    key.compatPreserveMasks_[k] = compatPreserveMasks_[k] | getPreserveMask(k);
  }
  for (VkAttachmentDescription2& attachment : key.attachments2_) {
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  }
  return key;
}

uint32_t VulkanRenderPassBuilder::getPreserveMask(uint32_t subpass) const {
  if (subpass >= numSubpasses_) {
    return 0;
  }

  const uint32_t numColorAttachments =
      std::min(static_cast<uint32_t>(refsColor2_.size()), IGL_COLOR_ATTACHMENTS_MAX);
  uint32_t storedMask = 0;
  for (uint32_t i = 0; i != numColorAttachments; ++i) {
    if (attachments2_[refsColor2_[i].attachment].storeOp == VK_ATTACHMENT_STORE_OP_STORE) {
      storedMask |= 1u << i;
    }
  }

  uint32_t usedBeforeMask = 0;
  for (uint32_t k = 0; k < subpass; ++k) {
    usedBeforeMask |= subpasses_[k].colorAttachmentMask | subpasses_[k].inputAttachmentMask;
  }
  uint32_t usedAfterMask = 0;
  for (uint32_t k = subpass + 1; k < numSubpasses_; ++k) {
    usedAfterMask |= subpasses_[k].colorAttachmentMask | subpasses_[k].inputAttachmentMask;
  }
  const uint32_t usedMask =
      subpasses_[subpass].colorAttachmentMask | subpasses_[subpass].inputAttachmentMask;

  return usedBeforeMask & ~usedMask & (usedAfterMask | storedMask) &
         lowBitsMask(numColorAttachments);
}

VulkanRenderPassBuilder::SubpassDependencies VulkanRenderPassBuilder::getSubpassDependencies()
    const {
  SubpassDependencies dependencies;
  if (numSubpasses_ < 2) {
    return dependencies;
  }

  for (uint32_t dst = 1; dst < numSubpasses_; ++dst) {
    for (uint32_t src = 0; src < dst; ++src) {
      const bool isConsecutive = src + 1 == dst;
      const bool readsFromSrc =
          (subpasses_[src].colorAttachmentMask & subpasses_[dst].inputAttachmentMask) != 0;
      if (isConsecutive || readsFromSrc) {
        dependencies.pairs[dependencies.count++] = {.srcSubpass = src, .dstSubpass = dst};
      }
    }
  }

  // The transition of an attachment to its finalLayout is ordered after the last subpass that
  // uses the attachment, and preserving an attachment is not using it. So every subpass that is
  // the last to use a color attachment gets a dependency on VK_SUBPASS_EXTERNAL, and so does the
  // last subpass if there is a depth attachment, which every subpass uses.
  const uint32_t numColorAttachments =
      std::min(static_cast<uint32_t>(refsColor2_.size()), IGL_COLOR_ATTACHMENTS_MAX);
  uint32_t lastUserMask = 0;
  for (uint32_t i = 0; i != numColorAttachments; ++i) {
    for (uint32_t k = numSubpasses_; k-- > 0;) {
      if (hasBit(subpasses_[k].colorAttachmentMask | subpasses_[k].inputAttachmentMask, i)) {
        lastUserMask |= 1u << k;
        break;
      }
    }
  }
  if (refDepth2_.layout != VK_IMAGE_LAYOUT_UNDEFINED) {
    lastUserMask |= 1u << (numSubpasses_ - 1);
  }
  for (uint32_t k = 0; k != numSubpasses_; ++k) {
    if (hasBit(lastUserMask, k)) {
      dependencies.pairs[dependencies.count++] = {.srcSubpass = k,
                                                  .dstSubpass = VK_SUBPASS_EXTERNAL};
    }
  }
  return dependencies;
}

uint32_t VulkanRenderPassBuilder::getNumDependencies() const {
  // a render pass without subpasses has a single dependency on VK_SUBPASS_EXTERNAL
  return numSubpasses_ > 1 ? getSubpassDependencies().count : 1u;
}

VkDependencyFlags VulkanRenderPassBuilder::getDependencyFlags(
    const SubpassDependencyPair& pair) const {
  // A dependency on VK_SUBPASS_EXTERNAL cannot be view-local, and later passes can sample any texel
  // of the attachments, so it is not by-region either
  if (pair.dstSubpass == VK_SUBPASS_EXTERNAL) {
    return 0;
  }
  // A view of a subpass only reads the same view of what an earlier subpass wrote
  const VkDependencyFlags byRegion = VK_DEPENDENCY_BY_REGION_BIT;
  return viewMask_ != 0 ? (byRegion | VK_DEPENDENCY_VIEW_LOCAL_BIT) : byRegion;
}

VkDependencyFlags VulkanRenderPassBuilder::getDependencyFlags(uint32_t index) const {
  const SubpassDependencies dependencies = getSubpassDependencies();
  return index < dependencies.count ? getDependencyFlags(dependencies.pairs[index]) : 0;
}

bool VulkanRenderPassBuilder::operator==(const VulkanRenderPassBuilder& other) const {
  return attachments2_ == other.attachments2_ && refsColor2_ == other.refsColor2_ &&
         refsColorResolve2_ == other.refsColorResolve2_ && refDepth2_ == other.refDepth2_ &&
         refDepthResolve2_ == other.refDepthResolve2_ && viewMask_ == other.viewMask_ &&
         correlationMask_ == other.correlationMask_ && numSubpasses_ == other.numSubpasses_ &&
         subpasses_ == other.subpasses_ && compatPreserveMasks_ == other.compatPreserveMasks_;
}

uint64_t VulkanRenderPassBuilder::HashFunction::operator()(
    const VulkanRenderPassBuilder& builder) const {
  IGL_PROFILER_FUNCTION();
  uint64_t hash = 0;
  // Combines the values like boost::hash_combine() does. A plain XOR would cancel out identical
  // attachments, and would not depend on their order.
  const auto combine = [&hash](uint32_t value) {
    hash ^= std::hash<uint32_t>()(value) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
  };
  combine(static_cast<uint32_t>(builder.attachments2_.size()));
  for (const auto& a : builder.attachments2_) {
    combine(a.flags);
    combine(a.format);
    combine(a.samples);
    combine(a.loadOp);
    combine(a.storeOp);
    combine(a.stencilLoadOp);
    combine(a.stencilStoreOp);
    combine(a.initialLayout);
    combine(a.finalLayout);
  }
  combine(static_cast<uint32_t>(builder.refsColor2_.size()));
  for (const auto& r : builder.refsColor2_) {
    combine(r.attachment);
    combine(r.layout);
    combine(r.aspectMask);
  }
  combine(static_cast<uint32_t>(builder.refsColorResolve2_.size()));
  for (const auto& r : builder.refsColorResolve2_) {
    combine(r.attachment);
    combine(r.layout);
    combine(r.aspectMask);
  }
  combine(builder.refDepth2_.attachment);
  combine(builder.refDepth2_.layout);
  combine(builder.refDepth2_.aspectMask);
  combine(builder.refDepthResolve2_.attachment);
  combine(builder.refDepthResolve2_.layout);
  combine(builder.refDepthResolve2_.aspectMask);
  combine(builder.viewMask_);
  combine(builder.correlationMask_);
  combine(builder.numSubpasses_);
  for (const RenderPassDesc::SubpassDesc& subpass : builder.subpasses_) {
    combine(subpass.colorAttachmentMask);
    combine(subpass.inputAttachmentMask);
  }
  for (const uint32_t preserveMask : builder.compatPreserveMasks_) {
    combine(preserveMask);
  }
  return hash;
}

} // namespace igl::vulkan
