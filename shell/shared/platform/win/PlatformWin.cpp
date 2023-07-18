/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/platform/win/PlatformWin.h>

#include <shell/shared/fileLoader/win/FileLoaderWin.h>
#include <shell/shared/imageLoader/win/ImageLoaderWin.h>
#include <shell/shared/imageWriter/win/ImageWriterWin.h>

namespace igl::shell {

PlatformWin::PlatformWin(std::shared_ptr<igl::IDevice> device) : device_(std::move(device)) {
  imageLoader_ = std::make_unique<igl::shell::ImageLoaderWin>();
  imageWriter_ = std::make_unique<igl::shell::ImageWriterWin>();
  fileLoader_ = std::make_unique<igl::shell::FileLoaderWin>();
}

igl::IDevice& PlatformWin::getDevice() noexcept {
  return *device_;
};

std::shared_ptr<igl::IDevice> PlatformWin::getDevicePtr() const noexcept {
  return device_;
};

ImageLoader& PlatformWin::getImageLoader() noexcept {
  return *imageLoader_;
};

const ImageWriter& PlatformWin::getImageWriter() const noexcept {
  return *imageWriter_;
};

FileLoader& PlatformWin::getFileLoader() const noexcept {
  return *fileLoader_;
}

std::shared_ptr<FileLoader> PlatformWin::getFileLoaderPtr() const noexcept {
  return fileLoader_;
}

} // namespace igl::shell
