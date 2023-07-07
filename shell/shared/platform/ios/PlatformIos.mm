/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/platform/ios/PlatformIos.h>

#include <shell/shared/fileLoader/ios/FileLoaderIos.h>
#include <shell/shared/imageLoader/ios/ImageLoaderIos.h>
#include <shell/shared/imageWriter/ios/ImageWriterIos.h>

namespace igl::shell {

PlatformIos::PlatformIos(std::unique_ptr<igl::IDevice> device) : device_(std::move(device)) {
  imageLoader_ = std::make_unique<igl::shell::ImageLoaderIos>();
  imageWriter_ = std::make_unique<igl::shell::ImageWriterIos>();
  fileLoader_ = std::make_unique<igl::shell::FileLoaderIos>();
}

igl::IDevice& PlatformIos::getDevice() noexcept {
  return *device_;
};

std::shared_ptr<igl::IDevice> PlatformIos::getDevicePtr() const noexcept {
  return device_;
};

ImageLoader& PlatformIos::getImageLoader() noexcept {
  return *imageLoader_;
};

const ImageWriter& PlatformIos::getImageWriter() const noexcept {
  return *imageWriter_;
}

FileLoader& PlatformIos::getFileLoader() noexcept {
  return *fileLoader_;
};
} // namespace igl::shell
