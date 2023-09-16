/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/imageLoader/ImageLoader.h>
#include <string>

namespace igl::shell {

class ImageLoaderWin final : public ImageLoader {
 public:
  ImageLoaderWin(FileLoader& fileLoader);
  ~ImageLoaderWin() override = default;
  ImageData loadImageData(std::string imageName) noexcept override;
};

} // namespace igl::shell
