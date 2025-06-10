/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Common.h>

#ifdef __ANDROID__
#include <vulkan/vulkan_android.h>
#endif

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX
#include <cstddef>
#include <gtest/gtest.h>
#include <memory>

#include <igl/tests/util/device/vulkan/TestDevice.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanSwapchain.h>

namespace igl::tests {

namespace {
[[maybe_unused]] constexpr uint32_t kWidth = 1024;
[[maybe_unused]] constexpr uint32_t kHeight = 1024;
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
#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID
    // @fb-only
    GTEST_SKIP() << "Fix these tests on Windows and Android, no headless surface support there.";
#else
    igl::setDebugBreakEnabled(false);

    // Add headless config for creating swapchains without a window
    igl::vulkan::VulkanContextConfig config = util::device::vulkan::getContextConfig(true);
    config.headless = true;

    device_ = igl::tests::util::device::vulkan::createTestDevice(config);
    ASSERT_TRUE(device_ != nullptr);
    auto& device = static_cast<igl::vulkan::Device&>(*device_);
    context_ = &device.getVulkanContext();
    ASSERT_TRUE(context_ != nullptr);
#endif
  }

 protected:
  std::shared_ptr<IDevice> device_;
  vulkan::VulkanContext* context_ = nullptr;
};

TEST_F(VulkanSwapchainTest, CreateVulkanSwapchain) {
#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID
  // @fb-only
  GTEST_SKIP() << "Fix these tests on Windows and Android, no headless surface support there.";
#else
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
#endif
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX
