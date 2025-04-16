/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>

#include <igl/tests/util/device/TestDevice.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX

namespace igl::tests {

namespace {
constexpr uint32_t kWidth = 1024;
constexpr uint32_t kHeight = 1024;
constexpr VkFormat kFormat = VK_FORMAT_R8G8B8A8_UNORM;
} // namespace

//
// VulkanImageTest
//
// Unit tests for igl::vulkan::VulkanImage.
//
class VulkanImageTest : public ::testing::Test {
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

TEST_F(VulkanImageTest, CreateImageWithExportedMemory) {
  auto vulkanImage = igl::vulkan::VulkanImage::createWithExportMemory(
      *context_,
      context_->getVkDevice(),
      VkExtent3D{.width = kWidth, .height = kHeight, .depth = 1},
      VK_IMAGE_TYPE_2D,
      kFormat,
      1, /* mipLevels */
      1, /* arrayLayers */
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      0, /* createFlags */
      VK_SAMPLE_COUNT_1_BIT,
      "Image: vulkan export memory");
  ASSERT_NE(vulkanImage.valid(), false);
  EXPECT_TRUE(vulkanImage.isExported_);
#if IGL_PLATFORM_WINDOWS
  EXPECT_NE(vulkanImage.exportedMemoryHandle_, nullptr);
  EXPECT_NE(vulkanImage.getVkImage(), static_cast<VkImage_T*>(VK_NULL_HANDLE));
#elif IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX
  EXPECT_NE(vulkanImage.exportedFd_, -1);
  EXPECT_NE(vulkanImage.getVkImage(), VK_NULL_HANDLE);
#endif
}

#if IGL_PLATFORM_WINDOWS
TEST_F(VulkanImageTest, CreateImageWithImportedMemoryWin32) {
  auto exportedImage = igl::vulkan::VulkanImage::createWithExportMemory(
      *context_,
      context_->getVkDevice(),
      VkExtent3D{.width = kWidth, .height = kHeight, .depth = 1},
      VK_IMAGE_TYPE_2D,
      kFormat,
      1, /* mipLevels */
      1, /* arrayLayers */
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      0, /* createFlags */
      VK_SAMPLE_COUNT_1_BIT,
      "Image: vulkan export memory");
  ASSERT_NE(exportedImage.valid(), false);
  EXPECT_NE(exportedImage.exportedMemoryHandle_, nullptr);

  auto importedImage =
      igl::vulkan::VulkanImage(*context_,
                               exportedImage.exportedMemoryHandle_,
                               context_->getVkDevice(),
                               VkExtent3D{.width = kWidth, .height = kHeight, .depth = 1},
                               VK_IMAGE_TYPE_2D,
                               kFormat,
                               1, /* mipLevels */
                               1, /* arrayLayers */
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                               0, /* createFlags */
                               VK_SAMPLE_COUNT_1_BIT,
                               "Image: vulkan import memory");
  EXPECT_TRUE(importedImage.isImported_);
  EXPECT_NE(importedImage.getVkImage(), static_cast<VkImage_T*>(VK_NULL_HANDLE));
}
#endif // IGL_PLATFORM_WINDOWS

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX
