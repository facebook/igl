/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/fileLoader/linux/FileLoaderLinux.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <igl/Core.h>
// @fb-only
// @fb-only
// @fb-only

namespace {

std::string findSubdir(const char* subdir, const std::string& fileName) {
  // Use non-throwing overloads throughout to avoid std::terminate when a
  // signal (e.g. during test teardown) interrupts the underlying stat() call.
  std::error_code ec;
  std::filesystem::path dir = std::filesystem::current_path(ec);
  if (ec) {
    IGL_LOG_INFO_ONCE(
        "FileLoaderLinux::current_path failed: %d (%s)\n", ec.value(), ec.message().c_str());
    return {};
  }
  const auto root = dir.root_path();
  // find `subdir` somewhere above our current directory
  while (dir != root) {
    const std::filesystem::path subdirPath = dir / subdir;
    if (std::filesystem::exists(subdirPath, ec)) {
      break;
    }
    if (ec) {
      const std::string subdirPathString = subdirPath.string();
      IGL_LOG_INFO_ONCE("FileLoaderLinux::exists failed for %s: %d (%s)\n",
                        subdirPathString.c_str(),
                        ec.value(),
                        ec.message().c_str());
      ec.clear();
    }
    dir = dir.parent_path();
  }

  std::filesystem::path fullPath = (dir / subdir / fileName);
  if (std::filesystem::exists(fullPath, ec)) {
    return fullPath.string();
  }
  if (ec) {
    const std::string fullPathString = fullPath.string();
    IGL_LOG_INFO_ONCE("FileLoaderLinux::exists failed for %s: %d (%s)\n",
                      fullPathString.c_str(),
                      ec.value(),
                      ec.message().c_str());
    ec.clear();
  }

  dir = std::filesystem::current_path(ec);
  if (ec) {
    IGL_LOG_INFO_ONCE(
        "FileLoaderLinux::current_path failed: %d (%s)\n", ec.value(), ec.message().c_str());
    return {};
  }

  fullPath = (dir / "images/" / fileName);
  if (std::filesystem::exists(fullPath, ec)) {
    return fullPath.string();
  }
  if (ec) {
    const std::string fullPathString = fullPath.string();
    IGL_LOG_INFO_ONCE("FileLoaderLinux::exists failed for %s: %d (%s)\n",
                      fullPathString.c_str(),
                      ec.value(),
                      ec.message().c_str());
  }

  return {};
}

} // namespace

namespace igl::shell {

FileLoader::FileData FileLoaderLinux::loadBinaryData(const std::string& fileName) {
  return loadBinaryDataInternal(fullPath(fileName));
}

bool FileLoaderLinux::fileExists(const std::string& fileName) const {
  std::ifstream file(fileName, std::ios::binary);
  auto exists = (file.rdstate() & std::ifstream::failbit) == 0;
  file.close();
  return exists;
}

std::string FileLoaderLinux::basePath() const {
  // Return executable path:
  return basePath_;
}

std::string FileLoaderLinux::fullPath(const std::string& fileName) const {
  std::error_code ec;
  if (std::filesystem::exists(fileName, ec)) {
    return fileName;
  }
  if (ec) {
    IGL_LOG_INFO_ONCE("FileLoaderLinux::exists failed for %s: %d (%s)\n",
                      fileName.c_str(),
                      ec.value(),
                      ec.message().c_str());
    ec.clear();
  }

  auto fullPath = std::filesystem::path(basePath_) / fileName;
  if (std::filesystem::exists(fullPath, ec)) {
    return fullPath.string();
  }
  if (ec) {
    const std::string fullPathString = fullPath.string();
    IGL_LOG_INFO_ONCE("FileLoaderLinux::exists failed for %s: %d (%s)\n",
                      fullPathString.c_str(),
                      ec.value(),
                      ec.message().c_str());
    ec.clear();
  }

  fullPath = std::filesystem::path("shell/resources/images") / fileName;
  if (std::filesystem::exists(fullPath, ec)) {
    return fullPath.string();
  }
  if (ec) {
    const std::string fullPathString = fullPath.string();
    IGL_LOG_INFO_ONCE("FileLoaderLinux::exists failed for %s: %d (%s)\n",
                      fullPathString.c_str(),
                      ec.value(),
                      ec.message().c_str());
  }

  // @lint-ignore CLANGTIDY
  const char* folders[] = {
      "shell/resources/images/",
      "samples/resources/images/",
      "samples/resources/models/",
      "samples/resources/fonts/",
      "samples/resources/fonts/optimistic",
      // @fb-only
      // @fb-only
      // @fb-only
      // @fb-only
      // @fb-only
      // @fb-only
      // @fb-only
      // @fb-only
      // @fb-only
      // @fb-only
      // @fb-only
      // @fb-only
      // @fb-only
      // @fb-only
  };

  // find folders somewhere above our current directory
  for (const char* folder : folders) {
    if (std::string p = findSubdir(folder, fileName); !p.empty()) {
      return p;
    }
  }

// @fb-only
  // clang-format off
  // @fb-only
  // @fb-only
  // @fb-only
                                     // @fb-only
                                     // @fb-only
                                     // @fb-only
                                     // @fb-only
                                     // @fb-only
                                     // @fb-only
                                     // @fb-only
                                     // @fb-only
                                     // @fb-only
                                     // @fb-only
                                     // @fb-only
                                     // @fb-only
  // clang-format on
  // @fb-only
    // @fb-only
    // @fb-only
      // @fb-only
          // @fb-only
      // @fb-only
        // @fb-only
      // @fb-only
    // @fb-only
  // @fb-only
// @fb-only

  IGL_DEBUG_ASSERT_NOT_REACHED();
  return "";
}

} // namespace igl::shell
