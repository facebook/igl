/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <array>
#include <gtest/gtest.h>
#include <igl/vulkan/Common.h>
#include <string>
#include <vulkan/vulkan_core.h>

#ifdef __ANDROID__
#include <vulkan/vulkan_android.h>
#endif

namespace igl::tests {

//
// VulkanHelpersTest
//
// Unit tests for functions in VulkanHelpers.{h|c}.
//

// ivkGetVulkanResultString **********************************************************************
class GetVulkanResultString : public ::testing::Test {};
TEST_F(GetVulkanResultString, VulkanHelpersTest) {
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_SUCCESS), "VK_SUCCESS") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_NOT_READY), "VK_NOT_READY") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_TIMEOUT), "VK_TIMEOUT") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_EVENT_SET), "VK_EVENT_SET") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_EVENT_RESET), "VK_EVENT_RESET") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_INCOMPLETE), "VK_INCOMPLETE") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_OUT_OF_HOST_MEMORY),
                     "VK_ERROR_OUT_OF_HOST_MEMORY") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_OUT_OF_DEVICE_MEMORY),
                     "VK_ERROR_OUT_OF_DEVICE_MEMORY") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_INITIALIZATION_FAILED),
                     "VK_ERROR_INITIALIZATION_FAILED") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_DEVICE_LOST), "VK_ERROR_DEVICE_LOST") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_MEMORY_MAP_FAILED),
                     "VK_ERROR_MEMORY_MAP_FAILED") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_LAYER_NOT_PRESENT),
                     "VK_ERROR_LAYER_NOT_PRESENT") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_EXTENSION_NOT_PRESENT),
                     "VK_ERROR_EXTENSION_NOT_PRESENT") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_FEATURE_NOT_PRESENT),
                     "VK_ERROR_FEATURE_NOT_PRESENT") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_INCOMPATIBLE_DRIVER),
                     "VK_ERROR_INCOMPATIBLE_DRIVER") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_TOO_MANY_OBJECTS),
                     "VK_ERROR_TOO_MANY_OBJECTS") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_FORMAT_NOT_SUPPORTED),
                     "VK_ERROR_FORMAT_NOT_SUPPORTED") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_SURFACE_LOST_KHR),
                     "VK_ERROR_SURFACE_LOST_KHR") == 0);
  EXPECT_TRUE(
      strcmp(ivkGetVulkanResultString(VK_ERROR_OUT_OF_DATE_KHR), "VK_ERROR_OUT_OF_DATE_KHR") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR),
                     "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR),
                     "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_VALIDATION_FAILED_EXT),
                     "VK_ERROR_VALIDATION_FAILED_EXT") == 0);
  EXPECT_TRUE(
      strcmp(ivkGetVulkanResultString(VK_ERROR_FRAGMENTED_POOL), "VK_ERROR_FRAGMENTED_POOL") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_UNKNOWN), "VK_ERROR_UNKNOWN") == 0);
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_OUT_OF_POOL_MEMORY),
                     "VK_ERROR_OUT_OF_POOL_MEMORY") == 0); // 1.1
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_INVALID_EXTERNAL_HANDLE),
                     "VK_ERROR_INVALID_EXTERNAL_HANDLE") == 0); // 1.1
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_FRAGMENTATION), "VK_ERROR_FRAGMENTATION") ==
              0); // 1.2
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS),
                     "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS") == 0); // 1.2
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_SUBOPTIMAL_KHR), "VK_SUBOPTIMAL_KHR") ==
              0); // VK_KHR_swapchain
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_INVALID_SHADER_NV),
                     "VK_ERROR_INVALID_SHADER_NV") == 0); // VK_NV_glsl_shader
#ifdef VK_ENABLE_BETA_EXTENSIONS
                                                          // Provided by VK_KHR_video_queue
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR),
                     "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR") == 0);
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
  // Provided by VK_KHR_video_queue
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR),
                     "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR") == 0);
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
  // Provided by VK_KHR_video_queue
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR),
                     "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR") == 0);
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
  // Provided by VK_KHR_video_queue
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR),
                     "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR") == 0);
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
  // Provided by VK_KHR_video_queue
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR),
                     "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR") == 0);
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
  // Provided by VK_KHR_video_queue
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR),
                     "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR") == 0);
#endif
  EXPECT_TRUE(
      strcmp(ivkGetVulkanResultString(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT),
             "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT") ==
      0); // VK_EXT_image_drm_format_modifier
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_NOT_PERMITTED_KHR),
                     "VK_ERROR_NOT_PERMITTED_KHR") == 0); // VK_KHR_global_priority
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT),
                     "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT") ==
              0); // VK_EXT_full_screen_exclusive
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_THREAD_IDLE_KHR), "VK_THREAD_IDLE_KHR") ==
              0); // VK_KHR_deferred_host_operations
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_THREAD_DONE_KHR), "VK_THREAD_DONE_KHR") ==
              0); // VK_KHR_deferred_host_operations
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_OPERATION_DEFERRED_KHR),
                     "VK_OPERATION_DEFERRED_KHR") == 0); // VK_KHR_deferred_host_operations
  EXPECT_TRUE(strcmp(ivkGetVulkanResultString(VK_OPERATION_NOT_DEFERRED_KHR),
                     "VK_OPERATION_NOT_DEFERRED_KHR") == 0); // VK_KHR_deferred_host_operations
}

// ivkGetImageViewCreateInfo *********************************************************************
class GetImageViewCreateInfoTest : public ::testing::TestWithParam<std::tuple<VkImageViewType,
                                                                              VkFormat,
                                                                              VkImageAspectFlags,
                                                                              uint32_t,
                                                                              uint32_t,
                                                                              uint32_t,
                                                                              uint32_t>> {};

TEST_P(GetImageViewCreateInfoTest, GetImageViewCreateInfo) {
  const VkImage image = VK_NULL_HANDLE;
  const VkImageViewType imageViewType = std::get<0>(GetParam());
  const VkFormat format = std::get<1>(GetParam());
  const VkImageAspectFlags aspectMask = std::get<2>(GetParam());
  const uint32_t baseMipLevel = std::get<3>(GetParam());
  const uint32_t levelCount = std::get<4>(GetParam());
  const uint32_t baseArrayLayer = std::get<5>(GetParam());
  const uint32_t layerCount = std::get<6>(GetParam());

  const VkImageViewCreateInfo imageviewCreateInfo =
      ivkGetImageViewCreateInfo(image,
                                imageViewType,
                                format,
                                {aspectMask, baseMipLevel, levelCount, baseArrayLayer, layerCount});

  EXPECT_EQ(imageviewCreateInfo.sType, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
  EXPECT_EQ(imageviewCreateInfo.pNext, nullptr);
  EXPECT_EQ(imageviewCreateInfo.flags, 0);
  EXPECT_EQ(imageviewCreateInfo.image, image);
  EXPECT_EQ(imageviewCreateInfo.viewType, imageViewType);
  EXPECT_EQ(imageviewCreateInfo.format, format);
  EXPECT_EQ(imageviewCreateInfo.components.r, VK_COMPONENT_SWIZZLE_IDENTITY);
  EXPECT_EQ(imageviewCreateInfo.components.g, VK_COMPONENT_SWIZZLE_IDENTITY);
  EXPECT_EQ(imageviewCreateInfo.components.b, VK_COMPONENT_SWIZZLE_IDENTITY);
  EXPECT_EQ(imageviewCreateInfo.components.a, VK_COMPONENT_SWIZZLE_IDENTITY);
  EXPECT_EQ(imageviewCreateInfo.subresourceRange.aspectMask, aspectMask);
  EXPECT_EQ(imageviewCreateInfo.subresourceRange.baseMipLevel, baseMipLevel);
  EXPECT_EQ(imageviewCreateInfo.subresourceRange.levelCount, levelCount);
  EXPECT_EQ(imageviewCreateInfo.subresourceRange.baseArrayLayer, baseArrayLayer);
  EXPECT_EQ(imageviewCreateInfo.subresourceRange.layerCount, layerCount);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    GetImageViewCreateInfoTest,
    ::testing::Combine(::testing::Values(VK_IMAGE_VIEW_TYPE_1D, VK_IMAGE_VIEW_TYPE_2D),
                       ::testing::Values(VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB),
                       ::testing::Values(VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_DEPTH_BIT),
                       ::testing::Values(0, 1),
                       ::testing::Values(1, 2),
                       ::testing::Values(0, 1),
                       ::testing::Values(1, 2)),
    [](const testing::TestParamInfo<GetImageViewCreateInfoTest::ParamType>& info) {
      const std::string name =
          std::to_string(std::get<0>(info.param)) + "_" + std::to_string(std::get<1>(info.param)) +
          "_" + std::to_string(std::get<2>(info.param)) + "_" +
          std::to_string(std::get<3>(info.param)) + "_" + std::to_string(std::get<4>(info.param)) +
          "_" + std::to_string(std::get<5>(info.param)) + "_" +
          std::to_string(std::get<6>(info.param));
      return name;
    });

// ivkGetAttachmentDescriptionColor **************************************************************
class AttachmentDescriptionColorTest
  : public ::testing::TestWithParam<
        std::tuple<VkFormat, VkAttachmentLoadOp, VkAttachmentStoreOp, VkImageLayout>> {};

TEST_P(AttachmentDescriptionColorTest, GetAttachmentDescriptionColor) {
  const VkFormat format = std::get<0>(GetParam());
  const VkAttachmentLoadOp loadOp = std::get<1>(GetParam());
  const VkAttachmentStoreOp storeOp = std::get<2>(GetParam());
  const VkImageLayout initialLayout = std::get<3>(GetParam());
  const auto finalLayout = static_cast<VkImageLayout>(initialLayout + 1);

  const auto attachmentDescriptionColor =
      ivkGetAttachmentDescriptionColor(format, loadOp, storeOp, initialLayout, finalLayout);

  EXPECT_EQ(attachmentDescriptionColor.sType, VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2);
  EXPECT_EQ(attachmentDescriptionColor.format, format);
  EXPECT_EQ(attachmentDescriptionColor.samples, VK_SAMPLE_COUNT_1_BIT);
  EXPECT_EQ(attachmentDescriptionColor.loadOp, loadOp);
  EXPECT_EQ(attachmentDescriptionColor.storeOp, storeOp);
  EXPECT_EQ(attachmentDescriptionColor.stencilLoadOp, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
  EXPECT_EQ(attachmentDescriptionColor.stencilStoreOp, VK_ATTACHMENT_STORE_OP_DONT_CARE);
  EXPECT_EQ(attachmentDescriptionColor.initialLayout, initialLayout);
  EXPECT_EQ(attachmentDescriptionColor.finalLayout, finalLayout);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    AttachmentDescriptionColorTest,
    ::testing::Combine(
        ::testing::Values(VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB),
        ::testing::Values(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_LOAD),
        ::testing::Values(VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE),
        ::testing::Values(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)),
    [](const testing::TestParamInfo<AttachmentDescriptionColorTest::ParamType>& info) {
      const std::string name = std::to_string(std::get<0>(info.param)) + "_" +
                               std::to_string(std::get<1>(info.param)) + "_" +
                               std::to_string(std::get<2>(info.param)) + "_" +
                               std::to_string(std::get<3>(info.param));
      return name;
    });

// ivkGetAttachmentReferenceColor **************************************************************
class AttachmentReferenceColorTest : public ::testing::Test {};

TEST_F(AttachmentReferenceColorTest, GetAttachmentReferenceColor) {
  for (uint32_t i = 0; i < 2; ++i) {
    const auto attachmentRef = ivkGetAttachmentReferenceColor(i);
    EXPECT_EQ(attachmentRef.sType, VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2);
    EXPECT_EQ(attachmentRef.pNext, nullptr);
    EXPECT_EQ(attachmentRef.attachment, i);
    EXPECT_EQ(attachmentRef.layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(attachmentRef.aspectMask, VK_IMAGE_ASPECT_COLOR_BIT);
  }
}

// ivkGetDescriptorSetLayoutBinding **************************************************************
class DescriptorSetLayoutTest
  : public ::testing::TestWithParam<std::tuple<uint32_t, VkDescriptorType, uint32_t>> {};

TEST_P(DescriptorSetLayoutTest, GetDescriptorSetLayoutBinding) {
  const uint32_t binding = std::get<0>(GetParam());
  const VkDescriptorType descriptorType = std::get<1>(GetParam());
  const uint32_t count = std::get<2>(GetParam());

  const VkShaderStageFlags flags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

  const VkDescriptorSetLayoutBinding descSetLayoutBinding =
      ivkGetDescriptorSetLayoutBinding(binding, descriptorType, count, flags);
  EXPECT_EQ(descSetLayoutBinding.binding, binding);
  EXPECT_EQ(descSetLayoutBinding.descriptorType, descriptorType);
  EXPECT_EQ(descSetLayoutBinding.descriptorCount, count);
  EXPECT_EQ(descSetLayoutBinding.stageFlags, flags);
  EXPECT_EQ(descSetLayoutBinding.pImmutableSamplers, nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    DescriptorSetLayoutTest,
    ::testing::Combine(::testing::Values(0, 1, 2),
                       ::testing::Values(VK_DESCRIPTOR_TYPE_SAMPLER,
                                         VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                       ::testing::Values(0, 1, 2)),
    [](const testing::TestParamInfo<DescriptorSetLayoutTest::ParamType>& info) {
      const std::string name = std::to_string(std::get<0>(info.param)) + "_" +
                               std::to_string(std::get<1>(info.param)) + "_" +
                               std::to_string(std::get<2>(info.param));
      return name;
    });

// ivkGetAttachmentDescription **************************************************************

class AttachmentDescriptionTest
  : public ::testing::TestWithParam<std::tuple<VkFormat,
                                               VkAttachmentLoadOp,
                                               VkAttachmentStoreOp,
                                               VkImageLayout,
                                               VkImageLayout,
                                               VkSampleCountFlagBits>> {};

TEST_P(AttachmentDescriptionTest, GetAttachmentDescription) {
  const VkFormat format = std::get<0>(GetParam());
  const VkAttachmentLoadOp loadOp = std::get<1>(GetParam());
  const VkAttachmentStoreOp storeOp = std::get<2>(GetParam());
  const VkImageLayout initialLayout = std::get<3>(GetParam());
  const VkImageLayout finalLayout = std::get<4>(GetParam());
  const VkSampleCountFlagBits samples = std::get<5>(GetParam());

  const auto attachmentDescription =
      ivkGetAttachmentDescription(format, loadOp, storeOp, initialLayout, finalLayout, samples);
  EXPECT_EQ(attachmentDescription.flags, 0);
  EXPECT_EQ(attachmentDescription.format, format);
  EXPECT_EQ(attachmentDescription.samples, samples);
  EXPECT_EQ(attachmentDescription.loadOp, loadOp);
  EXPECT_EQ(attachmentDescription.storeOp, storeOp);
  EXPECT_EQ(attachmentDescription.stencilLoadOp, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
  EXPECT_EQ(attachmentDescription.stencilStoreOp, VK_ATTACHMENT_STORE_OP_DONT_CARE);
  EXPECT_EQ(attachmentDescription.initialLayout, initialLayout);
  EXPECT_EQ(attachmentDescription.finalLayout, finalLayout);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    AttachmentDescriptionTest,
    ::testing::Combine(
        ::testing::Values(VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8_SRGB),
        ::testing::Values(VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_LOAD),
        ::testing::Values(VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE),
        ::testing::Values(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
        ::testing::Values(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
        ::testing::Values(VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_4_BIT)),
    [](const testing::TestParamInfo<AttachmentDescriptionTest::ParamType>& info) {
      const std::string name =
          std::to_string(std::get<0>(info.param)) + "_" + std::to_string(std::get<1>(info.param)) +
          "_" + std::to_string(std::get<2>(info.param)) + "_" +
          std::to_string(std::get<3>(info.param)) + "_" + std::to_string(std::get<4>(info.param)) +
          "_" + std::to_string(std::get<5>(info.param));
      return name;
    });

// ivkGetAttachmentReference **************************************************************

class AttachmentReferenceTest
  : public ::testing::TestWithParam<std::tuple<uint32_t, VkImageLayout>> {};

TEST_P(AttachmentReferenceTest, GetAttachmentDescription) {
  const uint32_t attachmentId = std::get<0>(GetParam());
  const VkImageLayout layout = std::get<1>(GetParam());

  const auto attachmentReference = ivkGetAttachmentReference(attachmentId, layout);
  EXPECT_EQ(attachmentReference.attachment, attachmentId);
  EXPECT_EQ(attachmentReference.layout, layout);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    AttachmentReferenceTest,
    ::testing::Combine(::testing::Values(0, 1),
                       ::testing::Values(VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)),
    [](const testing::TestParamInfo<AttachmentReferenceTest::ParamType>& info) {
      const std::string name =
          std::to_string(std::get<0>(info.param)) + "_" + std::to_string(std::get<1>(info.param));
      return name;
    });

// ivkGetSubpassDescription **************************************************************

// Parameter list:
//   1. Number of attachments (total, including color, resolve, and depth)
//   2. MSAA enabled. Resolve attachments are created if true
//   3. Depth attachment present?
class SubpassDescriptionTest : public ::testing::TestWithParam<std::tuple<uint32_t, bool, bool>> {};

TEST_P(SubpassDescriptionTest, GetSubpassDescription) {
  const uint32_t numColorAttachments = std::get<0>(GetParam());
  const bool withResolveAttachments = std::get<1>(GetParam());
  const bool withDepthAttachment = std::get<2>(GetParam());

  std::vector<VkAttachmentReference> colorAttachmentReferences(numColorAttachments);
  std::vector<VkAttachmentReference> resolveAttachmentReferences(numColorAttachments);
  VkAttachmentReference depthAttachment =
      ivkGetAttachmentReference(numColorAttachments, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  for (uint32_t i = 0; i < numColorAttachments; ++i) {
    colorAttachmentReferences.emplace_back(
        ivkGetAttachmentReference(i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    if (withResolveAttachments) {
      resolveAttachmentReferences.emplace_back(
          ivkGetAttachmentReference(i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    }
  }

  const auto subpassDescription = ivkGetSubpassDescription(
      numColorAttachments,
      colorAttachmentReferences.data(),
      withResolveAttachments ? resolveAttachmentReferences.data() : nullptr,
      withDepthAttachment ? &depthAttachment : nullptr);

  EXPECT_EQ(subpassDescription.flags, 0);
  EXPECT_EQ(subpassDescription.pipelineBindPoint, VK_PIPELINE_BIND_POINT_GRAPHICS);
  EXPECT_EQ(subpassDescription.inputAttachmentCount, 0);
  EXPECT_EQ(subpassDescription.pInputAttachments, nullptr);
  EXPECT_EQ(subpassDescription.colorAttachmentCount, numColorAttachments);
  EXPECT_EQ(subpassDescription.pColorAttachments, colorAttachmentReferences.data());
  EXPECT_EQ(subpassDescription.pResolveAttachments,
            withResolveAttachments ? resolveAttachmentReferences.data() : nullptr);
  EXPECT_EQ(subpassDescription.pDepthStencilAttachment,
            withDepthAttachment ? &depthAttachment : nullptr);
  EXPECT_EQ(subpassDescription.preserveAttachmentCount, 0);
  EXPECT_EQ(subpassDescription.pPreserveAttachments, nullptr);
}

INSTANTIATE_TEST_SUITE_P(AllCombinations,
                         SubpassDescriptionTest,
                         ::testing::Combine(::testing::Values(1, 2),
                                            ::testing::Bool(),
                                            ::testing::Bool()),
                         [](const testing::TestParamInfo<SubpassDescriptionTest::ParamType>& info) {
                           const std::string name = std::to_string(std::get<0>(info.param)) + "_" +
                                                    std::to_string(std::get<1>(info.param)) + "_" +
                                                    std::to_string(std::get<2>(info.param));
                           return name;
                         });

// ivkGetSubpassDependency **************************************************************

class SubpassDependencyTest : public ::testing::Test {};

TEST_F(SubpassDependencyTest, GetAttachmentDescription) {
  const auto subpassDependency = ivkGetSubpassDependency();
  EXPECT_EQ(subpassDependency.srcSubpass, 0);
  EXPECT_EQ(subpassDependency.dstSubpass, VK_SUBPASS_EXTERNAL);
  EXPECT_EQ(subpassDependency.srcStageMask, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
  EXPECT_EQ(subpassDependency.dstStageMask, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  EXPECT_EQ(subpassDependency.srcAccessMask, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
  EXPECT_EQ(subpassDependency.dstAccessMask, VK_ACCESS_SHADER_READ_BIT);
}

// ivkGetRenderPassMultiviewCreateInfo ***************************************************

class RenderPassMultiviewCreateInfoTest : public ::testing::Test {};

TEST_F(RenderPassMultiviewCreateInfoTest, GetRenderPassMultiviewCreateInfo) {
  constexpr uint32_t viewMask = 0;
  constexpr uint32_t correlationMask = 0;

  const auto renderPassMultiviewCreateInfo =
      ivkGetRenderPassMultiviewCreateInfo(&viewMask, &correlationMask);
  EXPECT_EQ(renderPassMultiviewCreateInfo.sType,
            VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO);
  EXPECT_EQ(renderPassMultiviewCreateInfo.pNext, nullptr);
  EXPECT_EQ(renderPassMultiviewCreateInfo.subpassCount, 1);
  EXPECT_EQ(renderPassMultiviewCreateInfo.pViewMasks, &viewMask);
  EXPECT_EQ(renderPassMultiviewCreateInfo.dependencyCount, 0);
  EXPECT_EQ(renderPassMultiviewCreateInfo.pViewOffsets, nullptr);
  EXPECT_EQ(renderPassMultiviewCreateInfo.correlationMaskCount, 1);
  EXPECT_EQ(renderPassMultiviewCreateInfo.pCorrelationMasks, &correlationMask);
}

// ivkGetClearColorValue ***************************************************

class ClearColorValueTest
  : public ::testing::TestWithParam<std::tuple<float, float, float, float>> {};

TEST_P(ClearColorValueTest, GetClearColorValue) {
  const float r = std::get<0>(GetParam());
  const float g = std::get<1>(GetParam());
  const float b = std::get<2>(GetParam());
  const float a = std::get<3>(GetParam());

  const auto clearValue = ivkGetClearColorValue(r, g, b, a);
  EXPECT_EQ(clearValue.color.float32[0], r);
  EXPECT_EQ(clearValue.color.float32[1], g);
  EXPECT_EQ(clearValue.color.float32[2], b);
  EXPECT_EQ(clearValue.color.float32[3], a);
}

INSTANTIATE_TEST_SUITE_P(AllCombinations,
                         ClearColorValueTest,
                         ::testing::Combine(::testing::Values(0.f, 1.0f),
                                            ::testing::Values(0.f, 1.0f),
                                            ::testing::Values(0.f, 1.0f),
                                            ::testing::Values(0.f, 1.0f)));

// ivkGetClearDepthStencilValue ***************************************************

class ClearDepthStencilValueTest : public ::testing::TestWithParam<std::tuple<float, uint32_t>> {};

TEST_P(ClearDepthStencilValueTest, GetClearDepthStencilValue) {
  const float depth = std::get<0>(GetParam());
  const uint32_t stencil = std::get<1>(GetParam());

  const auto clearValue = ivkGetClearDepthStencilValue(depth, stencil);
  EXPECT_EQ(clearValue.depthStencil.depth, depth);
  EXPECT_EQ(clearValue.depthStencil.stencil, stencil);
}

INSTANTIATE_TEST_SUITE_P(AllCombinations,
                         ClearDepthStencilValueTest,
                         ::testing::Combine(::testing::Values(0.f, 1.0f), ::testing::Values(0, 1)));

// ivkGetBufferCreateInfo ***************************************************

class BufferCreateInfoTest
  : public ::testing::TestWithParam<std::tuple<uint64_t, VkBufferUsageFlags>> {};

TEST_P(BufferCreateInfoTest, GetBufferCreateInfo) {
  const uint64_t size = std::get<0>(GetParam());
  const VkBufferUsageFlags usage = std::get<1>(GetParam());

  const auto bufferCreateInfo = ivkGetBufferCreateInfo(size, usage);
  EXPECT_EQ(bufferCreateInfo.sType, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
  EXPECT_EQ(bufferCreateInfo.pNext, nullptr);
  EXPECT_EQ(bufferCreateInfo.flags, 0);
  EXPECT_EQ(bufferCreateInfo.size, size);
  EXPECT_EQ(bufferCreateInfo.usage, usage);
  EXPECT_EQ(bufferCreateInfo.sharingMode, VK_SHARING_MODE_EXCLUSIVE);
  EXPECT_EQ(bufferCreateInfo.queueFamilyIndexCount, 0);
  EXPECT_EQ(bufferCreateInfo.pQueueFamilyIndices, nullptr);
}

INSTANTIATE_TEST_SUITE_P(AllCombinations,
                         BufferCreateInfoTest,
                         ::testing::Combine(::testing::Values(100, 1'000),
                                            ::testing::Values(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT)),
                         [](const testing::TestParamInfo<BufferCreateInfoTest::ParamType>& info) {
                           const std::string name = std::to_string(std::get<0>(info.param)) + "_" +
                                                    std::to_string(std::get<1>(info.param));
                           return name;
                         });

// ivkGetImageCreateInfo ***************************************************

class ImageCreateInfoTest : public ::testing::TestWithParam<std::tuple<VkImageType,
                                                                       VkFormat,
                                                                       VkImageTiling,
                                                                       VkImageUsageFlags,
                                                                       VkExtent3D,
                                                                       uint32_t,
                                                                       uint32_t,
                                                                       VkImageCreateFlags,
                                                                       VkSampleCountFlags>> {};

TEST_P(ImageCreateInfoTest, GetImageCreateInfo) {
  const VkImageType imageType = std::get<0>(GetParam());
  const VkFormat format = std::get<1>(GetParam());
  const VkImageTiling tiling = std::get<2>(GetParam());
  const VkImageUsageFlags usage = std::get<3>(GetParam());
  const VkExtent3D extent = std::get<4>(GetParam());
  const uint32_t mipLevels = std::get<5>(GetParam());
  const uint32_t arrayLayers = std::get<6>(GetParam());
  const VkImageCreateFlags flags = std::get<7>(GetParam());
  const VkSampleCountFlags samples = std::get<8>(GetParam());

  const auto imageCreateInfo = ivkGetImageCreateInfo(
      imageType, format, tiling, usage, extent, mipLevels, arrayLayers, flags, samples);
  EXPECT_EQ(imageCreateInfo.sType, VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
  EXPECT_EQ(imageCreateInfo.pNext, nullptr);
  EXPECT_EQ(imageCreateInfo.flags, flags);
  EXPECT_EQ(imageCreateInfo.imageType, imageType);
  EXPECT_EQ(imageCreateInfo.format, format);
  EXPECT_EQ(imageCreateInfo.extent.width, extent.width);
  EXPECT_EQ(imageCreateInfo.extent.height, extent.height);
  EXPECT_EQ(imageCreateInfo.extent.depth, extent.depth);
  EXPECT_EQ(imageCreateInfo.mipLevels, mipLevels);
  EXPECT_EQ(imageCreateInfo.arrayLayers, arrayLayers);
  EXPECT_EQ(imageCreateInfo.samples, samples);
  EXPECT_EQ(imageCreateInfo.tiling, tiling);
  EXPECT_EQ(imageCreateInfo.sharingMode, VK_SHARING_MODE_EXCLUSIVE);
  EXPECT_EQ(imageCreateInfo.queueFamilyIndexCount, 0);
  EXPECT_EQ(imageCreateInfo.pQueueFamilyIndices, nullptr);
  EXPECT_EQ(imageCreateInfo.initialLayout, VK_IMAGE_LAYOUT_UNDEFINED);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    ImageCreateInfoTest,
    ::testing::Combine(::testing::Values(VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D),
                       ::testing::Values(VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_SRGB),
                       ::testing::Values(VK_IMAGE_TILING_LINEAR, VK_IMAGE_TILING_OPTIMAL),
                       ::testing::Values(VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_USAGE_STORAGE_BIT),
                       ::testing::Values(VkExtent3D{50, 50, 1}, VkExtent3D{100, 100, 1}),
                       ::testing::Values(1, 2),
                       ::testing::Values(1, 2),
                       ::testing::Values(0, VK_IMAGE_CREATE_SPARSE_BINDING_BIT),
                       ::testing::Values(VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_4_BIT)),
    [](const testing::TestParamInfo<ImageCreateInfoTest::ParamType>& info) {
      const std::string name =
          std::to_string(std::get<0>(info.param)) + "_" + std::to_string(std::get<1>(info.param)) +
          "_" + std::to_string(std::get<2>(info.param)) + "_" +
          std::to_string(std::get<3>(info.param)) + "_" +
          std::to_string(std::get<4>(info.param).width) + "_" +
          std::to_string(std::get<4>(info.param).height) + "_" +
          std::to_string(std::get<4>(info.param).depth) + "_" +
          std::to_string(std::get<5>(info.param)) + "_" + std::to_string(std::get<6>(info.param)) +
          "_" + std::to_string(std::get<7>(info.param)) + "_" +
          std::to_string(std::get<8>(info.param));
      return name;
    });

// ivkGetPipelineVertexInputStateCreateInfo_Empty ***********************************************

class PipelineVertexInpusStateCreateInfoTest_Empty : public ::testing::Test {};

TEST_F(PipelineVertexInpusStateCreateInfoTest_Empty, GetPipelineVertexInputStateCreateInfo_Empty) {
  const VkPipelineVertexInputStateCreateInfo pipelineVertexInputCreateInfo =
      ivkGetPipelineVertexInputStateCreateInfo_Empty();

  EXPECT_EQ(pipelineVertexInputCreateInfo.sType,
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
  EXPECT_EQ(pipelineVertexInputCreateInfo.pNext, nullptr);
  EXPECT_EQ(pipelineVertexInputCreateInfo.flags, 0);
  EXPECT_EQ(pipelineVertexInputCreateInfo.vertexBindingDescriptionCount, 0);
  EXPECT_EQ(pipelineVertexInputCreateInfo.pVertexBindingDescriptions, nullptr);
  EXPECT_EQ(pipelineVertexInputCreateInfo.vertexAttributeDescriptionCount, 0);
  EXPECT_EQ(pipelineVertexInputCreateInfo.pVertexAttributeDescriptions, nullptr);
}

// ivkGetPipelineInputAssemblyStateCreateInfo ***************************************************

class PipelineInputAssemblyStateCreateInfoTest
  : public ::testing::TestWithParam<std::tuple<VkPrimitiveTopology, VkBool32>> {};

TEST_P(PipelineInputAssemblyStateCreateInfoTest, GetPipelineInputAssemblyStateCreateInfo) {
  const VkPrimitiveTopology topology = std::get<0>(GetParam());
  const VkBool32 primitiveRestart = std::get<1>(GetParam());

  const auto pipelineInputAssemblyStateCreateInfo =
      ivkGetPipelineInputAssemblyStateCreateInfo(topology, primitiveRestart);
  EXPECT_EQ(pipelineInputAssemblyStateCreateInfo.sType,
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
  EXPECT_EQ(pipelineInputAssemblyStateCreateInfo.pNext, nullptr);
  EXPECT_EQ(pipelineInputAssemblyStateCreateInfo.flags, 0);
  EXPECT_EQ(pipelineInputAssemblyStateCreateInfo.topology, topology);
  EXPECT_EQ(pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable, primitiveRestart);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    PipelineInputAssemblyStateCreateInfoTest,
    ::testing::Combine(::testing::Values(VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
                                         VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
                       ::testing::Values(VK_TRUE, VK_FALSE)),
    [](const testing::TestParamInfo<PipelineInputAssemblyStateCreateInfoTest::ParamType>& info) {
      const std::string name =
          std::to_string(std::get<0>(info.param)) + "_" + std::to_string(std::get<1>(info.param));
      return name;
    });

// ivkGetPipelineDynamicStateCreateInfo ***************************************************

class PipelineDynamicStateCreateInfoTest : public ::testing::TestWithParam<std::tuple<uint32_t>> {};

TEST_P(PipelineDynamicStateCreateInfoTest, GetPipelineDyncStateCreateInfo) {
  const uint32_t dynamicStateCount = std::get<0>(GetParam());
  EXPECT_LE(dynamicStateCount, 2);
  const std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                       VK_DYNAMIC_STATE_SCISSOR};

  const auto pipelineDynamicStateCreateInfo =
      ivkGetPipelineDynamicStateCreateInfo(dynamicStateCount, dynamicStates.data());
  EXPECT_EQ(pipelineDynamicStateCreateInfo.sType,
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
  EXPECT_EQ(pipelineDynamicStateCreateInfo.pNext, nullptr);
  EXPECT_EQ(pipelineDynamicStateCreateInfo.dynamicStateCount, dynamicStateCount);
  EXPECT_EQ(pipelineDynamicStateCreateInfo.pDynamicStates, dynamicStates.data());
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    PipelineDynamicStateCreateInfoTest,
    ::testing::Combine(::testing::Values(1, 2)),
    [](const testing::TestParamInfo<PipelineDynamicStateCreateInfoTest::ParamType>& info) {
      const std::string name = "dynamicStateCount_" + std::to_string(std::get<0>(info.param));
      return name;
    });

// ivkGetPipelineRasterizationStateCreateInfo ***************************************************

class PipelineRasterizationStateCreateInfoTest
  : public ::testing::TestWithParam<std::tuple<VkPolygonMode, VkCullModeFlags>> {};

TEST_P(PipelineRasterizationStateCreateInfoTest, GetPipelineRasterizationStateCreateInfo) {
  const VkPolygonMode polygonMode = std::get<0>(GetParam());
  const VkCullModeFlags cullMode = std::get<1>(GetParam());

  const auto pipelineRasterizationStateCreateInfo =
      ivkGetPipelineRasterizationStateCreateInfo(polygonMode, cullMode);
  EXPECT_EQ(pipelineRasterizationStateCreateInfo.sType,
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
  EXPECT_EQ(pipelineRasterizationStateCreateInfo.pNext, nullptr);
  EXPECT_EQ(pipelineRasterizationStateCreateInfo.flags, 0);
  EXPECT_EQ(pipelineRasterizationStateCreateInfo.depthClampEnable, VK_FALSE);
  EXPECT_EQ(pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable, VK_FALSE);
  EXPECT_EQ(pipelineRasterizationStateCreateInfo.polygonMode, polygonMode);
  EXPECT_EQ(pipelineRasterizationStateCreateInfo.cullMode, cullMode);
  EXPECT_EQ(pipelineRasterizationStateCreateInfo.frontFace, VK_FRONT_FACE_COUNTER_CLOCKWISE);
  EXPECT_EQ(pipelineRasterizationStateCreateInfo.depthBiasEnable, VK_FALSE);
  EXPECT_EQ(pipelineRasterizationStateCreateInfo.depthBiasConstantFactor, 0.0f);
  EXPECT_EQ(pipelineRasterizationStateCreateInfo.depthBiasClamp, 0.0f);
  EXPECT_EQ(pipelineRasterizationStateCreateInfo.depthBiasSlopeFactor, 0.0f);
  EXPECT_EQ(pipelineRasterizationStateCreateInfo.lineWidth, 1.0f);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    PipelineRasterizationStateCreateInfoTest,
    ::testing::Combine(::testing::Values(VK_POLYGON_MODE_FILL, VK_POLYGON_MODE_LINE),
                       ::testing::Values(VK_CULL_MODE_FRONT_BIT, VK_CULL_MODE_BACK_BIT)),
    [](const testing::TestParamInfo<PipelineRasterizationStateCreateInfoTest::ParamType>& info) {
      const std::string name =
          std::to_string(std::get<0>(info.param)) + "_" + std::to_string(std::get<1>(info.param));
      return name;
    });

// ivkGetPipelineMultisampleStateCreateInfo_Empty ***********************************************

class GetPipelineMultisampleStateCreateInfo_Empty : public ::testing::Test {};

TEST_F(GetPipelineMultisampleStateCreateInfo_Empty, GetPipelineMultisampleStateCreateInfo_Empty) {
  const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo =
      ivkGetPipelineMultisampleStateCreateInfo_Empty();

  EXPECT_EQ(pipelineMultisampleStateCreateInfo.sType,
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
  EXPECT_EQ(pipelineMultisampleStateCreateInfo.pNext, nullptr);
  EXPECT_EQ(pipelineMultisampleStateCreateInfo.rasterizationSamples, VK_SAMPLE_COUNT_1_BIT);
  EXPECT_EQ(pipelineMultisampleStateCreateInfo.sampleShadingEnable, VK_FALSE);
  EXPECT_EQ(pipelineMultisampleStateCreateInfo.minSampleShading, 1.0f);
  EXPECT_EQ(pipelineMultisampleStateCreateInfo.pSampleMask, nullptr);
  EXPECT_EQ(pipelineMultisampleStateCreateInfo.alphaToCoverageEnable, VK_FALSE);
  EXPECT_EQ(pipelineMultisampleStateCreateInfo.alphaToOneEnable, VK_FALSE);
}

// ivkGetPipelineDepthStencilStateCreateInfo_NoDepthStencilTests *******************************

class GetPipelineDepthStencilStateCreateInfo_NoDepthStencilTestsTest : public ::testing::Test {};

TEST_F(GetPipelineDepthStencilStateCreateInfo_NoDepthStencilTestsTest,
       GetPipelineDepthStencilStateCreateInfo_NoDepthStencilTests) {
  const VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo =
      ivkGetPipelineDepthStencilStateCreateInfo_NoDepthStencilTests();

  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.sType,
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.pNext, nullptr);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.flags, 0);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.depthTestEnable, VK_FALSE);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.depthWriteEnable, VK_FALSE);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.depthCompareOp, VK_COMPARE_OP_LESS);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable, VK_FALSE);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.stencilTestEnable, VK_FALSE);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.minDepthBounds, 0.0f);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.maxDepthBounds, 1.0f);

  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.front.failOp, VK_STENCIL_OP_KEEP);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.front.passOp, VK_STENCIL_OP_KEEP);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.front.depthFailOp, VK_STENCIL_OP_KEEP);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.front.compareOp, VK_COMPARE_OP_NEVER);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.front.compareMask, 0);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.front.writeMask, 0);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.front.reference, 0);

  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.back.failOp, VK_STENCIL_OP_KEEP);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.back.passOp, VK_STENCIL_OP_KEEP);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.back.depthFailOp, VK_STENCIL_OP_KEEP);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.back.compareOp, VK_COMPARE_OP_NEVER);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.back.compareMask, 0);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.back.writeMask, 0);
  EXPECT_EQ(pipelineDepthStencilStateCreateInfo.back.reference, 0);
}

// ivkGetPipelineColorBlendAttachmentState_NoBlending *******************************************

class GetPipelineColorBlendAttachmentState_NoBlendingTest : public ::testing::Test {};

TEST_F(GetPipelineColorBlendAttachmentState_NoBlendingTest,
       GetPipelineColorBlendAttachmentState_NoBlending) {
  const VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState =
      ivkGetPipelineColorBlendAttachmentState_NoBlending();

  EXPECT_EQ(pipelineColorBlendAttachmentState.blendEnable, VK_FALSE);
  EXPECT_EQ(pipelineColorBlendAttachmentState.srcColorBlendFactor, VK_BLEND_FACTOR_ONE);
  EXPECT_EQ(pipelineColorBlendAttachmentState.dstColorBlendFactor, VK_BLEND_FACTOR_ZERO);
  EXPECT_EQ(pipelineColorBlendAttachmentState.colorBlendOp, VK_BLEND_OP_ADD);
  EXPECT_EQ(pipelineColorBlendAttachmentState.srcAlphaBlendFactor, VK_BLEND_FACTOR_ONE);
  EXPECT_EQ(pipelineColorBlendAttachmentState.dstAlphaBlendFactor, VK_BLEND_FACTOR_ZERO);
  EXPECT_EQ(pipelineColorBlendAttachmentState.alphaBlendOp, VK_BLEND_OP_ADD);
  EXPECT_EQ(pipelineColorBlendAttachmentState.colorWriteMask,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT);
}

// ivkGetPipelineColorBlendAttachmentState ***************************************************

class PipelineColorBlendAttachmentStateTest
  : public ::testing::TestWithParam<std::tuple<bool,
                                               VkBlendFactor,
                                               VkBlendFactor,
                                               VkBlendOp,
                                               VkBlendFactor,
                                               VkBlendFactor,
                                               VkBlendOp,
                                               VkColorComponentFlags>> {};

TEST_P(PipelineColorBlendAttachmentStateTest, GetPipelineColorBlendAttachmentState) {
  const bool blendEnabled = std::get<0>(GetParam());
  const VkBlendFactor srcColorBlendFactor = std::get<1>(GetParam());
  const VkBlendFactor dstColorBlendFactor = std::get<2>(GetParam());
  const VkBlendOp colorBlendOp = std::get<3>(GetParam());
  const VkBlendFactor srcAlphaBlendFactor = std::get<4>(GetParam());
  const VkBlendFactor dstAlphaBlendFactor = std::get<5>(GetParam());
  const VkBlendOp alphaBlendOp = std::get<6>(GetParam());
  const VkColorComponentFlags colorWriteMask = std::get<7>(GetParam());

  const auto pipelineColorBlendAttachmentState =
      ivkGetPipelineColorBlendAttachmentState(blendEnabled,
                                              srcColorBlendFactor,
                                              dstColorBlendFactor,
                                              colorBlendOp,
                                              srcAlphaBlendFactor,
                                              dstAlphaBlendFactor,
                                              alphaBlendOp,
                                              colorWriteMask);
  EXPECT_EQ(pipelineColorBlendAttachmentState.blendEnable, blendEnabled);
  EXPECT_EQ(pipelineColorBlendAttachmentState.srcColorBlendFactor, srcColorBlendFactor);
  EXPECT_EQ(pipelineColorBlendAttachmentState.dstColorBlendFactor, dstColorBlendFactor);
  EXPECT_EQ(pipelineColorBlendAttachmentState.colorBlendOp, colorBlendOp);
  EXPECT_EQ(pipelineColorBlendAttachmentState.srcAlphaBlendFactor, srcAlphaBlendFactor);
  EXPECT_EQ(pipelineColorBlendAttachmentState.dstAlphaBlendFactor, dstAlphaBlendFactor);
  EXPECT_EQ(pipelineColorBlendAttachmentState.alphaBlendOp, alphaBlendOp);
  EXPECT_EQ(pipelineColorBlendAttachmentState.colorWriteMask, colorWriteMask);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    PipelineColorBlendAttachmentStateTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO),
                       ::testing::Values(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO),
                       ::testing::Values(VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT),
                       ::testing::Values(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO),
                       ::testing::Values(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO),
                       ::testing::Values(VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT),
                       ::testing::Values(VK_COLOR_COMPONENT_R_BIT, VK_COLOR_COMPONENT_A_BIT)),
    [](const testing::TestParamInfo<PipelineColorBlendAttachmentStateTest::ParamType>& info) {
      const std::string name =
          "_" + std::to_string(std::get<0>(info.param)) + "_" +
          std::to_string(std::get<1>(info.param)) + "_" + std::to_string(std::get<2>(info.param)) +
          "_" + std::to_string(std::get<3>(info.param)) + "_" +
          std::to_string(std::get<4>(info.param)) + "_" + std::to_string(std::get<5>(info.param)) +
          "_" + std::to_string(std::get<6>(info.param)) + "_" +
          std::to_string(std::get<7>(info.param));
      return name;
    });

// ivkGetPipelineViewportStateCreateInfo *******************************

// Parameters:
//   bool: true if viewport is nullptr
//   bool: true if scissor is nullptr
class GetPipelineViewportStateCreateInfoTest
  : public ::testing::TestWithParam<std::tuple<bool, bool>> {};

TEST_P(GetPipelineViewportStateCreateInfoTest, GetPipelineViewportStateCreateInfo) {
  const bool useViewportPtr = std::get<0>(GetParam());
  const bool useScissorPtr = std::get<1>(GetParam());
  constexpr VkViewport viewport{};
  constexpr VkRect2D scissor{};
  const VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo =
      ivkGetPipelineViewportStateCreateInfo(useViewportPtr ? &viewport : nullptr,
                                            useScissorPtr ? &scissor : nullptr);

  EXPECT_EQ(pipelineViewportStateCreateInfo.sType,
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
  EXPECT_EQ(pipelineViewportStateCreateInfo.pNext, nullptr);
  EXPECT_EQ(pipelineViewportStateCreateInfo.flags, 0);
  EXPECT_EQ(pipelineViewportStateCreateInfo.viewportCount, 1);
  EXPECT_EQ(pipelineViewportStateCreateInfo.pViewports, useViewportPtr ? &viewport : nullptr);
  EXPECT_EQ(pipelineViewportStateCreateInfo.scissorCount, 1);
  EXPECT_EQ(pipelineViewportStateCreateInfo.pScissors, useScissorPtr ? &scissor : nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    GetPipelineViewportStateCreateInfoTest,
    ::testing::Combine(::testing::Bool(), ::testing::Bool()),
    [](const testing::TestParamInfo<GetPipelineViewportStateCreateInfoTest::ParamType>& info) {
      const std::string name =
          std::to_string(std::get<0>(info.param)) + "_" + std::to_string(std::get<1>(info.param));
      return name;
    });

// ivkGetImageSubresourceRange *******************************

// Parameters:
//   bool: true if viewport is nullptr
//   bool: true if scissor is nullptr
class GetImageSubresourceRangeTest
  : public ::testing::TestWithParam<std::tuple<VkImageAspectFlags>> {};

TEST_P(GetImageSubresourceRangeTest, GetImageSubresourceRange) {
  const VkImageAspectFlags aspectFlag = std::get<0>(GetParam());

  const VkImageSubresourceRange imageSubresourceRange = ivkGetImageSubresourceRange(aspectFlag);

  EXPECT_EQ(imageSubresourceRange.aspectMask, aspectFlag);
  EXPECT_EQ(imageSubresourceRange.baseMipLevel, 0);
  EXPECT_EQ(imageSubresourceRange.levelCount, 1);
  EXPECT_EQ(imageSubresourceRange.baseArrayLayer, 0);
  EXPECT_EQ(imageSubresourceRange.layerCount, 1);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    GetImageSubresourceRangeTest,
    ::testing::Combine(::testing::Values(VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_DEPTH_BIT)),
    [](const testing::TestParamInfo<GetImageSubresourceRangeTest::ParamType>& info) {
      const std::string name = std::to_string(std::get<0>(info.param));
      return name;
    });

// ivkGetWriteDescriptorSet_ImageInfo *******************************
class GetWriteDescriptorSet_ImageInfoTest
  : public ::testing::TestWithParam<std::tuple<uint32_t, VkDescriptorType, uint32_t>> {};

TEST_P(GetWriteDescriptorSet_ImageInfoTest, GetWriteDescriptorSet_ImageInfo) {
  constexpr VkDescriptorSet descSet = VK_NULL_HANDLE;
  const uint32_t dstBinding = std::get<0>(GetParam());
  const VkDescriptorType descType = std::get<1>(GetParam());
  const uint32_t numDescs = std::get<2>(GetParam());

  const std::array<VkDescriptorImageInfo, 2> pImageInfo = {
      VkDescriptorImageInfo{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED},
      VkDescriptorImageInfo{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED}};

  const VkWriteDescriptorSet imageDescSet = ivkGetWriteDescriptorSet_ImageInfo(
      descSet, dstBinding, descType, numDescs, pImageInfo.data());

  EXPECT_EQ(imageDescSet.sType, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
  EXPECT_EQ(imageDescSet.pNext, nullptr);
  EXPECT_EQ(imageDescSet.dstSet, descSet);
  EXPECT_EQ(imageDescSet.dstBinding, dstBinding);
  EXPECT_EQ(imageDescSet.dstArrayElement, 0);
  EXPECT_EQ(imageDescSet.descriptorCount, numDescs);
  EXPECT_EQ(imageDescSet.descriptorType, descType);
  EXPECT_EQ(imageDescSet.pImageInfo, pImageInfo.data());
  EXPECT_EQ(imageDescSet.pBufferInfo, nullptr);
  EXPECT_EQ(imageDescSet.pTexelBufferView, nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    GetWriteDescriptorSet_ImageInfoTest,
    ::testing::Combine(::testing::Values(0, 1),
                       ::testing::Values(VK_DESCRIPTOR_TYPE_SAMPLER,
                                         VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE),
                       ::testing::Values(1, 2)),
    [](const testing::TestParamInfo<GetWriteDescriptorSet_ImageInfoTest::ParamType>& info) {
      const std::string name = std::to_string(std::get<0>(info.param)) + "_" +
                               std::to_string(std::get<1>(info.param)) + "_" +
                               std::to_string(std::get<2>(info.param));
      return name;
    });

// ivkGetWriteDescriptorSet_BufferInfo *******************************
class GetWriteDescriptorSet_BufferInfoTest
  : public ::testing::TestWithParam<std::tuple<uint32_t, VkDescriptorType, uint32_t>> {};

TEST_P(GetWriteDescriptorSet_BufferInfoTest, GetWriteDescriptorSet_BufferInfo) {
  constexpr VkDescriptorSet descSet = VK_NULL_HANDLE;
  const uint32_t dstBinding = std::get<0>(GetParam());
  const VkDescriptorType descType = std::get<1>(GetParam());
  const uint32_t numDescs = std::get<2>(GetParam());

  const std::array<VkDescriptorBufferInfo, 2> pBufferInfo = {
      VkDescriptorBufferInfo{VK_NULL_HANDLE, 0, VK_WHOLE_SIZE},
      VkDescriptorBufferInfo{VK_NULL_HANDLE, 0, VK_WHOLE_SIZE}};

  const VkWriteDescriptorSet bufferDescSet = ivkGetWriteDescriptorSet_BufferInfo(
      descSet, dstBinding, descType, numDescs, pBufferInfo.data());

  EXPECT_EQ(bufferDescSet.sType, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
  EXPECT_EQ(bufferDescSet.pNext, nullptr);
  EXPECT_EQ(bufferDescSet.dstSet, descSet);
  EXPECT_EQ(bufferDescSet.dstBinding, dstBinding);
  EXPECT_EQ(bufferDescSet.dstArrayElement, 0);
  EXPECT_EQ(bufferDescSet.descriptorCount, numDescs);
  EXPECT_EQ(bufferDescSet.descriptorType, descType);
  EXPECT_EQ(bufferDescSet.pImageInfo, nullptr);
  EXPECT_EQ(bufferDescSet.pBufferInfo, pBufferInfo.data());
  EXPECT_EQ(bufferDescSet.pTexelBufferView, nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    GetWriteDescriptorSet_BufferInfoTest,
    ::testing::Combine(::testing::Values(0, 1),
                       ::testing::Values(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                       ::testing::Values(1, 2)),
    [](const testing::TestParamInfo<GetWriteDescriptorSet_BufferInfoTest::ParamType>& info) {
      const std::string name = std::to_string(std::get<0>(info.param)) + "_" +
                               std::to_string(std::get<1>(info.param)) + "_" +
                               std::to_string(std::get<2>(info.param));
      return name;
    });

// ivkGetPipelineLayoutCreateInfo *******************************
class GetPipelineLayoutCreateInfoTest
  : public ::testing::TestWithParam<std::tuple<uint32_t, VkShaderStageFlags, bool>> {};

TEST_P(GetPipelineLayoutCreateInfoTest, GetPipelineLayoutCreateInfo) {
  const uint32_t numLayouts = std::get<0>(GetParam());
  const VkShaderStageFlags shaderFlags = std::get<1>(GetParam());
  const bool addPushContantRange = std::get<2>(GetParam());

  const std::array<VkDescriptorSetLayout, 2> pDescSetLayout = {VK_NULL_HANDLE, VK_NULL_HANDLE};

  const VkPushConstantRange pPushConstants = {shaderFlags, 0, 0};

  const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = ivkGetPipelineLayoutCreateInfo(
      numLayouts, pDescSetLayout.data(), addPushContantRange ? &pPushConstants : nullptr);

  EXPECT_EQ(pipelineLayoutCreateInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
  EXPECT_EQ(pipelineLayoutCreateInfo.pNext, nullptr);
  EXPECT_EQ(pipelineLayoutCreateInfo.flags, 0);
  EXPECT_EQ(pipelineLayoutCreateInfo.setLayoutCount, numLayouts);
  EXPECT_EQ(pipelineLayoutCreateInfo.pSetLayouts, pDescSetLayout.data());
  EXPECT_EQ(pipelineLayoutCreateInfo.pushConstantRangeCount,
            static_cast<uint32_t>(addPushContantRange));
  EXPECT_EQ(pipelineLayoutCreateInfo.pPushConstantRanges,
            addPushContantRange ? &pPushConstants : nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    GetPipelineLayoutCreateInfoTest,
    ::testing::Combine(::testing::Values(0, 1, 2),
                       ::testing::Values(VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT),
                       ::testing::Bool()),
    [](const testing::TestParamInfo<GetPipelineLayoutCreateInfoTest::ParamType>& info) {
      const std::string name = std::to_string(std::get<0>(info.param)) + "_" +
                               std::to_string(std::get<1>(info.param)) + "_" +
                               std::to_string(std::get<2>(info.param));
      return name;
    });

// ivkGetPushConstantRange *******************************
class GetPushConstantRangeTest
  : public ::testing::TestWithParam<std::tuple<VkShaderStageFlags, uint32_t, uint32_t>> {};

TEST_P(GetPushConstantRangeTest, GetPushConstantRange) {
  const VkShaderStageFlags shaderStageFlags = std::get<0>(GetParam());
  const uint32_t offset = std::get<1>(GetParam());
  const uint32_t size = std::get<2>(GetParam());

  const VkPushConstantRange pushConstantRange =
      ivkGetPushConstantRange(shaderStageFlags, offset, size);

  EXPECT_EQ(pushConstantRange.stageFlags, shaderStageFlags);
  EXPECT_EQ(pushConstantRange.offset, offset);
  EXPECT_EQ(pushConstantRange.size, size);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    GetPushConstantRangeTest,
    ::testing::Combine(::testing::Values(VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT),
                       ::testing::Values(0, 100),
                       ::testing::Values(1000, 2000)),
    [](const testing::TestParamInfo<GetPushConstantRangeTest::ParamType>& info) {
      const std::string name = std::to_string(std::get<0>(info.param)) + "_" +
                               std::to_string(std::get<1>(info.param)) + "_" +
                               std::to_string(std::get<2>(info.param));
      return name;
    });

// ivkGetViewport *******************************
class GetViewportTest : public ::testing::TestWithParam<std::tuple<float, float, float, float>> {};

TEST_P(GetViewportTest, GetViewport) {
  const float x = std::get<0>(GetParam());
  const float y = std::get<1>(GetParam());
  const float width = std::get<2>(GetParam());
  const float height = std::get<3>(GetParam());

  const VkViewport viewport = ivkGetViewport(x, y, width, height);

  EXPECT_EQ(viewport.x, x);
  EXPECT_EQ(viewport.y, y);
  EXPECT_EQ(viewport.width, width);
  EXPECT_EQ(viewport.height, height);
  EXPECT_EQ(viewport.minDepth, 0.0f);
  EXPECT_EQ(viewport.maxDepth, +1.0f);
}

INSTANTIATE_TEST_SUITE_P(AllCombinations,
                         GetViewportTest,
                         ::testing::Combine(::testing::Values(0.f, 50.f),
                                            ::testing::Values(0.f, 50.f),
                                            ::testing::Values(100.f, 500.f),
                                            ::testing::Values(100.f, 500.f)));

// ivkGetRect2D *******************************
class GetRect2DTest
  : public ::testing::TestWithParam<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> {};

TEST_P(GetRect2DTest, GetRect2D) {
  const uint32_t x = std::get<0>(GetParam());
  const uint32_t y = std::get<1>(GetParam());
  const uint32_t width = std::get<2>(GetParam());
  const uint32_t height = std::get<3>(GetParam());

  const VkRect2D rect = ivkGetRect2D(x, y, width, height);

  EXPECT_EQ(rect.offset.x, x);
  EXPECT_EQ(rect.offset.y, y);
  EXPECT_EQ(rect.extent.width, width);
  EXPECT_EQ(rect.extent.height, height);
}

INSTANTIATE_TEST_SUITE_P(AllCombinations,
                         GetRect2DTest,
                         ::testing::Combine(::testing::Values(0, 50),
                                            ::testing::Values(0, 50),
                                            ::testing::Values(100, 500),
                                            ::testing::Values(100, 500)),
                         [](const testing::TestParamInfo<GetRect2DTest::ParamType>& info) {
                           const std::string name = std::to_string(std::get<0>(info.param)) + "_" +
                                                    std::to_string(std::get<1>(info.param)) + "_" +
                                                    std::to_string(std::get<2>(info.param)) + "_" +
                                                    std::to_string(std::get<3>(info.param));
                           return name;
                         });

// ivkGetPipelineShaderStageCreateInfo *******************************
class GetPipelineShaderStageCreateInfoTest
  : public ::testing::TestWithParam<std::tuple<VkShaderStageFlagBits, bool>> {};

TEST_P(GetPipelineShaderStageCreateInfoTest, GetPipelineShaderStageCreateInfo) {
  const VkShaderStageFlagBits stage = std::get<0>(GetParam());
  const VkShaderModule shaderModule = VK_NULL_HANDLE;
  const bool addEntryPoint = std::get<1>(GetParam());
  const char* entryPoint = "main";

  const VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo =
      ivkGetPipelineShaderStageCreateInfo(
          stage, shaderModule, addEntryPoint ? entryPoint : nullptr);

  EXPECT_EQ(pipelineShaderStageCreateInfo.sType,
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
  EXPECT_EQ(pipelineShaderStageCreateInfo.flags, 0);
  EXPECT_EQ(pipelineShaderStageCreateInfo.stage, stage);
  EXPECT_EQ(pipelineShaderStageCreateInfo.module, shaderModule);
  EXPECT_EQ(strcmp(pipelineShaderStageCreateInfo.pName, entryPoint), 0);
  EXPECT_EQ(pipelineShaderStageCreateInfo.pSpecializationInfo, nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    GetPipelineShaderStageCreateInfoTest,
    ::testing::Combine(::testing::Values(VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT),
                       ::testing::Bool()),
    [](const testing::TestParamInfo<GetPipelineShaderStageCreateInfoTest::ParamType>& info) {
      const std::string name =
          std::to_string(std::get<0>(info.param)) + "_" + std::to_string(std::get<1>(info.param));
      return name;
    });

// ivkGetImageCopy2D *******************************
class GetImageCopy2DTest : public ::testing::TestWithParam<std::tuple<uint32_t,
                                                                      uint32_t,
                                                                      VkImageAspectFlags,
                                                                      uint32_t,
                                                                      uint32_t,
                                                                      uint32_t,
                                                                      uint32_t,
                                                                      uint32_t>> {};

TEST_P(GetImageCopy2DTest, GetImageCopy2D) {
  const auto x = std::get<0>(GetParam());
  const auto y = std::get<1>(GetParam());
  const auto aspectMask = std::get<2>(GetParam());
  const auto mipLevel = std::get<3>(GetParam());
  const auto baseArrayLayer = std::get<4>(GetParam());
  const auto layerCount = std::get<5>(GetParam());
  const auto width = std::get<6>(GetParam());
  const auto height = std::get<7>(GetParam());

  const VkOffset2D srcDstOffset = {static_cast<int32_t>(x), static_cast<int32_t>(y)};
  const VkExtent2D imageRegion = {width, height};
  const VkImageSubresourceLayers srcDstImageSubresource = {
      aspectMask, mipLevel, baseArrayLayer, layerCount};

  const VkImageCopy imageCopy =
      ivkGetImageCopy2D(srcDstOffset, srcDstImageSubresource, imageRegion);

  EXPECT_EQ(imageCopy.srcSubresource.aspectMask, aspectMask);
  EXPECT_EQ(imageCopy.srcSubresource.mipLevel, mipLevel);
  EXPECT_EQ(imageCopy.srcSubresource.baseArrayLayer, baseArrayLayer);
  EXPECT_EQ(imageCopy.srcSubresource.layerCount, layerCount);
  EXPECT_EQ(imageCopy.dstSubresource.aspectMask, aspectMask);
  EXPECT_EQ(imageCopy.dstSubresource.mipLevel, mipLevel);
  EXPECT_EQ(imageCopy.dstSubresource.baseArrayLayer, baseArrayLayer);
  EXPECT_EQ(imageCopy.dstSubresource.layerCount, layerCount);
  EXPECT_EQ(imageCopy.srcOffset.x, x);
  EXPECT_EQ(imageCopy.srcOffset.y, y);
  EXPECT_EQ(imageCopy.srcOffset.z, 0);
  EXPECT_EQ(imageCopy.dstOffset.x, x);
  EXPECT_EQ(imageCopy.dstOffset.y, y);
  EXPECT_EQ(imageCopy.dstOffset.z, 0);
  EXPECT_EQ(imageCopy.extent.width, width);
  EXPECT_EQ(imageCopy.extent.height, height);
  EXPECT_EQ(imageCopy.extent.depth, 1);
}

INSTANTIATE_TEST_SUITE_P(AllCombinations,
                         GetImageCopy2DTest,
                         ::testing::Combine(::testing::Values(0, 50),
                                            ::testing::Values(0, 50),
                                            ::testing::Values(VK_IMAGE_ASPECT_COLOR_BIT,
                                                              VK_IMAGE_ASPECT_DEPTH_BIT),
                                            ::testing::Values(0, 5),
                                            ::testing::Values(0, 3),
                                            ::testing::Values(1, 5),
                                            ::testing::Values(100, 500),
                                            ::testing::Values(100, 500)),
                         [](const testing::TestParamInfo<GetImageCopy2DTest::ParamType>& info) {
                           const std::string name = std::to_string(std::get<0>(info.param)) + "_" +
                                                    std::to_string(std::get<1>(info.param)) + "_" +
                                                    std::to_string(std::get<2>(info.param)) + "_" +
                                                    std::to_string(std::get<3>(info.param)) + "_" +
                                                    std::to_string(std::get<4>(info.param)) + "_" +
                                                    std::to_string(std::get<5>(info.param)) + "_" +
                                                    std::to_string(std::get<6>(info.param)) + "_" +
                                                    std::to_string(std::get<7>(info.param));
                           return name;
                         });

// ivkGetBufferImageCopy2D *******************************
class GetBufferImageCopy2DTest : public ::testing::TestWithParam<std::tuple<uint32_t,
                                                                            uint32_t,
                                                                            VkImageAspectFlags,
                                                                            uint32_t,
                                                                            uint32_t,
                                                                            uint32_t,
                                                                            uint32_t,
                                                                            uint32_t>> {};

TEST_P(GetBufferImageCopy2DTest, GetBufferImageCopy2D) {
  const auto x = 0;
  const auto y = 0;
  const auto aspectMask = std::get<0>(GetParam());
  const auto mipLevel = std::get<1>(GetParam());
  const auto baseArrayLayer = std::get<2>(GetParam());
  const auto layerCount = std::get<3>(GetParam());
  const auto width = std::get<4>(GetParam());
  const auto height = std::get<5>(GetParam());
  const auto bufferOffset = std::get<6>(GetParam());
  const auto bufferRowLength = std::get<7>(GetParam());

  const VkOffset2D srcDstOffset = {static_cast<int32_t>(x), static_cast<int32_t>(y)};
  const VkExtent2D imageRegion = {width, height};
  const VkRect2D bufferImageCopyRegion = {srcDstOffset, imageRegion};
  const VkImageSubresourceLayers imageSubresource = {
      aspectMask, mipLevel, baseArrayLayer, layerCount};

  const VkBufferImageCopy bufferCopy = ivkGetBufferImageCopy2D(
      bufferOffset, bufferRowLength, bufferImageCopyRegion, imageSubresource);

  EXPECT_EQ(bufferCopy.imageSubresource.aspectMask, aspectMask);
  EXPECT_EQ(bufferCopy.imageSubresource.mipLevel, mipLevel);
  EXPECT_EQ(bufferCopy.imageSubresource.baseArrayLayer, baseArrayLayer);
  EXPECT_EQ(bufferCopy.imageSubresource.layerCount, layerCount);
  EXPECT_EQ(bufferCopy.imageOffset.x, x);
  EXPECT_EQ(bufferCopy.imageOffset.y, y);
  EXPECT_EQ(bufferCopy.imageOffset.z, 0);
  EXPECT_EQ(bufferCopy.imageExtent.width, width);
  EXPECT_EQ(bufferCopy.imageExtent.height, height);
  EXPECT_EQ(bufferCopy.imageExtent.depth, 1);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    GetBufferImageCopy2DTest,
    ::testing::Combine(::testing::Values(VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_DEPTH_BIT),
                       ::testing::Values(0, 5),
                       ::testing::Values(0, 3),
                       ::testing::Values(1, 5),
                       ::testing::Values(100, 500),
                       ::testing::Values(100, 500),
                       ::testing::Values(0, 50),
                       ::testing::Values(1000, 2000)),
    [](const testing::TestParamInfo<GetBufferImageCopy2DTest::ParamType>& info) {
      const std::string name =
          std::to_string(std::get<0>(info.param)) + "_" + std::to_string(std::get<1>(info.param)) +
          "_" + std::to_string(std::get<2>(info.param)) + "_" +
          std::to_string(std::get<3>(info.param)) + "_" + std::to_string(std::get<4>(info.param)) +
          "_" + std::to_string(std::get<5>(info.param)) + "_" +
          std::to_string(std::get<6>(info.param)) + "_" + std::to_string(std::get<7>(info.param));
      return name;
    });

// ivkGetBufferImageCopy3D *******************************
class GetBufferImageCopy3DTest : public ::testing::TestWithParam<std::tuple<uint32_t,
                                                                            uint32_t,
                                                                            VkImageAspectFlags,
                                                                            uint32_t,
                                                                            uint32_t,
                                                                            uint32_t,
                                                                            uint32_t,
                                                                            uint32_t>> {};

TEST_P(GetBufferImageCopy3DTest, GetBufferImageCopy3D) {
  const auto x = 0u;
  const auto y = 0u;
  const auto z = 0u;
  const auto aspectMask = std::get<0>(GetParam());
  const auto mipLevel = std::get<1>(GetParam());
  const auto baseArrayLayer = std::get<2>(GetParam());
  const auto layerCount = std::get<3>(GetParam());
  const auto width = std::get<4>(GetParam());
  const auto height = std::get<5>(GetParam());
  const auto depth = std::get<5>(GetParam()); // duplicate height as depth
  const auto bufferOffset = std::get<6>(GetParam());
  const auto bufferRowLength = std::get<7>(GetParam());

  const VkOffset3D offset = {static_cast<int32_t>(x), static_cast<int32_t>(y), z};
  const VkExtent3D extent = {width, height, depth};
  const VkImageSubresourceLayers imageSubresource = {
      aspectMask, mipLevel, baseArrayLayer, layerCount};

  const VkBufferImageCopy bufferCopy =
      ivkGetBufferImageCopy3D(bufferOffset, bufferRowLength, offset, extent, imageSubresource);

  EXPECT_EQ(bufferCopy.imageSubresource.aspectMask, aspectMask);
  EXPECT_EQ(bufferCopy.imageSubresource.mipLevel, mipLevel);
  EXPECT_EQ(bufferCopy.imageSubresource.baseArrayLayer, baseArrayLayer);
  EXPECT_EQ(bufferCopy.imageSubresource.layerCount, layerCount);
  EXPECT_EQ(bufferCopy.imageOffset.x, x);
  EXPECT_EQ(bufferCopy.imageOffset.y, y);
  EXPECT_EQ(bufferCopy.imageOffset.z, z);
  EXPECT_EQ(bufferCopy.imageExtent.width, width);
  EXPECT_EQ(bufferCopy.imageExtent.height, height);
  EXPECT_EQ(bufferCopy.imageExtent.depth, depth);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    GetBufferImageCopy3DTest,
    ::testing::Combine(::testing::Values(VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_DEPTH_BIT),
                       ::testing::Values(0, 5),
                       ::testing::Values(0, 3),
                       ::testing::Values(1, 5),
                       ::testing::Values(100, 500),
                       ::testing::Values(100, 500),
                       ::testing::Values(0, 50),
                       ::testing::Values(1000, 2000)),
    [](const testing::TestParamInfo<GetBufferImageCopy3DTest::ParamType>& info) {
      const std::string name =
          std::to_string(std::get<0>(info.param)) + "_" + std::to_string(std::get<1>(info.param)) +
          "_" + std::to_string(std::get<2>(info.param)) + "_" +
          std::to_string(std::get<3>(info.param)) + "_" + std::to_string(std::get<4>(info.param)) +
          "_" + std::to_string(std::get<5>(info.param)) + "_" +
          std::to_string(std::get<6>(info.param)) + "_" + std::to_string(std::get<7>(info.param));
      return name;
    });

// ivkGetVertexInputBindingDescription *******************************
class GetVertexInputBindingDescriptionTest
  : public ::testing::TestWithParam<std::tuple<uint32_t, uint32_t, VkVertexInputRate>> {};

TEST_P(GetVertexInputBindingDescriptionTest, GetVertexInputBindingDescription) {
  const auto binding = std::get<0>(GetParam());
  const auto stride = std::get<1>(GetParam());
  const auto inputRate = std::get<2>(GetParam());

  const VkVertexInputBindingDescription vtxInputBindingDesc =
      ivkGetVertexInputBindingDescription(binding, stride, inputRate);

  EXPECT_EQ(vtxInputBindingDesc.binding, binding);
  EXPECT_EQ(vtxInputBindingDesc.stride, stride);
  EXPECT_EQ(vtxInputBindingDesc.inputRate, inputRate);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    GetVertexInputBindingDescriptionTest,
    ::testing::Combine(::testing::Values(0, 1),
                       ::testing::Values(0, 16),
                       ::testing::Values(VK_VERTEX_INPUT_RATE_VERTEX,
                                         VK_VERTEX_INPUT_RATE_INSTANCE)),
    [](const testing::TestParamInfo<GetVertexInputBindingDescriptionTest::ParamType>& info) {
      const std::string name = std::to_string(std::get<0>(info.param)) + "_" +
                               std::to_string(std::get<1>(info.param)) + "_" +
                               std::to_string(std::get<2>(info.param));
      return name;
    });

// ivkGetVertexInputAttributeDescription *******************************
class GetVertexInputAttributeDescriptionTest
  : public ::testing::TestWithParam<std::tuple<uint32_t, uint32_t, VkFormat, uint32_t>> {};

TEST_P(GetVertexInputAttributeDescriptionTest, GetVertexInputAttributeDescription) {
  const auto location = std::get<0>(GetParam());
  const auto binding = std::get<1>(GetParam());
  const auto format = std::get<2>(GetParam());
  const auto offset = std::get<3>(GetParam());

  const VkVertexInputAttributeDescription vtxInputAttrDesc =
      ivkGetVertexInputAttributeDescription(location, binding, format, offset);

  EXPECT_EQ(vtxInputAttrDesc.location, location);
  EXPECT_EQ(vtxInputAttrDesc.binding, binding);
  EXPECT_EQ(vtxInputAttrDesc.format, format);
  EXPECT_EQ(vtxInputAttrDesc.offset, offset);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    GetVertexInputAttributeDescriptionTest,
    ::testing::Combine(::testing::Values(0, 1),
                       ::testing::Values(0, 1),
                       ::testing::Values(VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8_SNORM),
                       ::testing::Values(0, 16)),
    [](const testing::TestParamInfo<GetVertexInputAttributeDescriptionTest::ParamType>& info) {
      const std::string name = std::to_string(std::get<0>(info.param)) + "_" +
                               std::to_string(std::get<1>(info.param)) + "_" +
                               std::to_string(std::get<2>(info.param)) + "_" +
                               std::to_string(std::get<3>(info.param));
      return name;
    });

} // namespace igl::tests
