/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <vulkan/vulkan_core.h>

namespace igl::vulkan {

// Human-readable names for Vulkan enum values, intended for debug logging and diagnostics.
// Unrecognized values return a stable "..._UNKNOWN" sentinel rather than nullptr, so results are
// always safe to pass to a %s format specifier. Only the subset of enum values relevant to IGL
// import/sampler paths is enumerated; extend the switches as new values are needed.

[[nodiscard]] const char* vkFormatToString(VkFormat format) noexcept;

[[nodiscard]] const char* vkComponentSwizzleToString(VkComponentSwizzle swizzle) noexcept;

[[nodiscard]] const char* vkSamplerYcbcrModelConversionToString(
    VkSamplerYcbcrModelConversion model) noexcept;

[[nodiscard]] const char* vkSamplerYcbcrRangeToString(VkSamplerYcbcrRange range) noexcept;

[[nodiscard]] const char* vkDriverIdToString(VkDriverId driverId) noexcept;

} // namespace igl::vulkan
