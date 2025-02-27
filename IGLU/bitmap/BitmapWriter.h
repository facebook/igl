/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <igl/IGL.h>

namespace igl::iglu {

// Check if a texture format is supported by the bitmap writer
bool isSupportedBitmapTextureFormat(TextureFormat format);

// Write the contents of a texture to a bitmap file
void writeBitmap(std::ostream& stream,
                 std::shared_ptr<ITexture> texture,
                 IDevice& device,
                 bool flipY = false);

void writeBitmap(std::ostream& stream, const uint8_t* imageData, uint32_t width, uint32_t height);

} // namespace igl::iglu
