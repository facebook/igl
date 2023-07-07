/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/opengl/TextureBufferBase.h>

namespace igl {
namespace opengl {

// TextureBufferExternal encapsulates OpenGL textures without the guarantee of the lifecycle
// Specifically, this class does not delete the GL texture it encapsulates on destruction
class TextureBufferExternal : public TextureBufferBase {
  using Super = TextureBufferBase;
  friend class PlatformDevice; // So that PlatformDevice can do setTextureBufferProperties

 public:
  explicit TextureBufferExternal(IContext& context, TextureFormat format) :
    Super(context, format) {}
  ~TextureBufferExternal() override = default;

  // ITexture overrides
  Result upload(const TextureRangeDesc& range,
                const void* data,
                size_t bytesPerRow) const override {
    // no-op, texture is not managed by igl
    return Result();
  }
  Result uploadCube(const TextureRangeDesc& range,
                    TextureCubeFace face,
                    const void* data,
                    size_t bytesPerRow) const override {
    // no-op, texture is not managed by igl
    return Result();
  }
};

} // namespace opengl
} // namespace igl
