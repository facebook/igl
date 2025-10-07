/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import "IGLBytesToUIImage.h"

#import <CoreGraphics/CGColorSpace.h>
#import <CoreGraphics/CGContext.h>
#import <CoreGraphics/CGDataProvider.h>
#import <CoreGraphics/CGGeometry.h>
#import <CoreGraphics/CGImage.h>
#import <Foundation/NSData.h>
#import <Foundation/NSException.h>
#import <UIKit/UIGraphics.h>
#import <UIKit/UIImage.h>

UIImage* IGLRGBABytesToUIImage(void* bytes, size_t width, size_t height) {
  auto data = [NSData dataWithBytes:bytes length:width * height * 4];

  auto* const colorSpace = CGColorSpaceCreateDeviceRGB();
  auto* const provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)data);
  auto* const cgImage = CGImageCreate(width,
                                      height,
                                      8,
                                      32,
                                      width * 4,
                                      colorSpace,
                                      kCGImageAlphaLast,
                                      provider,
                                      nullptr,
                                      NO,
                                      kCGRenderingIntentDefault);
  CGColorSpaceRelease(colorSpace);
  CGDataProviderRelease(provider);

  const CGRect bounds = CGRectMake(0, 0, width, height);
  UIGraphicsBeginImageContextWithOptions(bounds.size, NO, 0);
  CGContextRef context = UIGraphicsGetCurrentContext();
  CGContextSaveGState(context);
  CGContextDrawImage(context, bounds, cgImage);
  CGContextRestoreGState(context);
  UIImage* snapshot = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();

  CGImageRelease(cgImage);

  return snapshot;
}

UIImage* IGLFramebufferToUIImage(const igl::IFramebuffer& framebuffer,
                                 igl::ICommandQueue& commandQueue,
                                 size_t width,
                                 size_t height) {
  if (framebuffer.getColorAttachment(0)->getProperties().format !=
      igl::TextureFormat::RGBA_UNorm8) {
    NSCAssert(false, @"Only BGRA texture format is supported");
    return nil;
  }

  auto pixels = std::vector<uint32_t>(width * height);
  framebuffer.copyBytesColorAttachment(
      commandQueue, 0, pixels.data(), igl::TextureRangeDesc::new2D(0, 0, width, height));

  return IGLRGBABytesToUIImage(pixels.data(), width, height);
}
