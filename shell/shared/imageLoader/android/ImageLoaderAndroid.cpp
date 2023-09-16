/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/imageLoader/android/ImageLoaderAndroid.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

namespace igl::shell {

ImageLoaderAndroid::ImageLoaderAndroid(FileLoader& fileLoader) : ImageLoader(fileLoader) {}

ImageData ImageLoaderAndroid::loadImageData(const std::string& imageName) noexcept {
  auto ret = ImageData();

  // Load file
  std::unique_ptr<uint8_t[]> buffer;
  AAsset* asset = AAssetManager_open(assetManager_, imageName.c_str(), AASSET_MODE_BUFFER);
  IGL_ASSERT(asset != nullptr);
  off64_t length = AAsset_getLength64(asset);
  if (IGL_UNEXPECTED(length > std::numeric_limits<int>::max())) {
    AAsset_close(asset);
    return {};
  }
  buffer = std::make_unique<uint8_t[]>(length);
  auto readSize = AAsset_read(asset, buffer.get(), length);
  if (readSize != length) {
    IGL_ASSERT_NOT_REACHED();
    return ret;
  }
  AAsset_close(asset);

  return loadImageDataFromMemory(buffer.get(), length);
}

} // namespace igl::shell
