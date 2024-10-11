/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstddef>
#include <gtest/gtest.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/HWDevice.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanSwapchain.h>
#include <memory>

#include <igl/tests/util/device/TestDevice.h>

#ifdef __ANDROID__
#include <vulkan/vulkan_android.h>
#endif

#if IGL_PLATFORM_WIN || IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX

namespace igl::tests {

namespace {
constexpr uint32_t kWidth = 1024;
constexpr uint32_t kHeight = 1024;
} // namespace

//
// VulkanSwapchainTest
//
// Unit tests for igl::vulkan::VulkanSwapchain
//
class VulkanSwapchainTest : public ::testing::Test {
 public:
  // Set up common resources.
  void SetUp() override {
    // Turn off debug break so unit tests can run
    igl::setDebugBreakEnabled(false);

    device_ = igl::tests::util::device::createTestDevice(igl::BackendType::Vulkan);
    ASSERT_TRUE(device_ != nullptr);
    auto& device = static_cast<igl::vulkan::Device&>(*device_);
    context_ = &device.getVulkanContext();
    ASSERT_TRUE(context_ != nullptr);
  }

 protected:
  std::shared_ptr<IDevice> device_;
  vulkan::VulkanContext* context_ = nullptr;
};

TEST_F(VulkanSwapchainTest, CreateVulkanSwapchain) {
  auto swapchain = std::make_unique<igl::vulkan::VulkanSwapchain>(*context_, kWidth, kHeight);
  ASSERT_NE(swapchain, nullptr);

  ASSERT_EQ(swapchain->getWidth(), kWidth);
  ASSERT_EQ(swapchain->getHeight(), kHeight);

  const VkExtent2D extent = swapchain->getExtent();
  ASSERT_EQ(extent.width, kWidth);
  ASSERT_EQ(extent.height, kHeight);

  ASSERT_NE(swapchain->getFormatColor(), VK_FORMAT_UNDEFINED);

  ASSERT_GT(swapchain->getNumSwapchainImages(), 0);

  ASSERT_EQ(swapchain->getCurrentImageIndex(), 0);
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WIN || IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX
