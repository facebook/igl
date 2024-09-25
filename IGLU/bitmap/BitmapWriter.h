/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>

namespace igl::iglu {

void writeBitmap(const char* filename, const uint8_t* imageData, uint32_t width, uint32_t height);

} // namespace igl::iglu
