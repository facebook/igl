/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <filesystem>
#include <shell/shared/imageWriter/ImageWriter.h>
#include <shell/shared/imageWriter/stb/ImageWriterSTB.h>
#if defined(IGL_CMAKE_BUILD)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#endif // IGL_CMAKE_BUILD
#include <stb_image_write.h>

namespace igl::shell {

void ImageWriterSTB::writeImage(const std::string& imageAbsolutePath,
                                const ImageData& imageData) const noexcept {
  if (imageData.desc.width * 4 > std::numeric_limits<int>::max() ||
      imageData.desc.height > std::numeric_limits<int>::max()) {
    IGLLog(IGLLogError, "Failed saving the file: %s", imageAbsolutePath.c_str());
    return;
  }
  stbi_flip_vertically_on_write(1);
  auto ret = stbi_write_png(imageAbsolutePath.c_str(),
                            static_cast<int>(imageData.desc.width),
                            static_cast<int>(imageData.desc.height),
                            4,
                            imageData.data->data(),
                            /*int stride_in_bytes*/ 4 * static_cast<int>(imageData.desc.width));
  if (!ret) {
    IGLLog(IGLLogError, "Failed saving the file: %s", imageAbsolutePath.c_str());
  }
}

} // namespace igl::shell
