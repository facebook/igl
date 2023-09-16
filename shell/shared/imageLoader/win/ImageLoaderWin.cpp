/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/imageLoader/win/ImageLoaderWin.h>

#include <shell/shared/fileLoader/FileLoader.h>

namespace igl::shell {

ImageLoaderWin::ImageLoaderWin(FileLoader& fileLoader) : ImageLoader(fileLoader) {}

ImageData ImageLoaderWin::loadImageData(std::string imageName) noexcept {
  std::string fullName = fileLoader().fullPath(imageName);

  return loadImageDataFromFile(fullName);
}

} // namespace igl::shell
