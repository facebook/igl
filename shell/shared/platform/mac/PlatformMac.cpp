/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/platform/mac/PlatformMac.h>

#include <shell/shared/fileLoader/mac/FileLoaderMac.h>
#include <shell/shared/imageLoader/mac/ImageLoaderMac.h>
#include <shell/shared/imageWriter/mac/ImageWriterMac.h>

namespace igl::shell {

PlatformMac::PlatformMac(std::shared_ptr<igl::IDevice> device) : device_(std::move(device)) {
  imageLoader_ = std::make_unique<igl::shell::ImageLoaderMac>();
  imageWriter_ = std::make_unique<igl::shell::ImageWriterMac>();
  fileLoader_ = std::make_unique<igl::shell::FileLoaderMac>();
}

igl::IDevice& PlatformMac::getDevice() noexcept {
  return *device_;
};

std::shared_ptr<igl::IDevice> PlatformMac::getDevicePtr() const noexcept {
  return device_;
};

ImageLoader& PlatformMac::getImageLoader() noexcept {
  return *imageLoader_;
};

const ImageWriter& PlatformMac::getImageWriter() const noexcept {
  return *imageWriter_;
};

FileLoader& PlatformMac::getFileLoader() noexcept {
  return *fileLoader_;
};
} // namespace igl::shell
