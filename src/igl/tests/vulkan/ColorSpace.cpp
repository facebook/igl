/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <igl/vulkan/ColorSpace.h>

namespace igl::vulkan::tests {
TEST(ColorSpaceTest, colorSpaceToVkColorSpace) {
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_LINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::SRGB_LINEAR));
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            colorSpaceToVkColorSpace(ColorSpace::SRGB_NONLINEAR));
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::DISPLAY_P3_NONLINEAR));
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::DISPLAY_P3_LINEAR));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::EXTENDED_SRGB_LINEAR));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::DCI_P3_NONLINEAR));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_LINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::BT709_LINEAR));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::BT709_NONLINEAR));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT2020_LINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::BT2020_LINEAR));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_HDR10_ST2084_EXT,
            colorSpaceToVkColorSpace(ColorSpace::HDR10_ST2084));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_DOLBYVISION_EXT,
            colorSpaceToVkColorSpace(ColorSpace::DOLBYVISION));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_HDR10_HLG_EXT,
            colorSpaceToVkColorSpace(ColorSpace::HDR10_HLG));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::ADOBERGB_LINEAR));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::ADOBERGB_NONLINEAR));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_PASS_THROUGH_EXT,
            colorSpaceToVkColorSpace(ColorSpace::PASS_THROUGH));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::EXTENDED_SRGB_NONLINEAR));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_NATIVE_AMD,
            colorSpaceToVkColorSpace(ColorSpace::DISPLAY_NATIVE_AMD));

  /* asserts for IGL_DEBUG_ASSERT_NOT_IMPLEMENTED but would be valid otherwise
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::BT2020_NONLINEAR));
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::BT601_NONLINEAR));
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::BT2100_HLG_NONLINEAR));
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::BT2100_PQ_NONLINEAR));
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::DISPLAY_P3_NONLINEAR));*/
}

TEST(ColorSpaceTest, vkColorSpaceToColorSpace) {
  ASSERT_EQ(ColorSpace::SRGB_NONLINEAR,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_SRGB_NONLINEAR_KHR));

  ASSERT_EQ(ColorSpace::DISPLAY_P3_NONLINEAR,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT));

  ASSERT_EQ(ColorSpace::EXTENDED_SRGB_LINEAR,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT));

  ASSERT_EQ(ColorSpace::DISPLAY_P3_LINEAR,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT));

  ASSERT_EQ(ColorSpace::DCI_P3_NONLINEAR,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT));

  ASSERT_EQ(ColorSpace::BT709_LINEAR,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_LINEAR_EXT));

  ASSERT_EQ(ColorSpace::BT709_NONLINEAR,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT));

  ASSERT_EQ(ColorSpace::BT2020_LINEAR,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_BT2020_LINEAR_EXT));

  ASSERT_EQ(ColorSpace::HDR10_ST2084,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_HDR10_ST2084_EXT));

  ASSERT_EQ(ColorSpace::DOLBYVISION,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_DOLBYVISION_EXT));

  ASSERT_EQ(ColorSpace::HDR10_HLG,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_HDR10_HLG_EXT));

  ASSERT_EQ(ColorSpace::ADOBERGB_LINEAR,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT));

  ASSERT_EQ(ColorSpace::ADOBERGB_NONLINEAR,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT));

  ASSERT_EQ(ColorSpace::PASS_THROUGH,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_PASS_THROUGH_EXT));

  ASSERT_EQ(ColorSpace::EXTENDED_SRGB_NONLINEAR,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT));

  ASSERT_EQ(ColorSpace::DISPLAY_NATIVE_AMD,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_NATIVE_AMD));
}

} // namespace igl::vulkan::tests
