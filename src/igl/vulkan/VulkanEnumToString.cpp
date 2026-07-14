/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/VulkanEnumToString.h>

namespace igl::vulkan {

#define IGL_VK_ENUM_CASE(value) \
  case value:                   \
    return #value

const char* vkFormatToString(VkFormat format) noexcept {
  // value-to-string maps a known subset; default handles the rest
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (format) {
    IGL_VK_ENUM_CASE(VK_FORMAT_UNDEFINED);
    IGL_VK_ENUM_CASE(VK_FORMAT_R8G8B8A8_UNORM);
    IGL_VK_ENUM_CASE(VK_FORMAT_B8G8R8A8_UNORM);
    IGL_VK_ENUM_CASE(VK_FORMAT_R8G8B8_UNORM);
    IGL_VK_ENUM_CASE(VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM);
    IGL_VK_ENUM_CASE(VK_FORMAT_G8_B8R8_2PLANE_420_UNORM);
  default:
    return "VK_FORMAT_UNKNOWN";
  }
}

const char* vkComponentSwizzleToString(VkComponentSwizzle swizzle) noexcept {
  // value-to-string maps a known subset; default handles the rest
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (swizzle) {
    IGL_VK_ENUM_CASE(VK_COMPONENT_SWIZZLE_IDENTITY);
    IGL_VK_ENUM_CASE(VK_COMPONENT_SWIZZLE_ZERO);
    IGL_VK_ENUM_CASE(VK_COMPONENT_SWIZZLE_ONE);
    IGL_VK_ENUM_CASE(VK_COMPONENT_SWIZZLE_R);
    IGL_VK_ENUM_CASE(VK_COMPONENT_SWIZZLE_G);
    IGL_VK_ENUM_CASE(VK_COMPONENT_SWIZZLE_B);
    IGL_VK_ENUM_CASE(VK_COMPONENT_SWIZZLE_A);
  default:
    return "VK_COMPONENT_SWIZZLE_UNKNOWN";
  }
}

const char* vkSamplerYcbcrModelConversionToString(VkSamplerYcbcrModelConversion model) noexcept {
  // value-to-string maps a known subset; default handles the rest
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (model) {
    IGL_VK_ENUM_CASE(VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY);
    IGL_VK_ENUM_CASE(VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_IDENTITY);
    IGL_VK_ENUM_CASE(VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709);
    IGL_VK_ENUM_CASE(VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601);
    IGL_VK_ENUM_CASE(VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020);
  default:
    return "VK_SAMPLER_YCBCR_MODEL_CONVERSION_UNKNOWN";
  }
}

const char* vkSamplerYcbcrRangeToString(VkSamplerYcbcrRange range) noexcept {
  // value-to-string maps a known subset; default handles the rest
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (range) {
    IGL_VK_ENUM_CASE(VK_SAMPLER_YCBCR_RANGE_ITU_FULL);
    IGL_VK_ENUM_CASE(VK_SAMPLER_YCBCR_RANGE_ITU_NARROW);
  default:
    return "VK_SAMPLER_YCBCR_RANGE_UNKNOWN";
  }
}

const char* vkDriverIdToString(VkDriverId driverId) noexcept {
  // value-to-string maps a known subset; default handles the rest
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (driverId) {
    IGL_VK_ENUM_CASE(VK_DRIVER_ID_QUALCOMM_PROPRIETARY);
    IGL_VK_ENUM_CASE(VK_DRIVER_ID_ARM_PROPRIETARY);
    IGL_VK_ENUM_CASE(VK_DRIVER_ID_GOOGLE_SWIFTSHADER);
    IGL_VK_ENUM_CASE(VK_DRIVER_ID_MESA_RADV);
    IGL_VK_ENUM_CASE(VK_DRIVER_ID_MESA_LLVMPIPE);
  default:
    return "VK_DRIVER_ID_UNKNOWN";
  }
}

#undef IGL_VK_ENUM_CASE

} // namespace igl::vulkan
