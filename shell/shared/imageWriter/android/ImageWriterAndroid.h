/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <android/log.h>
#include <shell/shared/imageWriter/ImageWriter.h>
#include <string>

struct AAssetManager;

namespace igl::shell {

class ImageWriterAndroid final : public ImageWriter {
 public:
  ImageWriterAndroid() = default;
  ~ImageWriterAndroid() override = default;

  void writeImage(const std::string& /*imageAbsolutePath*/,
                  const ImageData& imageData) const noexcept override;
  void setAssetManager(AAssetManager* mgr) {
    assetManager_ = mgr;
  }
  AAssetManager* getAssetManager() const noexcept {
    return assetManager_;
  }

 private:
  AAssetManager* assetManager_;
};

} // namespace igl::shell
