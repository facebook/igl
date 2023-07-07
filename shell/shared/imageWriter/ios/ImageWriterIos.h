/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/imageWriter/ImageWriter.h>
#include <string>

namespace igl::shell {

class ImageWriterIos final : public ImageWriter {
 public:
  ImageWriterIos() = default;
  ~ImageWriterIos() override = default;
  void writeImage(const std::string& /*imageAbsolutePath*/,
                  const ImageData& imageData) const noexcept override;

 private:
};

} // namespace igl::shell
