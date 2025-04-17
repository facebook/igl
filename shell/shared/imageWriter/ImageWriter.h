/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <string>
#include <igl/IGL.h>

#include <shell/shared/imageLoader/ImageLoader.h>

namespace igl::shell {

class ImageWriter {
 public:
  ImageWriter() = default;
  virtual ~ImageWriter() = default;
  virtual void writeImage(const std::string& imageName,
                          const ImageData& imageData) const noexcept = 0;

 private:
};

} // namespace igl::shell
