/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <shell/shared/imageWriter/ImageWriter.h>

namespace igl::shell {

class ImageWriterIos final : public ImageWriter {
 public:
  ImageWriterIos() = default;
  ~ImageWriterIos() override = default;
  void writeImage(const std::string& /*imageAbsolutePath*/,
                  const ImageData& imageData,
                  bool flipY = true) const noexcept override;

 private:
};

} // namespace igl::shell
