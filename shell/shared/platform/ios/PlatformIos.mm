/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/platform/ios/PlatformIos.h>

#include <shell/shared/fileLoader/apple/FileLoaderApple.h>
#include <shell/shared/imageLoader/ImageLoader.h>
#include <shell/shared/imageWriter/ios/ImageWriterIos.h>

namespace igl::shell {

PlatformIos::PlatformIos(std::shared_ptr<IDevice> device) : device_(std::move(device)) {
  fileLoader_ = std::make_unique<FileLoaderApple>();
  imageLoader_ = std::make_unique<ImageLoader>(*fileLoader_);
  imageWriter_ = std::make_unique<ImageWriterIos>();
}

IDevice& PlatformIos::getDevice() noexcept {
  return *device_;
}

std::shared_ptr<IDevice> PlatformIos::getDevicePtr() const noexcept {
  return device_;
}

ImageLoader& PlatformIos::getImageLoader() noexcept {
  return *imageLoader_;
}

const ImageWriter& PlatformIos::getImageWriter() const noexcept {
  return *imageWriter_;
}

FileLoader& PlatformIos::getFileLoader() const noexcept {
  return *fileLoader_;
}

} // namespace igl::shell
