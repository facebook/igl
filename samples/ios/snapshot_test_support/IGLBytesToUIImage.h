/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <igl/IGL.h>

NS_ASSUME_NONNULL_BEGIN

extern "C" {

UIImage* IGLRGBABytesToUIImage(void* bytes, size_t width, size_t height);

UIImage* IGLFramebufferToUIImage(const igl::IFramebuffer& framebuffer,
                                 igl::ICommandQueue& commandQueue,
                                 size_t width,
                                 size_t height);
}

NS_ASSUME_NONNULL_END
