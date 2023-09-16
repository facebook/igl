/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/imageLoader/mac/ImageLoaderMac.h>

#include <shell/shared/fileLoader/FileLoader.h>

namespace igl::shell {

ImageLoaderMac::ImageLoaderMac(FileLoader& fileLoader) : ImageLoader(fileLoader) {}

ImageData ImageLoaderMac::loadImageData(std::string imageName) noexcept {
  auto fullPath = fileLoader().fullPath(imageName);
  return loadImageDataFromFile(fullPath);
}

} // namespace igl::shell
