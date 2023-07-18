/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/fileLoader/win/FileLoaderWin.h>

#include <fstream>
#include <iterator>
#include <string>

namespace igl::shell {

std::vector<uint8_t> FileLoaderWin::loadBinaryData(const std::string& fileName) {
  std::vector<uint8_t> data;
  std::ifstream file(fileName, std::ios::binary);
  if ((file.rdstate() & std::ifstream::failbit) != 0) {
    file.close();
    return data;
  }
  file.unsetf(std::ios::skipws);

  std::streampos file_size;
  file.seekg(0, std::ios::end);
  file_size = file.tellg();
  file.seekg(0, std::ios::beg);

  data.reserve(file_size);
  data.insert(data.begin(), std::istream_iterator<uint8_t>(file), std::istream_iterator<uint8_t>());
  file.close();

  return (data);
}

bool FileLoaderWin::fileExists(const std::string& fileName) const {
  std::ifstream file(fileName, std::ios::binary);
  auto exists = (file.rdstate() & std::ifstream::failbit) == 0;
  file.close();
  return exists;
}

std::string FileLoaderWin::basePath() const {
  // passthrough, since there are no bundles on Windows
  return FileLoader::basePath();
}

std::string FileLoaderWin::fullPath(const std::string& fileName) const {
  // passthrough, since there are no bundles on Windows
  return fileName;
}

} // namespace igl::shell
