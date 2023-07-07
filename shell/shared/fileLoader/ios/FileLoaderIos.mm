/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/fileLoader/ios/FileLoaderIos.h>

#import <Foundation/Foundation.h>
#include <fstream>
#include <string>

namespace igl::shell {

std::vector<uint8_t> FileLoaderIos::loadBinaryData(const std::string& fileName) {
  std::vector<uint8_t> data;
  if (fileName.empty()) {
    return data;
  }

  NSString* nsFileName = [NSString stringWithUTF8String:fileName.c_str()];
  if ([nsFileName length] == 0) {
    return data;
  }

  NSString* nsPath = [[NSBundle mainBundle] pathForResource:nsFileName ofType:nil];
  std::string path = std::string([nsPath UTF8String]);

  std::ifstream file(path, std::ios::binary);
  if ((file.rdstate() & std::ifstream::failbit) != 0) {
    return data;
  }
  file.unsetf(std::ios::skipws);

  std::streampos file_size;
  file.seekg(0, std::ios::end);
  file_size = file.tellg();
  file.seekg(0, std::ios::beg);

  data.reserve(file_size);
  data.insert(data.begin(), std::istream_iterator<uint8_t>(file), std::istream_iterator<uint8_t>());
  return (data);
}

bool FileLoaderIos::fileExists(const std::string& fileName) const {
  NSString* nsFileName = [NSString stringWithUTF8String:fileName.c_str()];
  if (nsFileName == nil || [nsFileName length] == 0) {
    return false;
  }
  NSString* nsPath = [[NSBundle mainBundle] pathForResource:nsFileName ofType:nil];
  if (nsPath == nil || [nsPath length] == 0) {
    return false;
  }
  return [[NSFileManager defaultManager] fileExistsAtPath:nsPath];
}

} // namespace igl::shell
