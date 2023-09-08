/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <filesystem>
#include <igl/IGL.h>
#include <shell/shared/fileLoader/android/FileLoaderAndroid.h>

namespace igl::shell {

std::vector<uint8_t> FileLoaderAndroid::loadBinaryData(const std::string& fileName) {
  std::vector<uint8_t> data;
  if (fileName.empty()) {
    IGL_LOG_ERROR("Error in loadBinaryData(): empty fileName\n");
    return data;
  }

  if (assetManager_ == nullptr) {
    IGL_LOG_ERROR("Error in loadBinaryData(): Asset manager is nullptr\n");
    return data;
  }

  // Load file
  AAsset* asset = AAssetManager_open(assetManager_, fileName.c_str(), AASSET_MODE_BUFFER);
  IGL_ASSERT(asset != nullptr);
  if (asset == nullptr) {
    IGL_LOG_ERROR("Error in loadBinaryData(): failed to open file %s\n", fileName.c_str());
    return data;
  }

  data.resize(AAsset_getLength(asset));
  auto readSize = AAsset_read(asset, data.data(), data.size());
  if (readSize != data.size()) {
    IGL_LOG_ERROR("Error in loadBinaryData(): read size mismatch (%ld != %ld) in %s\n",
                  readSize,
                  data.size(),
                  fileName.c_str());
    IGL_ASSERT_NOT_REACHED();
  }
  AAsset_close(asset);

  return data;
}

bool FileLoaderAndroid::fileExists(const std::string& fileName) const {
  std::vector<uint8_t> data;
  if (fileName.empty()) {
    IGL_LOG_ERROR("Error in fileExists(): Empty fileName\n");
    return false;
  }

  // Load file
  AAsset* asset = AAssetManager_open(assetManager_, fileName.c_str(), AASSET_MODE_BUFFER);
  if (asset != nullptr) {
    AAsset_close(asset);
    return true;
  }
  return false;
}

std::string FileLoaderAndroid::basePath() const {
  std::string basePath;
  AAssetDir* assetDir = AAssetManager_openDir(assetManager_, "");
  if (assetDir != nullptr) {
    const char* fileName = AAssetDir_getNextFileName(assetDir);
    std::filesystem::path filePath(fileName);
    basePath = filePath.root_path();
    AAssetDir_close(assetDir);
  }
  return basePath;
}

std::string FileLoaderAndroid::fullPath(const std::string& fileName) const {
  return fileName;
}

} // namespace igl::shell
