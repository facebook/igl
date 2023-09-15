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

FileLoader::FileData FileLoaderAndroid::loadBinaryData(const std::string& fileName) {
  if (fileName.empty()) {
    IGL_LOG_ERROR("Error in loadBinaryData(): empty fileName\n");
    return {};
  }

  if (assetManager_ == nullptr) {
    IGL_LOG_ERROR("Error in loadBinaryData(): Asset manager is nullptr\n");
    return {};
  }

  // Load file
  AAsset* asset = AAssetManager_open(assetManager_, fileName.c_str(), AASSET_MODE_BUFFER);
  if (IGL_UNEXPECTED(asset == nullptr)) {
    IGL_LOG_ERROR("Error in loadBinaryData(): failed to open file %s\n", fileName.c_str());
    return {};
  }

  off64_t length = AAsset_getLength64(asset);
  if (IGL_UNEXPECTED(length > std::numeric_limits<uint32_t>::max())) {
    AAsset_close(asset);
    return {};
  }

  auto data = std::make_unique<uint8_t[]>(length);
  auto readSize = AAsset_read(asset, data.get(), length);
  if (IGL_UNEXPECTED(readSize != length)) {
    IGL_LOG_ERROR("Error in loadBinaryData(): read size mismatch (%ld != %zu) in %s\n",
                  readSize,
                  length,
                  fileName.c_str());
  }
  AAsset_close(asset);

  return {std::move(data), static_cast<uint32_t>(length)};
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
