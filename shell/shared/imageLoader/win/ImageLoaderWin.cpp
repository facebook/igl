/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/imageLoader/win/ImageLoaderWin.h>

#include <cstring>
#include <filesystem>
#include <igl/Common.h>
#include <shell/shared/fileLoader/FileLoader.h>
#include <stb_image.h>
#include <stdint.h>

#if IGL_PLATFORM_WIN
#include <windows.h>
#endif

namespace igl::shell {

ImageLoaderWin::ImageLoaderWin(FileLoader& fileLoader) : ImageLoader(fileLoader) {
#if IGL_PLATFORM_WIN
  wchar_t path[MAX_PATH] = {0};
  if (IGL_VERIFY(GetModuleFileNameW(NULL, path, MAX_PATH) != 0)) {
    executablePath_ = std::filesystem::path(path).parent_path().string();
  }
#endif
}

ImageData ImageLoaderWin::loadImageData(std::string imageName) noexcept {
  std::string fullName = fileLoader().fullPath(imageName);

  // Load image from file in RGBA format
  auto ret = ImageData();
  int width, height;
  unsigned char* data = stbi_load(fullName.c_str(), &width, &height, 0, 4);
  if (!data) {
    IGL_ASSERT_MSG(data, "Could not find image file: %s", fullName.c_str());
    return ret;
  }

  // Copy results into return value
  ret.width = width;
  ret.height = height;
  ret.bitsPerComponent = 8;
  ret.bytesPerRow = (ret.bitsPerComponent * 4 / 8) * ret.width;
  ret.buffer.resize(ret.bytesPerRow * ret.height);
  memcpy(ret.buffer.data(), data, ret.bytesPerRow * ret.height);

  stbi_image_free(data);

  return ret;
}

} // namespace igl::shell
