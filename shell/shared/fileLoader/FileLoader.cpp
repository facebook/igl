/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/fileLoader/FileLoader.h>

#include <cstdio>
#include <filesystem>
#include <limits>
#include <system_error>
#include <igl/Common.h>

#if IGL_PLATFORM_ANDROID
#include <shell/shared/fileLoader/android/FileLoaderAndroid.h>
#elif IGL_PLATFORM_LINUX
#include <shell/shared/fileLoader/linux/FileLoaderLinux.h>
#elif IGL_PLATFORM_APPLE
#include <shell/shared/fileLoader/apple/FileLoaderApple.h>
#elif IGL_PLATFORM_WINDOWS
#include <shell/shared/fileLoader/win/FileLoaderWin.h>
#endif

namespace igl::shell {

FileLoader::FileData FileLoader::loadBinaryDataInternal(const std::string& filePath) {
  // Use non-throwing overloads to avoid std::terminate in noexcept contexts.
  // The throwing variants call std::filesystem::status() which can throw
  // filesystem_error on EINTR (interrupted system call) under signal pressure.
  std::error_code ec;
  const bool exists = std::filesystem::exists(filePath, ec);
  if (ec) {
    IGL_LOG_INFO_ONCE("FileLoader::loadBinaryDataInternal exists failed for %s: %d (%s)\n",
                      filePath.c_str(),
                      ec.value(),
                      ec.message().c_str());
    return {};
  }
  if (IGL_DEBUG_VERIFY_NOT(!exists, "Couldn't find file: %s", filePath.c_str())) {
    return {};
  }
  const uintmax_t length = std::filesystem::file_size(filePath, ec);
  if (ec) {
    IGL_LOG_INFO_ONCE("FileLoader::loadBinaryDataInternal file_size failed for %s: %d (%s)\n",
                      filePath.c_str(),
                      ec.value(),
                      ec.message().c_str());
    return {};
  }

  if (IGL_DEBUG_VERIFY_NOT(length > std::numeric_limits<uint64_t>::max())) {
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

  return {.data = std::move(data), .length = static_cast<uint64_t>(length)};
}

std::unique_ptr<FileLoader> createFileLoader() {
#if IGL_PLATFORM_ANDROID
  return std::make_unique<FileLoaderAndroid>();
#elif IGL_PLATFORM_LINUX
  return std::make_unique<FileLoaderLinux>();
#elif IGL_PLATFORM_APPLE
  return std::make_unique<FileLoaderApple>();
#elif IGL_PLATFORM_WINDOWS
  return std::make_unique<FileLoaderWin>();
#else
  return nullptr;
#endif
}

} // namespace igl::shell
