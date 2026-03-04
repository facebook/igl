/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/VulkanHelpers.h>

#include <array>
#include <string>

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

// ivkGetWriteDescriptorSetImageInfo *******************************
class GetWriteDescriptorSetImageInfoTest
  : public ::testing::TestWithParam<std::tuple<uint32_t, VkDescriptorType, uint32_t>> {};

TEST_P(GetWriteDescriptorSetImageInfoTest, GetWriteDescriptorSet_ImageInfo) {
  constexpr VkDescriptorSet descSet = VK_NULL_HANDLE;
  const uint32_t dstBinding = std::get<0>(GetParam());
  const VkDescriptorType descType = std::get<1>(GetParam());
  const uint32_t numDescs = std::get<2>(GetParam());

  const std::array<VkDescriptorImageInfo, 2> pImageInfo = {
      VkDescriptorImageInfo{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED},
      VkDescriptorImageInfo{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED}};

  const VkWriteDescriptorSet imageDescSet =
      ivkGetWriteDescriptorSetImageInfo(descSet, dstBinding, descType, numDescs, pImageInfo.data());

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
    GetWriteDescriptorSetImageInfoTest,
    ::testing::Combine(::testing::Values(0, 1),
                       ::testing::Values(VK_DESCRIPTOR_TYPE_SAMPLER,
                                         VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE),
                       ::testing::Values(1, 2)),
    [](const testing::TestParamInfo<GetWriteDescriptorSetImageInfoTest::ParamType>& info) {
      const std::string name = std::to_string(std::get<0>(info.param)) + "_" +
                               std::to_string(std::get<1>(info.param)) + "_" +
                               std::to_string(std::get<2>(info.param));
      return name;
    });

// ivkGetWriteDescriptorSetBufferInfo *******************************
class GetWriteDescriptorSetBufferInfoTest
  : public ::testing::TestWithParam<std::tuple<uint32_t, VkDescriptorType, uint32_t>> {};

TEST_P(GetWriteDescriptorSetBufferInfoTest, GetWriteDescriptorSet_BufferInfo) {
  constexpr VkDescriptorSet descSet = VK_NULL_HANDLE;
  const uint32_t dstBinding = std::get<0>(GetParam());
  const VkDescriptorType descType = std::get<1>(GetParam());
  const uint32_t numDescs = std::get<2>(GetParam());

  const std::array<VkDescriptorBufferInfo, 2> pBufferInfo = {
      VkDescriptorBufferInfo{VK_NULL_HANDLE, 0, VK_WHOLE_SIZE},
      VkDescriptorBufferInfo{VK_NULL_HANDLE, 0, VK_WHOLE_SIZE}};

  const VkWriteDescriptorSet bufferDescSet = ivkGetWriteDescriptorSetBufferInfo(
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
    GetWriteDescriptorSetBufferInfoTest,
    ::testing::Combine(::testing::Values(0, 1),
                       ::testing::Values(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                       ::testing::Values(1, 2)),
    [](const testing::TestParamInfo<GetWriteDescriptorSetBufferInfoTest::ParamType>& info) {
      const std::string name = std::to_string(std::get<0>(info.param)) + "_" +
                               std::to_string(std::get<1>(info.param)) + "_" +
                               std::to_string(std::get<2>(info.param));
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
