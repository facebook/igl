/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/platform/android/PlatformAndroid.h>

#include <shell/shared/fileLoader/android/FileLoaderAndroid.h>
#include <shell/shared/imageLoader/android/ImageLoaderAndroid.h>
#include <shell/shared/imageWriter/ImageWriter.h>
#include <shell/shared/imageWriter/android/ImageWriterAndroid.h>

namespace igl::shell {

PlatformAndroid::PlatformAndroid(std::unique_ptr<igl::IDevice> device, bool useFakeLoader) :
  device_(std::move(device)) {
  fileLoader_ = std::make_unique<igl::shell::FileLoaderAndroid>();
  if (useFakeLoader) {
    imageLoader_ = std::make_unique<igl::shell::ImageLoader>(*fileLoader_);
  } else {
    imageLoader_ = std::make_unique<igl::shell::ImageLoaderAndroid>(*fileLoader_);
    imageWriter_ = std::make_unique<igl::shell::ImageWriterAndroid>();
  }
}

igl::IDevice& PlatformAndroid::getDevice() noexcept {
  return *device_;
};

std::shared_ptr<igl::IDevice> PlatformAndroid::getDevicePtr() const noexcept {
  return device_;
}

ImageLoader& PlatformAndroid::getImageLoader() noexcept {
  return *imageLoader_;
};

const ImageWriter& PlatformAndroid::getImageWriter() const noexcept {
  return *imageWriter_;
};

FileLoader& PlatformAndroid::getFileLoader() const noexcept {
  return *fileLoader_;
}

} // namespace igl::shell
