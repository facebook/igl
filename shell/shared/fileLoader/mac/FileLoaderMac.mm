/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/fileLoader/mac/FileLoaderMac.h>

#import <Foundation/Foundation.h>
#include <fstream>
#include <string>

// returns the NSString version of the full path of fileName within the main bundle
// it'll first attempt to find the bundle path using the full fileName
// if not found, it'll attempt to find the bundle path using just the root of the fileName
// a return value of nil indicates that the fileName wasn't found within the main bundle
static NSString* getBundleFilePath(const std::string& fileName) {
  NSString* nsFileName = [NSString stringWithUTF8String:fileName.c_str()];
  if (nsFileName == nil || [nsFileName length] == 0) {
    return nil;
  }

  // first search for the full file name, which may include a path
  NSString* nsPath = [[NSBundle mainBundle] pathForResource:nsFileName ofType:nil];
  if (nsPath != nil) {
    return nsPath;
  }

  // next search for the root file name without the path
  // since the bundle path may be different and flattened within the bundle
  NSString* nsRootFileName = [nsFileName lastPathComponent];
  nsPath = [[NSBundle mainBundle] pathForResource:nsRootFileName ofType:nil];
  if (nsPath != nil) {
    return nsPath;
  }

  for (NSBundle* bundle in [NSBundle allBundles]) {
    NSString* nsPath = [bundle pathForResource:nsFileName ofType:nil];
    if (nsPath != nil) {
      return nsPath;
    }

    // next search for the root file name without the path
    // since the bundle path may be different and flattened within the bundle
    NSString* nsRootFileName = [nsFileName lastPathComponent];
    nsPath = [[NSBundle mainBundle] pathForResource:nsRootFileName ofType:nil];
    if (nsPath != nil) {
      return nsPath;
    }
  }

  return nil;
}

namespace igl::shell {

std::vector<uint8_t> FileLoaderMac::loadBinaryData(const std::string& fileName) {
  std::vector<uint8_t> data;
  if (fileName.empty()) {
    return data;
  }

  const std::string path = fullPath(fileName);

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

bool FileLoaderMac::fileExists(const std::string& fileName) const {
  NSString* nsPath = getBundleFilePath(fileName);
  if (nsPath == nil) {
    return false;
  }
  return [[NSFileManager defaultManager] fileExistsAtPath:nsPath];
}

std::string FileLoaderMac::basePath() const {
  std::string basePath;
  const char* cstr = [[NSBundle mainBundle] resourcePath].UTF8String;
  if (cstr != nil) {
    basePath = std::string{cstr};
  }
  return basePath;
}

std::string FileLoaderMac::fullPath(const std::string& fileName) const {
  std::string fullPath;
  NSString* nsPath = getBundleFilePath(fileName);
  if (nsPath != nil) {
    fullPath = std::string([nsPath UTF8String]);
  }
  return fullPath;
}

} // namespace igl::shell
