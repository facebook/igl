/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/fileLoader/FileLoader.h>

#include <cstdio>
#include <limits>
#include <igl/Common.h>

#if IGL_PLATFORM_ANDROID
#include <shell/shared/fileLoader/android/FileLoaderAndroid.h>
#elif IGL_PLATFORM_LINUX
#include <shell/shared/fileLoader/linux/FileLoaderLinux.h>
#elif IGL_PLATFORM_MACOS || IGL_PLATFORM_IOS
#include <shell/shared/fileLoader/apple/FileLoaderApple.h>
#elif IGL_PLATFORM_WINDOWS
#include <shell/shared/fileLoader/win/FileLoaderWin.h>
#endif

#if defined(IGL_CMAKE_BUILD)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <boost/filesystem/operations.hpp>
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

std::unique_ptr<FileLoader> createFileLoader() {
#if IGL_PLATFORM_ANDROID
  return std::make_unique<FileLoaderAndroid>();
#elif IGL_PLATFORM_LINUX
  return std::make_unique<FileLoaderLinux>();
#elif IGL_PLATFORM_MACOS || IGL_PLATFORM_IOS
  return std::make_unique<FileLoaderApple>();
#elif IGL_PLATFORM_WINDOWS
  return std::make_unique<FileLoaderWin>();
#else
  return nullptr;
#endif
}

} // namespace igl::shell
