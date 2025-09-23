/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <cstdint> // For uint16_t

namespace igl::tests::data::vertex_index {
// clang-format off

// Using float 4 to alleviate packing issues
constexpr std::array<float, 16> kQuadVert = {
  -1.0f,  1.0f, 0.0f, 1.0f,
   1.0f,  1.0f, 0.0f, 1.0f,
  -1.0f, -1.0f, 0.0f, 1.0f,
   1.0f, -1.0f, 0.0f, 1.0f};

constexpr std::array<float, 8> kQuadUv = {
    0.0, 1.0,
    1.0, 1.0,
    0.0, 0.0,
    1.0, 0.0,
};

constexpr std::array<uint16_t, 6> kQuadInd = {
    0,
    1,
    2,
    1,
    3,
    2,
};

// clang-format on
} // namespace igl::tests::data::vertex_index
