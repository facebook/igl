/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <android/asset_manager.h>
#include <filesystem>
#include <igl/IGL.h>
#include <shell/shared/fileLoader/android/FileLoaderAndroid.h>

namespace igl::shell {

FileLoader::FileData FileLoaderAndroid::loadBinaryData(const std::string& fileName) {
  if (fileName.empty()) {
    IGL_LOG_ERROR("Error in loadBinaryData(): empty fileName\n");
    return {};
  }

  if (fileExists(fileName)) {
    if (assetManager_) {
      // Load file
      AAsset* asset = AAssetManager_open(assetManager_, fileName.c_str(), AASSET_MODE_BUFFER);
      if (asset != nullptr) {
        const off64_t length = AAsset_getLength64(asset);
        if (IGL_DEBUG_VERIFY_NOT(length > std::numeric_limits<uint32_t>::max())) {
          AAsset_close(asset);
          return {};
        }

        auto data = std::make_unique<uint8_t[]>(length);
        auto readSize = AAsset_read(asset, data.get(), length);
        if (IGL_DEBUG_VERIFY_NOT(readSize != length)) {
          IGL_LOG_ERROR("Error in loadBinaryData(): read size mismatch (%ld != %zu) in %s\n",
                        readSize,
                        length,
                        fileName.c_str());
        }
        AAsset_close(asset);

        return {std::move(data), static_cast<uint32_t>(length)};
      }
    }
    // Fallback to default behavior (i.e., loading w/ C++ functions) if asset manager is not set or
    // doesnt not contain the file.
    return loadBinaryDataInternal(fullPath(fileName));
  }

  IGL_LOG_ERROR("Error in loadBinaryData(): file not found in %s\n", fileName.c_str());
  return {};
}

bool FileLoaderAndroid::fileExists(const std::string& fileName) const {
  const std::vector<uint8_t> data;
  if (fileName.empty()) {
    IGL_LOG_ERROR("Error in fileExists(): Empty fileName\n");
    return false;
  }

  if (!fullPath(fileName).empty()) {
    return true;
  }
  return false;
}

std::string FileLoaderAndroid::basePath() const {
  std::string basePath;
  if (assetManager_) {
    AAssetDir* assetDir = AAssetManager_openDir(assetManager_, "");
    if (assetDir != nullptr) {
      const char* fileName = AAssetDir_getNextFileName(assetDir);
      if (fileName != nullptr) {
        const std::filesystem::path filePath(fileName);
        basePath = filePath.root_path();
      }
      AAssetDir_close(assetDir);
    }
  }
  if (basePath.empty()) {
    basePath = std::filesystem::current_path().string();
  }
  return basePath;
}

std::string FileLoaderAndroid::fullPath(const std::string& fileName) const {
  if (assetManager_) {
    // Load file
    AAsset* asset = AAssetManager_open(assetManager_, fileName.c_str(), AASSET_MODE_BUFFER);
    if (asset != nullptr) {
      AAsset_close(asset);
      return fileName;
    }
  }
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
  return "";
}

} // namespace igl::shell
