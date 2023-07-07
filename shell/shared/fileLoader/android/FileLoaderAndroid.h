/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <android/log.h>
#include <shell/shared/fileLoader/FileLoader.h>
#include <string>

class AAssetManager;

namespace igl::shell {

class FileLoaderAndroid final : public FileLoader {
 public:
  FileLoaderAndroid() = default;
  ~FileLoaderAndroid() override = default;
  std::vector<uint8_t> loadBinaryData(const std::string& fileName) override;
  bool fileExists(const std::string& fileName) const override;

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
