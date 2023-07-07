/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/fileLoader/FileLoader.h>

namespace igl::shell {

class FileLoaderWin final : public FileLoader {
 public:
  FileLoaderWin() = default;
  ~FileLoaderWin() override = default;
  std::vector<uint8_t> loadBinaryData(const std::string& fileName) override;
  bool fileExists(const std::string& fileName) const override;

 private:
};

} // namespace igl::shell
