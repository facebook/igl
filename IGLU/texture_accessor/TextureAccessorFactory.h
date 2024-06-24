/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "ITextureAccessor.h"
#include <igl/CommandQueue.h>
#include <igl/IGL.h>
#include <igl/Texture.h>

namespace iglu::textureaccessor {

class TextureAccessorFactory {
 public:
  static std::unique_ptr<ITextureAccessor> createTextureAccessor(
      igl::BackendType backendType,
      const std::shared_ptr<igl::ITexture>& texture,
      igl::IDevice& device);
};

} // namespace iglu::textureaccessor
