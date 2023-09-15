/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/fileLoader/FileLoader.h>

namespace igl::shell {

class FileLoaderApple final : public FileLoader {
 public:
  FileLoaderApple() = default;
  ~FileLoaderApple() override = default;
  [[nodiscard]] FileData loadBinaryData(const std::string& fileName) override;
  [[nodiscard]] bool fileExists(const std::string& fileName) const override;
  [[nodiscard]] std::string basePath() const override;
  [[nodiscard]] std::string fullPath(const std::string& fileName) const override;
};

} // namespace igl::shell
