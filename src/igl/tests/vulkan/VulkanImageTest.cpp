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
#include <igl/vulkan/VulkanImage.h>
#include <memory>

#ifdef __ANDROID__
#include <vulkan/vulkan_android.h>
#endif

#if IGL_PLATFORM_WIN || IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX

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

    device_ = createDevice();
    ASSERT_TRUE(device_ != nullptr);
    auto& device = static_cast<igl::vulkan::Device&>(*device_);
    context_ = &device.getVulkanContext();
    ASSERT_TRUE(context_ != nullptr);
  }

 protected:
  std::unique_ptr<igl::IDevice> createDevice() {
    igl::vulkan::VulkanContextConfig config;
#if IGL_DEBUG
    config.enableValidation = true;
    config.terminateOnValidationError = true;
#else
    config.enableValidation = false;
    config.terminateOnValidationError = false;
#endif // IGL_DEBUG
#ifdef IGL_DISABLE_VALIDATION
    config.enableValidation = false;
    config.terminateOnValidationError = false;
#endif

#if IGL_PLATFORM_WIN
    config.enableGPUAssistedValidation = true;
#else // !IGL_PLATFORM_WIN
    config.enableGPUAssistedValidation = false;
#endif // IGL_PLATFORM_WIN

    std::vector<const char*> deviceExtensions;
#if IGL_PLATFORM_ANDROID
    deviceExtensions.push_back(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
#endif // IGL_PLATFORM_ANDROID

    auto ctx = igl::vulkan::HWDevice::createContext(config, nullptr);

    std::vector<igl::HWDeviceDesc> devices = igl::vulkan::HWDevice::queryDevices(
        *ctx, igl::HWDeviceQueryDesc(igl::HWDeviceType::DiscreteGpu), nullptr);

    if (devices.empty()) {
      devices = igl::vulkan::HWDevice::queryDevices(
          *ctx, igl::HWDeviceQueryDesc(igl::HWDeviceType::IntegratedGpu), nullptr);
    }

    return igl::vulkan::HWDevice::create(
        std::move(ctx), devices[0], 0, 0, deviceExtensions.size(), deviceExtensions.data());
  }

  std::unique_ptr<IDevice> device_;
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
#if IGL_PLATFORM_ANDROID && __ANDROID_MIN_SDK_VERSION__ >= 26
      nullptr,
#endif
      "Image: vulkan export memory");
  ASSERT_NE(vulkanImage.valid(), false);
  EXPECT_TRUE(vulkanImage.isExported_);
#if IGL_PLATFORM_WIN
  EXPECT_NE(vulkanImage.exportedMemoryHandle_, nullptr);
  EXPECT_NE(vulkanImage.getVkImage(), static_cast<VkImage_T*>(VK_NULL_HANDLE));
#elif IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX
  EXPECT_NE(vulkanImage.exportedFd_, -1);
  EXPECT_NE(vulkanImage.getVkImage(), VK_NULL_HANDLE);
#endif
}

#if IGL_PLATFORM_WIN
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
#endif // IGL_PLATFORM_WIN

} // namespace igl::tests

#endif // IGL_PLATFORM_WIN || IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX
