/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/fileLoader/FileLoader.h>
#include <shell/shared/imageLoader/ImageLoader.h>
#include <shell/shared/imageWriter/ImageWriter.h>
#include <shell/shared/platform/Platform.h>

namespace igl::shell {

class PlatformIos : public Platform {
 public:
  ~PlatformIos() override = default;
  explicit PlatformIos(std::shared_ptr<igl::IDevice> device);
  igl::IDevice& getDevice() noexcept override;
  [[nodiscard]] std::shared_ptr<igl::IDevice> getDevicePtr() const noexcept override;
  ImageLoader& getImageLoader() noexcept override;
  [[nodiscard]] const ImageWriter& getImageWriter() const noexcept override;
  [[nodiscard]] FileLoader& getFileLoader() const noexcept override;

 private:
  std::shared_ptr<igl::IDevice> device_;
  std::shared_ptr<FileLoader> fileLoader_;
  std::shared_ptr<ImageLoader> imageLoader_;
  std::shared_ptr<ImageWriter> imageWriter_;
};

} // namespace igl::shell
