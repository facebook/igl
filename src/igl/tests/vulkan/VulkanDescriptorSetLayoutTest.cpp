/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/VulkanDescriptorSetLayout.h>

#include "../util/TestDevice.h"

#include <array>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class VulkanDescriptorSetLayoutTest : public ::testing::Test {
 public:
  VulkanDescriptorSetLayoutTest() = default;
  ~VulkanDescriptorSetLayoutTest() override = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    iglDev_ = util::createTestDevice();
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_EQ(iglDev_->getBackendType(), BackendType::Vulkan) << "Test requires Vulkan backend";
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;

  igl::vulkan::VulkanContext& getVulkanContext() {
    auto& device = static_cast<igl::vulkan::Device&>(*iglDev_);
    return device.getVulkanContext();
  }
};

TEST_F(VulkanDescriptorSetLayoutTest, SingleBinding) {
  auto& ctx = getVulkanContext();

  VkDescriptorSetLayoutBinding binding = {};
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  binding.pImmutableSamplers = nullptr;

  VkDescriptorBindingFlags bindingFlags = 0;
  auto layout = std::make_unique<igl::vulkan::VulkanDescriptorSetLayout>(
      ctx, 0, 1, &binding, &bindingFlags, "testSingleBinding");

  ASSERT_NE(layout, nullptr);
  EXPECT_NE(layout->getVkDescriptorSetLayout(), VK_NULL_HANDLE);
  EXPECT_EQ(layout->numBindings, 1u);
}

TEST_F(VulkanDescriptorSetLayoutTest, MultipleBindings) {
  auto& ctx = getVulkanContext();

  std::array<VkDescriptorSetLayoutBinding, 3> bindings = {};

  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  bindings[1].binding = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  bindings[2].binding = 2;
  bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[2].descriptorCount = 1;
  bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  std::array<VkDescriptorBindingFlags, 3> bindingFlags = {};
  auto layout = std::make_unique<igl::vulkan::VulkanDescriptorSetLayout>(
      ctx, 0, 3, bindings.data(), bindingFlags.data(), "testMultipleBindings");

  ASSERT_NE(layout, nullptr);
  EXPECT_NE(layout->getVkDescriptorSetLayout(), VK_NULL_HANDLE);
  EXPECT_EQ(layout->numBindings, 3u);
}

TEST_F(VulkanDescriptorSetLayoutTest, WithBindingFlags) {
  auto& ctx = getVulkanContext();

  VkDescriptorSetLayoutBinding binding = {};
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorBindingFlags flags = 0;

  auto layout = std::make_unique<igl::vulkan::VulkanDescriptorSetLayout>(
      ctx, 0, 1, &binding, &flags, "testWithFlags");

  ASSERT_NE(layout, nullptr);
  EXPECT_NE(layout->getVkDescriptorSetLayout(), VK_NULL_HANDLE);
  EXPECT_EQ(layout->numBindings, 1u);
}

TEST_F(VulkanDescriptorSetLayoutTest, DestructorCleanup) {
  auto& ctx = getVulkanContext();

  VkDescriptorSetLayoutBinding binding = {};
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  {
    VkDescriptorBindingFlags bindingFlags = 0;
    auto layout = std::make_unique<igl::vulkan::VulkanDescriptorSetLayout>(
        ctx, 0, 1, &binding, &bindingFlags, "testDestructor");

    ASSERT_NE(layout, nullptr);
    EXPECT_NE(layout->getVkDescriptorSetLayout(), VK_NULL_HANDLE);
  }

  ctx.waitDeferredTasks();
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
