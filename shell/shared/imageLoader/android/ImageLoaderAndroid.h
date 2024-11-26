/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#ifdef __ANDROID__

#include <android/log.h>
#include <shell/shared/imageLoader/ImageLoader.h>
#include <string>

struct AAssetManager;

namespace igl::shell {

class ImageLoaderAndroid final : public ImageLoader {
 public:
  ImageLoaderAndroid(FileLoader& fileLoader);
  ~ImageLoaderAndroid() override = default;
  ImageData loadImageData(const std::string& imageName,
                          std::optional<igl::TextureFormat> preferredFormat = {}) noexcept override;
  void setAssetManager(AAssetManager* mgr) {
    assetManager_ = mgr;
  }
  [[nodiscard]] AAssetManager* getAssetManager() const noexcept {
    return assetManager_;
  }

 private:
  AAssetManager* assetManager_{};
};

} // namespace igl::shell
#endif
