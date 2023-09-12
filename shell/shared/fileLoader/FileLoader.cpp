/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/fileLoader/FileLoader.h>

#include <cstdio>
#include <igl/Common.h>

#if defined(IGL_CMAKE_BUILD)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#endif

namespace igl::shell {

std::vector<uint8_t> FileLoader::loadBinaryDataInternal(const std::string& fileName) {
  if (IGL_UNEXPECTED(!fs::exists(fileName))) {
    return {};
  }
  const uintmax_t length = fs::file_size(fileName);

  if (IGL_UNEXPECTED(length > std::numeric_limits<uint32_t>::max())) {
    return {};
  }

  std::FILE* file = std::fopen(fileName.c_str(), "rb");
  if (IGL_UNEXPECTED(file == nullptr)) {
    return {};
  }

  IGL_SCOPE_EXIT {
    std::fclose(file);
  };

  std::vector<uint8_t> data(static_cast<size_t>(length));
  if (std::fread(data.data(), 1, data.size(), file) != data.size()) {
    return {};
  }

  return data;
}

} // namespace igl::shell
