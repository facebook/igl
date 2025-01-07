/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace igl::vulkan::util {

constexpr uint32_t kNoValue = 0xffffffff;

// Specializes integer, float and boolean constants in-place in the given SPIR-V binary. The value
// at the given index corrosponds the specialization constants constantId. Note that while we can't
// specialize OpSpecConstantOp, we could specialize OpSpecConstantComposite, but we would need
// support for variable size spec-constant values.
void specializeConstants(uint32_t* spirv, size_t numBytes, const std::vector<uint32_t>& values);

} // namespace igl::vulkan::util
