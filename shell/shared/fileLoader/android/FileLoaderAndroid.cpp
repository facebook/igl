/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <igl/IGL.h>
#include <shell/shared/fileLoader/android/FileLoaderAndroid.h>

namespace igl::shell {

std::vector<uint8_t> FileLoaderAndroid::loadBinaryData(const std::string& fileName) {
  std::vector<uint8_t> data;
  if (fileName.empty()) {
    return data;
  }

  // Load file
  AAsset* asset = AAssetManager_open(assetManager_, fileName.c_str(), AASSET_MODE_BUFFER);
  IGL_ASSERT(asset != nullptr);
  data.resize(AAsset_getLength(asset));
  auto readSize = AAsset_read(asset, data.data(), data.size());
  if (readSize != data.size()) {
    IGL_ASSERT_NOT_REACHED();
  }

  return data;
}

bool FileLoaderAndroid::fileExists(const std::string& fileName) const {
  std::vector<uint8_t> data;
  if (fileName.empty()) {
    return false;
  }

  // Load file
  AAsset* asset = AAssetManager_open(assetManager_, fileName.c_str(), AASSET_MODE_BUFFER);
  return asset != nullptr;
}

} // namespace igl::shell
