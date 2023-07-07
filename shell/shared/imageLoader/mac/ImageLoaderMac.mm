/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/imageLoader/mac/ImageLoaderMac.h>

#import <AppKit/NSImage.h>
#import <Foundation/Foundation.h>
#include <string>

namespace {

NSImage* imageForFileName(const std::string& fileName) {
  if (fileName.length() == 0) {
    return nil;
  }
  NSString* nsFileName = [NSString stringWithUTF8String:fileName.c_str()];
  if ([nsFileName length] == 0) {
    return nil;
  }
  for (NSBundle* bundle in [NSBundle allBundles]) {
    NSImage* image = [bundle imageForResource:nsFileName];
    if (image) {
      return image;
    }
  }
  return nil;
}

}

namespace igl::shell {

ImageData ImageLoaderMac::loadImageData(std::string imageName) noexcept {
  auto ret = ImageData();
  CGImage* image = [imageForFileName(imageName) CGImageForProposedRect:nil context:nil hints:nil];
  IGL_ASSERT_MSG(image, "Could not find image file: %s", imageName.c_str());
  ret.width = CGImageGetWidth(image);
  ret.height = CGImageGetHeight(image);
  ret.bitsPerComponent = CGImageGetBitsPerComponent(image);
  ret.bytesPerRow = CGImageGetBytesPerRow(image);
  IGL_ASSERT(ret.bytesPerRow == ret.width * (ret.bitsPerComponent * 4 / 8));
  ret.buffer.resize(ret.bytesPerRow * ret.height);

  CGColorSpaceRef imageColorSpace = CGImageGetColorSpace(image);
  CGColorSpaceModel imageColorModel = CGColorSpaceGetModel(imageColorSpace);
  CGColorSpaceRef srgbColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  CGColorSpaceModel srgbColorModel = CGColorSpaceGetModel(srgbColorSpace);
  // Test that this is an RGB testure
  if (imageColorModel != srgbColorModel) {
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
