/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/imageLoader/ImageLoader.h>
#include <shell/shared/platform/linux/PlatformLinux.h>

#include <shell/shared/fileLoader/linux/FileLoaderLinux.h>

namespace igl::shell {

PlatformLinux::PlatformLinux(std::shared_ptr<igl::IDevice> device) : device_(std::move(device)) {
  fileLoader_ = std::make_unique<igl::shell::FileLoaderLinux>();
  imageLoader_ = std::make_unique<igl::shell::ImageLoader>(*fileLoader_);
}

igl::IDevice& PlatformLinux::getDevice() noexcept {
  return *device_;
};

std::shared_ptr<igl::IDevice> PlatformLinux::getDevicePtr() const noexcept {
  return device_;
};

ImageLoader& PlatformLinux::getImageLoader() noexcept {
  return *imageLoader_;
};

const ImageWriter& PlatformLinux::getImageWriter() const noexcept {
  IGL_ASSERT_NOT_IMPLEMENTED();
  return *imageWriter_;
};

FileLoader& PlatformLinux::getFileLoader() const noexcept {
  return *fileLoader_;
}

} // namespace igl::shell
