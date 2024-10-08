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

FileLoader::FileData FileLoader::loadBinaryDataInternal(const std::string& filePath) {
  if (IGL_DEBUG_VERIFY_NOT(!fs::exists(filePath), "Couldn't find file: %s", filePath.c_str())) {
    return {};
  }
  const uintmax_t length = fs::file_size(filePath);

  if (IGL_DEBUG_VERIFY_NOT(length > std::numeric_limits<uint32_t>::max())) {
    return {};
  }

  std::FILE* file = std::fopen(filePath.c_str(), "rb");
  if (IGL_DEBUG_VERIFY_NOT(file == nullptr)) {
    return {};
  }

  IGL_SCOPE_EXIT {
    std::fclose(file);
  };

  auto data = std::make_unique<uint8_t[]>(static_cast<size_t>(length));
  if (std::fread(data.get(), 1, length, file) != length) {
    return {};
  }

  return {std::move(data), static_cast<uint32_t>(length)};
}

} // namespace igl::shell
