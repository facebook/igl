/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/imageLoader/android/ImageLoaderAndroid.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <stb/stb_image.h>

namespace igl::shell {

ImageData ImageLoaderAndroid::loadImageData(std::string imageName) noexcept {
  auto ret = ImageData();

  // Load file
  std::vector<uint8_t> buffer;
  AAsset* asset = AAssetManager_open(assetManager_, imageName.c_str(), AASSET_MODE_BUFFER);
  IGL_ASSERT(asset != nullptr);
  buffer.resize(AAsset_getLength(asset));
  auto readSize = AAsset_read(asset, buffer.data(), buffer.size());
  if (readSize != buffer.size()) {
    IGL_ASSERT_NOT_REACHED();
    return ret;
  }

  // Load image from file in RGBA format
  int width, height;
  unsigned char* data = stbi_load_from_memory(buffer.data(), buffer.size(), &width, &height, 0, 4);
  if (!data) {
    IGL_ASSERT_NOT_REACHED();
    return ret;
  }

  // Copy results into return value
  ret.width = width;
  ret.height = height;
  ret.bitsPerComponent = 8;
  ret.bytesPerRow = (ret.bitsPerComponent * 4 / 8) * ret.width;
  ret.buffer.resize(ret.bytesPerRow * ret.height);
  memcpy(ret.buffer.data(), data, ret.bytesPerRow * ret.height);

  free(data);
  return ret;
}

} // namespace igl::shell
