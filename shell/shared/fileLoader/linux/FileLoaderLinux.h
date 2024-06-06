/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/fileLoader/FileLoader.h>

namespace igl::shell {

class FileLoaderLinux final : public FileLoader {
 public:
  FileLoaderLinux();
  ~FileLoaderLinux() override = default;
  FileData loadBinaryData(const std::string& fileName) override;
  [[nodiscard]] bool fileExists(const std::string& fileName) const override;
  [[nodiscard]] std::string basePath() const override;
  [[nodiscard]] std::string fullPath(const std::string& fileName) const override;

 private:
  std::string basePath_;
};

} // namespace igl::shell
