/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/imageWriter/android/ImageWriterAndroid.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "stb_image.h"

namespace igl::shell {

void ImageWriterAndroid::writeImage(const std::string& imageAbsolutePath,
                                    const ImageData& imageData) const noexcept {
  IGL_ASSERT_NOT_IMPLEMENTED();
}

} // namespace igl::shell
