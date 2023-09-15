/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace igl::shell {

class FileLoader {
 public:
  struct FileData {
    std::unique_ptr<uint8_t[]> data = nullptr;
    uint32_t length;
  };

  FileLoader() = default;
  virtual ~FileLoader() = default;
  virtual FileData loadBinaryData(const std::string& /* filename */) {
    return {};
  }
  virtual bool fileExists(const std::string& /* filename */) const {
    return false;
  }
  virtual std::string basePath() const {
    return ".";
  }
  virtual std::string fullPath(const std::string& /* filename */) const {
    return "";
  }

 protected:
  FileData loadBinaryDataInternal(const std::string& filePath);
};

} // namespace igl::shell
