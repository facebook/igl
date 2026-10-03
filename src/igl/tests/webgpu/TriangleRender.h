/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <vector>
#include <igl/Device.h>

namespace igl::tests::webgpu {

/// Renders the reference triangle (red, green and blue corners on a fixed clear color) into a
/// `size` x `size` RGBA8 target and reads it back with rows top to bottom. Shared by the native
/// test and the wasm sample, so their frames can be compared byte for byte.
[[nodiscard]] Result renderTriangle(IDevice& device, uint32_t size, std::vector<uint8_t>& outRgba);

} // namespace igl::tests::webgpu
