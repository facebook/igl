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

  // @fb-only
    auto path = std::filesystem::path("/data/local/tmp"); // Path for boltbench assets
    return loadBinaryDataInternal(path / fileName);
    // @fb-only
  // @fb-only

  if (assetManager_ == nullptr) {
    IGL_LOG_INFO("Asset manager not set!\n");
    // Fallback to default behavior (i.e., loading w/ C++ functions) when asset manager is not set
    // as this is the case for some unit tests.
    return loadBinaryDataInternal(fileName);
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

  if (assetManager_) {
    // Load file
    AAsset* asset = AAssetManager_open(assetManager_, fileName.c_str(), AASSET_MODE_BUFFER);
    if (asset != nullptr) {
      AAsset_close(asset);
      return true;
    }
    return false;
  } else {
    return std::filesystem::exists(fileName);
  }
}

std::string FileLoaderAndroid::basePath() const {
  std::string basePath;
  if (assetManager_) {
    AAssetDir* assetDir = AAssetManager_openDir(assetManager_, "");
    if (assetDir != nullptr) {
      const char* fileName = AAssetDir_getNextFileName(assetDir);
      std::filesystem::path filePath(fileName);
      basePath = filePath.root_path();
      AAssetDir_close(assetDir);
    }
  } else {
    basePath = std::filesystem::current_path().string();
  }
  return basePath;
}

std::string FileLoaderAndroid::fullPath(const std::string& fileName) const {
  if (assetManager_) {
    return fileName;
  } else {
    if (std::filesystem::exists(fileName)) {
      return fileName;
    }

    auto fullPath = std::filesystem::path("/data/local/tmp") / fileName;
    if (std::filesystem::exists(fullPath)) {
      return fullPath.string();
    }

    fullPath = std::filesystem::temp_directory_path() / fileName;
    if (std::filesystem::exists(fullPath)) {
      return fullPath.string();
    }

    IGL_ASSERT_NOT_REACHED();
    return "";
  }
}

} // namespace igl::shell
