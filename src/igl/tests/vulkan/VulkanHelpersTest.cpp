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

// GetDescriptorSetLayoutBinding **************************************************************

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

} // namespace igl::tests
