/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint> // For uint16_t

namespace igl::tests::data::vertex_index {
// clang-format off

// Using float 4 to alleviate packing issues
static float QUAD_VERT[] =
  {-1.0f,  1.0f, 0.0f, 1.0f,
    1.0f,  1.0f, 0.0f, 1.0f,
   -1.0f, -1.0f, 0.0f, 1.0f,
    1.0f, -1.0f, 0.0f, 1.0f};

static float QUAD_UV[] = {
    0.0, 1.0,
    1.0, 1.0,
    0.0, 0.0,
    1.0, 0.0,
};

static uint16_t QUAD_IND[] = {
    0,
    1,
    2,
    1,
    3,
    2,
};

// clang-format on
} // namespace igl::tests::data::vertex_index
