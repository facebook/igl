/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/VulkanImageView.h>

#include <igl/Common.h>
#include <igl/tests/util/device/TestDevice.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX

namespace igl::tests {

namespace {
constexpr uint32_t kWidth = 64;
constexpr uint32_t kHeight = 64;
constexpr VkFormat kFormat = VK_FORMAT_R8G8B8A8_UNORM;
} // namespace

class VulkanImageViewTest : public ::testing::Test {
 public:
  void SetUp() override {
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

TEST_F(VulkanImageViewTest, ImageViewCreateInfoDefaultValues) {
  vulkan::VulkanImageViewCreateInfo ci;

  EXPECT_EQ(ci.image, VK_NULL_HANDLE);
  EXPECT_EQ(ci.viewType, VK_IMAGE_VIEW_TYPE_2D);
  EXPECT_EQ(ci.format, VK_FORMAT_UNDEFINED);
  EXPECT_EQ(ci.components.r, VK_COMPONENT_SWIZZLE_IDENTITY);
  EXPECT_EQ(ci.components.g, VK_COMPONENT_SWIZZLE_IDENTITY);
  EXPECT_EQ(ci.components.b, VK_COMPONENT_SWIZZLE_IDENTITY);
  EXPECT_EQ(ci.components.a, VK_COMPONENT_SWIZZLE_IDENTITY);
  EXPECT_EQ(ci.subresourceRange.aspectMask, VK_IMAGE_ASPECT_COLOR_BIT);
  EXPECT_EQ(ci.subresourceRange.baseMipLevel, 0u);
  EXPECT_EQ(ci.subresourceRange.levelCount, 1u);
  EXPECT_EQ(ci.subresourceRange.baseArrayLayer, 0u);
  EXPECT_EQ(ci.subresourceRange.layerCount, 1u);
}

TEST_F(VulkanImageViewTest, CreateImageViewWithCreateInfo) {
  vulkan::VulkanImage image(*context_,
                            VkExtent3D{.width = kWidth, .height = kHeight, .depth = 1},
                            VK_IMAGE_TYPE_2D,
                            kFormat,
                            1, /* mipLevels */
                            1, /* arrayLayers */
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, /* memFlags */
                            0, /* createFlags */
                            VK_SAMPLE_COUNT_1_BIT,
                            "Test Image");
  ASSERT_TRUE(image.valid());

  vulkan::VulkanImageViewCreateInfo ci;
  ci.image = image.getVkImage();
  ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
  ci.format = kFormat;
  ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  ci.subresourceRange.baseMipLevel = 0;
  ci.subresourceRange.levelCount = 1;
  ci.subresourceRange.baseArrayLayer = 0;
  ci.subresourceRange.layerCount = 1;

  vulkan::VulkanImageView imageView(*context_, ci, "Test ImageView");

  EXPECT_TRUE(imageView.valid());
  EXPECT_NE(imageView.getVkImageView(), VK_NULL_HANDLE);
  EXPECT_EQ(imageView.getVkImageAspectFlags(), VK_IMAGE_ASPECT_COLOR_BIT);
}

TEST_F(VulkanImageViewTest, CreateImageViewWithVkCreateInfo) {
  vulkan::VulkanImage image(*context_,
                            VkExtent3D{.width = kWidth, .height = kHeight, .depth = 1},
                            VK_IMAGE_TYPE_2D,
                            kFormat,
                            1, /* mipLevels */
                            1, /* arrayLayers */
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, /* memFlags */
                            0, /* createFlags */
                            VK_SAMPLE_COUNT_1_BIT,
                            "Test Image");
  ASSERT_TRUE(image.valid());

  VkImageViewCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .image = image.getVkImage(),
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = kFormat,
      .components =
          {
              .r = VK_COMPONENT_SWIZZLE_IDENTITY,
              .g = VK_COMPONENT_SWIZZLE_IDENTITY,
              .b = VK_COMPONENT_SWIZZLE_IDENTITY,
              .a = VK_COMPONENT_SWIZZLE_IDENTITY,
          },
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };

  vulkan::VulkanImageView imageView(*context_, ci, "Test ImageView");

  EXPECT_TRUE(imageView.valid());
  EXPECT_NE(imageView.getVkImageView(), VK_NULL_HANDLE);
  EXPECT_EQ(imageView.getVkImageAspectFlags(), VK_IMAGE_ASPECT_COLOR_BIT);
}
} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX
