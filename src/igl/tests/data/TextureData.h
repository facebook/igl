/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <cstdint>

namespace igl::tests::data::texture {
// clang-format off

constexpr std::array<uint32_t, 4> kTexRgba2x2 = {0x11223344, 0x11111111,
                                                  0x22222222, 0x33333333};

constexpr std::array<uint32_t, 4> kTexRgba2x2Modified = {0x11223344, 0x11111111,
                                                          0x22222222, 0x44332211};

constexpr std::array<uint32_t, 4> kTexRgbaGray2x2 = {0x80808080, 0x80808080,
                                                      0x80808080, 0x80808080};

constexpr std::array<uint32_t, 4> kTexRgbaClear2x2 = {0, 0,
                                                       0, 0};


constexpr std::array<uint32_t, 16> kTexRgbaGray4x4 = {0x888888FF, 0x888888FF, 0x888888FF, 0x888888FF,
                                                       0x888888FF, 0x888888FF, 0x888888FF, 0x888888FF,
                                                       0x888888FF, 0x888888FF, 0x888888FF, 0x888888FF,
                                                       0x888888FF, 0x888888FF, 0x888888FF, 0x888888FF};

// NOLINTNEXTLINE(readability-identifier-naming)
constexpr std::array<uint32_t, 16> kTexRgbaRedAlpha128_4x4 = {0x80000080, 0x80000080, 0x80000080, 0x80000080,
                                                               0x80000080, 0x80000080, 0x80000080, 0x80000080,
                                                               0x80000080, 0x80000080, 0x80000080, 0x80000080,
                                                               0x80000080, 0x80000080, 0x80000080, 0x80000080};

// NOLINTNEXTLINE(readability-identifier-naming)
constexpr std::array<uint32_t, 16> kTexRgbaBlueAlpha127_4x4 = {0x00007F7F, 0x00007F7F, 0x00007F7F, 0x00007F7F,
                                                                0x00007F7F, 0x00007F7F, 0x00007F7F, 0x00007F7F,
                                                                0x00007F7F, 0x00007F7F, 0x00007F7F, 0x00007F7F,
                                                                0x00007F7F, 0x00007F7F, 0x00007F7F, 0x00007F7F};

// NOLINTNEXTLINE(readability-identifier-naming)
constexpr std::array<uint32_t, 16> kTexRgbaMisc1_4x4 = {0x00000000, 0x11111111, 0x22222222, 0x33333333,
                                                         0x44444444, 0x55555555, 0x66666666, 0x77777777,
                                                         0x88888888, 0x99999999, 0xAAAAAAAA, 0xBBBBBBBB,
                                                         0xCCCCCCCC, 0xDDDDDDDD, 0xEEEEEEEE, 0xFFFFFFFF};

constexpr std::array<uint32_t, 25> kTexRgbaGray5x5 = {0x80808080, 0x80808080, 0x80808080, 0x80808080, 0x80808080,
                                                       0x80808080, 0x80808080, 0x80808080, 0x80808080, 0x80808080,
                                                       0x80808080, 0x80808080, 0x80808080, 0x80808080, 0x80808080,
                                                       0x80808080, 0x80808080, 0x80808080, 0x80808080, 0x80808080,
                                                       0x80808080, 0x80808080, 0x80808080, 0x80808080, 0x80808080};

// clang-format on
} // namespace igl::tests::data::texture
