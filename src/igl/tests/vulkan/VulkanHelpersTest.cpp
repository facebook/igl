/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <igl/vulkan/Common.h>

#ifdef __ANDROID__
#include <vulkan/vulkan_android.h>
#endif

namespace igl::tests {

//
// VulkanHelpersTest
//
// Unit tests for functions in VulkanHelpers.{h|c}.
//

// ivkGetDescriptorSetLayoutBinding **************************************************************

class DescriptorSetLayoutTest
  : public ::testing::TestWithParam<std::tuple<uint32_t, VkDescriptorType, uint32_t>> {};

TEST_P(DescriptorSetLayoutTest, GetDescriptorSetLayoutBinding) {
  const uint32_t binding = std::get<0>(GetParam());
  const VkDescriptorType descriptorType = std::get<1>(GetParam());
  const uint32_t count = std::get<2>(GetParam());

  const auto descSetLayoutBinding =
      ivkGetDescriptorSetLayoutBinding(binding, descriptorType, count);
  EXPECT_EQ(descSetLayoutBinding.binding, binding);
  EXPECT_EQ(descSetLayoutBinding.descriptorType, descriptorType);
  EXPECT_EQ(descSetLayoutBinding.descriptorCount, count);
  EXPECT_EQ(descSetLayoutBinding.stageFlags,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                VK_SHADER_STAGE_COMPUTE_BIT);
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
      const std::string name = "binding_" + std::to_string(std::get<0>(info.param)) +
                               "__descriptorType_" + std::to_string(std::get<1>(info.param)) +
                               "__count_" + std::to_string(std::get<2>(info.param));
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
      const std::string name = "format_" + std::to_string(std::get<0>(info.param)) + "__loadOp_" +
                               std::to_string(std::get<1>(info.param)) + "__storeOp_" +
                               std::to_string(std::get<2>(info.param)) + "__initialLayout_" +
                               std::to_string(std::get<3>(info.param)) + "__finalLayout_" +
                               std::to_string(std::get<4>(info.param)) + "__samples_" +
                               std::to_string(std::get<5>(info.param));
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
      const std::string name = "attachment_" + std::to_string(std::get<0>(info.param)) +
                               "__layout_" + std::to_string(std::get<1>(info.param));
      return name;
    });

} // namespace igl::tests
