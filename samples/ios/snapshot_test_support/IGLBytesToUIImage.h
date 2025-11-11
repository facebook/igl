/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import <Foundation/NSObjCRuntime.h>
#import <igl/IGL.h>

@class UIImage;

NS_ASSUME_NONNULL_BEGIN

extern "C" {

// NOLINTNEXTLINE(readability-identifier-naming)
UIImage* IGLRGBABytesToUIImage(void* bytes, size_t width, size_t height);

// NOLINTNEXTLINE(readability-identifier-naming)
UIImage* IGLFramebufferToUIImage(const igl::IFramebuffer& framebuffer,
                                 igl::ICommandQueue& commandQueue,
                                 size_t width,
                                 size_t height);
}

NS_ASSUME_NONNULL_END
