/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/fileLoader/apple/FileLoaderApple.h>

#import <Foundation/Foundation.h>
#include <string>

namespace igl::shell {
namespace {
// returns the NSString version of the full path of fileName within any bundle
// it'll first attempt to find the bundle path using the full fileName
// if not found, it'll attempt to find the bundle path using just the root of the fileName
// a return value of nil indicates that the fileName wasn't found within ny bundle
NSString* getBundleFilePath(const std::string& fileName) {
  // NOLINTNEXTLINE(misc-const-correctness)
  NSString* nsFileName = [NSString stringWithUTF8String:fileName.c_str()];
  if (nsFileName == nil || [nsFileName length] == 0) {
    return nil;
  }

  if ([[NSFileManager defaultManager] fileExistsAtPath:nsFileName]) {
    return nsFileName;
  }

  // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
  for (NSBundle* bundle in [NSBundle allBundles]) {
    NSString* nsPath = [bundle pathForResource:nsFileName ofType:nil];
    if (nsPath != nil) {
      return nsPath;
    }

    // next search for the root file name without the path
    // since the bundle path may be different and flattened within the bundle
    // NOLINTNEXTLINE(misc-const-correctness)
    NSString* nsRootFileName = [nsFileName lastPathComponent];
    nsPath = [[NSBundle mainBundle] pathForResource:nsRootFileName ofType:nil];
    if (nsPath != nil) {
      return nsPath;
    }
  }

  return nil;
}
} // namespace

FileLoader::FileData FileLoaderApple::loadBinaryData(const std::string& fileName) {
  return loadBinaryDataInternal(fullPath(fileName));
}

bool FileLoaderApple::fileExists(const std::string& fileName) const {
  // NOLINTNEXTLINE(misc-const-correctness)
  NSString* nsPath = getBundleFilePath(fileName);
  if (nsPath == nil) {
    return false;
  }
  return [[NSFileManager defaultManager] fileExistsAtPath:nsPath];
}

std::string FileLoaderApple::basePath() const {
  std::string basePath;
  const char* cstr = [[NSBundle mainBundle] resourcePath].UTF8String;
  if (cstr != nil) {
    basePath = std::string{cstr};
  }
  return basePath;
}

std::string FileLoaderApple::fullPath(const std::string& fileName) const {
  // NOLINTNEXTLINE(misc-const-correctness)
  const NSString* nsPath = getBundleFilePath(fileName);
  if (nsPath != nil) {
    return {[nsPath UTF8String]};
  }

  return {};
}

} // namespace igl::shell
