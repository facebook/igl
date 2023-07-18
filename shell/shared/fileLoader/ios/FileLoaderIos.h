/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/fileLoader/FileLoader.h>

namespace igl::shell {

class FileLoaderIos final : public FileLoader {
 public:
  FileLoaderIos() = default;
  ~FileLoaderIos() override = default;
  std::vector<uint8_t> loadBinaryData(const std::string& fileName) override;
  bool fileExists(const std::string& fileName) const override;
  std::string basePath() const override;
  std::string fullPath(const std::string& fileName) const override;

 private:
};

} // namespace igl::shell
