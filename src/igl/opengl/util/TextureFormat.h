/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <igl/TextureFormat.h>

namespace igl::opengl::util {

/// Converts an OpenGL texture format to an IGL TextureFormat.
/// @param glInternalFormat The OpenGL internal format. Always required.
/// @param glFormat The type of the external OpenGL format. Only required to differentiate between
/// generic (unsized) internal formats.
/// @param glType The data type of the external OpenGL format. Only required to differentiate
/// between generic (unsized) internal formats.
/// @return The corresponding IGL format if known; otherwise returns TextureFormat::Invalid.
TextureFormat glTextureFormatToTextureFormat(int32_t glInternalFormat,
                                             uint32_t glFormat = 0,
                                             uint32_t glType = 0);

} // namespace igl::opengl::util
