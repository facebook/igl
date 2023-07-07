/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstring>

namespace igl {
namespace opengl {

///--------------------------------------
/// MARK: - optimizedMemcpy

// Optimized version of memcpy that allows to copy smallest unforms in most efficient way
// Other sizes utilize a libc memcpy implementation
// It's not a universal function and expects to have a proper alignment for data!
void optimizedMemcpy(void* dst, const void* src, size_t size);
} // namespace opengl
} // namespace igl
