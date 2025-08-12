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

class ImageWriterSTB final : public ImageWriter {
 public:
  void writeImage(const std::string& /*imageAbsolutePath*/,
                  const ImageData& imageData,
                  bool flipY = true) const noexcept override;

 private:
};

} // namespace igl::shell
