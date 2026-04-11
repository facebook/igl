/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/Common.h>

namespace igl::vulkan::tests {
TEST(ColorSpaceTest, colorSpaceToVkColorSpace) {
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_LINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::SRGBLinear));
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            colorSpaceToVkColorSpace(ColorSpace::SRGBNonlinear));
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::DisplayP3Nonlinear));
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::DisplayP3Linear));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::ExtendedSRGBLinear));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::DCIP3Nonlinear));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_LINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::BT709Linear));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::BT709Nonlinear));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT2020_LINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::BT2020Linear));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_HDR10_ST2084_EXT,
            colorSpaceToVkColorSpace(ColorSpace::HDR10St2084));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_DOLBYVISION_EXT,
            colorSpaceToVkColorSpace(ColorSpace::DolbyVision));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_HDR10_HLG_EXT,
            colorSpaceToVkColorSpace(ColorSpace::HDR10HLG));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::AdobeRGBLinear));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::AdobeRGBNonlinear));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_PASS_THROUGH_EXT,
            colorSpaceToVkColorSpace(ColorSpace::PassThrough));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::ExtendedSRGBNonlinear));

  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_NATIVE_AMD,
            colorSpaceToVkColorSpace(ColorSpace::DisplayNativeAMD));

  /* asserts for IGL_DEBUG_ASSERT_NOT_IMPLEMENTED but would be valid otherwise
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::BT2020Nonlinear));
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::BT601Nonlinear));
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::BT2100HLGNonlinear));
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::BT2100PQNonlinear));
  ASSERT_EQ(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT,
            colorSpaceToVkColorSpace(ColorSpace::DisplayP3Nonlinear));*/
}

TEST(ColorSpaceTest, vkColorSpaceToColorSpace) {
  ASSERT_EQ(ColorSpace::SRGBNonlinear,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_SRGB_NONLINEAR_KHR));

  ASSERT_EQ(ColorSpace::DisplayP3Nonlinear,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT));

  ASSERT_EQ(ColorSpace::ExtendedSRGBLinear,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT));

  ASSERT_EQ(ColorSpace::DisplayP3Linear,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT));

  ASSERT_EQ(ColorSpace::DCIP3Nonlinear,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT));

  ASSERT_EQ(ColorSpace::BT709Linear,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_LINEAR_EXT));

  ASSERT_EQ(ColorSpace::BT709Nonlinear,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT));

  ASSERT_EQ(ColorSpace::BT2020Linear,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_BT2020_LINEAR_EXT));

  ASSERT_EQ(ColorSpace::HDR10St2084,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_HDR10_ST2084_EXT));

  ASSERT_EQ(ColorSpace::DolbyVision,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_DOLBYVISION_EXT));

  ASSERT_EQ(ColorSpace::HDR10HLG,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_HDR10_HLG_EXT));

  ASSERT_EQ(ColorSpace::AdobeRGBLinear,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT));

  ASSERT_EQ(ColorSpace::AdobeRGBNonlinear,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT));

  ASSERT_EQ(ColorSpace::PassThrough,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_PASS_THROUGH_EXT));

  ASSERT_EQ(ColorSpace::ExtendedSRGBNonlinear,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT));

  ASSERT_EQ(ColorSpace::DisplayNativeAMD,
            vkColorSpaceToColorSpace(VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_NATIVE_AMD));
}

} // namespace igl::vulkan::tests
