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
  FileLoaderLinux() = default;
  ~FileLoaderLinux() override = default;

  FileData loadBinaryData(const std::string& fileName) override;
  [[nodiscard]] bool fileExists(const std::string& fileName) const override;
  [[nodiscard]] std::string basePath() const override;
  [[nodiscard]] std::string fullPath(const std::string& fileName) const override;

 private:
  /// The path `fileName` resolves to, searching the same places as `fullPath()`, or an empty
  /// string when it is in none of them.
  [[nodiscard]] std::string findFile(const std::string& fileName) const;

  std::string basePath_;
};

} // namespace igl::shell
