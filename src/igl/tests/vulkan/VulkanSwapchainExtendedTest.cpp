/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Common.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <igl/tests/util/device/vulkan/TestDevice.h>
#include <igl/vulkan/VulkanSwapchain.h>

namespace igl::tests {

namespace {
[[maybe_unused]] constexpr uint32_t kWidth = 512;
[[maybe_unused]] constexpr uint32_t kHeight = 512;
} // namespace

class VulkanSwapchainExtendedTest : public ::testing::Test {
 public:
  void SetUp() override {
#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID
    GTEST_SKIP() << "Headless surface not supported on this platform.";
#else
    igl::setDebugBreakEnabled(false);

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

TEST_F(VulkanSwapchainExtendedTest, SwapchainFormat) {
#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID
  GTEST_SKIP() << "Headless surface not supported on this platform.";
#else
  auto swapchain = std::make_unique<igl::vulkan::VulkanSwapchain>(*context_, kWidth, kHeight);
  ASSERT_NE(swapchain, nullptr);

  VkFormat format = swapchain->getFormatColor();
  EXPECT_NE(format, VK_FORMAT_UNDEFINED);

  EXPECT_TRUE(format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_B8G8R8A8_UNORM ||
              format == VK_FORMAT_R8G8B8A8_SRGB || format == VK_FORMAT_B8G8R8A8_SRGB ||
              format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 ||
              format == VK_FORMAT_R16G16B16A16_SFLOAT);
#endif
}

TEST_F(VulkanSwapchainExtendedTest, ImageCount) {
#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID
  GTEST_SKIP() << "Headless surface not supported on this platform.";
#else
  auto swapchain = std::make_unique<igl::vulkan::VulkanSwapchain>(*context_, kWidth, kHeight);
  ASSERT_NE(swapchain, nullptr);

  uint32_t imageCount = swapchain->getNumSwapchainImages();
  EXPECT_GT(imageCount, 0u);
  EXPECT_LE(imageCount, 8u);
#endif
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX
