/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/imageLoader/ios/ImageLoaderIos.h>

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include <igl/IGL.h>

namespace igl::shell {

ImageLoaderIos::ImageLoaderIos(FileLoader& fileLoader) : ImageLoader(fileLoader) {}

ImageData ImageLoaderIos::loadImageData(std::string imageName) noexcept {
  auto ret = ImageData();

  NSString* name = [NSString stringWithUTF8String:imageName.c_str()];
  UIImage* uiImage = [UIImage imageNamed:name];
  if (!uiImage) {
    IGL_LOG_ERROR("Failed to load image: %s\n", imageName.c_str());
    IGL_ASSERT_NOT_REACHED();
    return ret;
  }

  CGImageRef image = [uiImage CGImage];
  ret.width = CGImageGetWidth(image);
  ret.height = CGImageGetHeight(image);
  ret.bitsPerComponent = CGImageGetBitsPerComponent(image);
  ret.bytesPerRow = CGImageGetBytesPerRow(image);
  IGL_ASSERT(ret.bytesPerRow == ret.width * (ret.bitsPerComponent * 4 / 8));
  ret.buffer.resize(ret.bytesPerRow * ret.height);
  if (CGImageGetColorSpace(image) != CGColorSpaceCreateWithName(kCGColorSpaceSRGB)) {
    IGL_ASSERT_NOT_IMPLEMENTED();
  }
  CGContextRef context = CGBitmapContextCreate(ret.buffer.data(),
                                               ret.width,
                                               ret.height,
                                               ret.bitsPerComponent,
                                               ret.bytesPerRow,
                                               CGImageGetColorSpace(image),
                                               kCGImageAlphaPremultipliedLast);
  CGContextDrawImage(context, CGRectMake(0, 0, ret.width, ret.height), image);

  CGContextRelease(context);
  return ret;
}

} // namespace igl::shell
