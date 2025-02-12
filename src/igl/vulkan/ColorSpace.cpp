/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/ColorSpace.h>

namespace igl::vulkan {
VkColorSpaceKHR colorSpaceToVkColorSpace(ColorSpace colorSpace) {
  switch (colorSpace) {
  case ColorSpace::SRGB_LINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_BT709_LINEAR_EXT; // closest thing to linear srgb
  case ColorSpace::SRGB_NONLINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  case ColorSpace::DISPLAY_P3_NONLINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
  case ColorSpace::DISPLAY_P3_LINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT;
  case ColorSpace::EXTENDED_SRGB_LINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
  case ColorSpace::DCI_P3_NONLINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT;
  case ColorSpace::BT709_LINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_BT709_LINEAR_EXT;
  case ColorSpace::BT709_NONLINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT;
  case ColorSpace::BT2020_LINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_BT2020_LINEAR_EXT;
  case ColorSpace::HDR10_ST2084:
    return VkColorSpaceKHR::VK_COLOR_SPACE_HDR10_ST2084_EXT;
  case ColorSpace::DOLBYVISION:
    return VkColorSpaceKHR::VK_COLOR_SPACE_DOLBYVISION_EXT;
  case ColorSpace::HDR10_HLG:
    return VkColorSpaceKHR::VK_COLOR_SPACE_HDR10_HLG_EXT;
  case ColorSpace::ADOBERGB_LINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT;
  case ColorSpace::ADOBERGB_NONLINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT;
  case ColorSpace::PASS_THROUGH:
    return VkColorSpaceKHR::VK_COLOR_SPACE_PASS_THROUGH_EXT;
  case ColorSpace::EXTENDED_SRGB_NONLINEAR:
    return VkColorSpaceKHR::VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT;
  case ColorSpace::DISPLAY_NATIVE_AMD:
    return VkColorSpaceKHR::VK_COLOR_SPACE_DISPLAY_NATIVE_AMD;
  case ColorSpace::BT2020_NONLINEAR:
  case ColorSpace::BT601_NONLINEAR:
  case ColorSpace::BT2100_HLG_NONLINEAR:
  case ColorSpace::BT2100_PQ_NONLINEAR:
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
    return VkColorSpaceKHR::VK_COLOR_SPACE_BT709_NONLINEAR_EXT;
  }
  IGL_UNREACHABLE_RETURN(VK_COLOR_SPACE_BT709_NONLINEAR_EXT);
}

ColorSpace vkColorSpaceToColorSpace(VkColorSpaceKHR colorSpace) {
  switch (colorSpace) {
  case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
    return ColorSpace::SRGB_NONLINEAR;
  case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT:
    return ColorSpace::DISPLAY_P3_NONLINEAR;
  case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
    return ColorSpace::EXTENDED_SRGB_LINEAR;
  case VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT:
    return ColorSpace::DISPLAY_P3_LINEAR;
  case VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT:
    return ColorSpace::DCI_P3_NONLINEAR;
  case VK_COLOR_SPACE_BT709_LINEAR_EXT:
    return ColorSpace::BT709_LINEAR;
  case VK_COLOR_SPACE_BT709_NONLINEAR_EXT:
    return ColorSpace::BT709_NONLINEAR;
  case VK_COLOR_SPACE_BT2020_LINEAR_EXT:
    return ColorSpace::BT2020_LINEAR;
  case VK_COLOR_SPACE_HDR10_ST2084_EXT:
    return ColorSpace::HDR10_ST2084;
  case VK_COLOR_SPACE_DOLBYVISION_EXT:
    return ColorSpace::DOLBYVISION;
  case VK_COLOR_SPACE_HDR10_HLG_EXT:
    return ColorSpace::HDR10_HLG;
  case VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT:
    return ColorSpace::ADOBERGB_LINEAR;
  case VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT:
    return ColorSpace::ADOBERGB_NONLINEAR;
  case VK_COLOR_SPACE_PASS_THROUGH_EXT:
    return ColorSpace::PASS_THROUGH;
  case VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT:
    return ColorSpace::EXTENDED_SRGB_NONLINEAR;
  case VK_COLOR_SPACE_DISPLAY_NATIVE_AMD:
    return ColorSpace::DISPLAY_NATIVE_AMD;
  default:
    IGL_DEBUG_ASSERT_NOT_REACHED();
    return ColorSpace::SRGB_NONLINEAR;
  }
}
} // namespace igl::vulkan
