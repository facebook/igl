/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/imageWriter/ios/ImageWriterIos.h>

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include <igl/IGL.h>

namespace igl::shell {
void ImageWriterIos::writeImage(const std::string& /*imageAbsolutePath*/,
                                const ImageData& /*imageData*/) const noexcept {
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
}
} // namespace igl::shell
