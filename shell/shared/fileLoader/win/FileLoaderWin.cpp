/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/fileLoader/win/FileLoaderWin.h>

#include <filesystem>
#include <fstream>
#include <igl/Common.h>
#include <iterator>
#include <string>

#if IGL_PLATFORM_WIN
#include <windows.h>
#endif

namespace igl::shell {

FileLoaderWin::FileLoaderWin() {
#if IGL_PLATFORM_WIN
  wchar_t path[MAX_PATH] = {0};
  if (IGL_VERIFY(GetModuleFileNameW(NULL, path, MAX_PATH) != 0)) {
    basePath_ = std::filesystem::path(path).parent_path().string();
  }
#endif
}

std::vector<uint8_t> FileLoaderWin::loadBinaryData(const std::string& fileName) {
  return loadBinaryDataInternal(fullPath(fileName));
}

bool FileLoaderWin::fileExists(const std::string& fileName) const {
  std::ifstream file(fileName, std::ios::binary);
  auto exists = (file.rdstate() & std::ifstream::failbit) == 0;
  file.close();
  return exists;
}

std::string FileLoaderWin::basePath() const {
  // Return executable path:
  return basePath_;
}

std::string FileLoaderWin::fullPath(const std::string& fileName) const {
  if (std::filesystem::exists(fileName)) {
    return fileName;
  }

  auto fullPath = std::filesystem::path(basePath_) / fileName;
  if (std::filesystem::exists(fullPath)) {
    return fullPath.string();
  }

  fullPath = std::filesystem::path("shell/resources/images") / fileName;
  if (std::filesystem::exists(fullPath)) {
    return fullPath.string();
  }

  std::filesystem::path dir = std::filesystem::current_path();
  // find `shell/resources/images/` somewhere above our current directory
  std::filesystem::path subdir("shell/resources/images/");
  while (dir != std::filesystem::current_path().root_path() &&
         !std::filesystem::exists(dir / subdir)) {
    dir = dir.parent_path();
  }

  fullPath = (dir / subdir / fileName);
  if (std::filesystem::exists(fullPath)) {
    return fullPath.string();
  }

  dir = std::filesystem::current_path();
  subdir = "images/";
  fullPath = (dir / subdir / fileName);
  if (std::filesystem::exists(fullPath)) {
    return fullPath.string();
  }

  IGL_ASSERT_NOT_REACHED();
  return "";
}

} // namespace igl::shell
