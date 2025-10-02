/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef IGL_PLATFORM_INTERNAL_H
#define IGL_PLATFORM_INTERNAL_H

#include <shell/shared/platform/Platform.h>
#include <memory>

#ifdef __OBJC__
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>

// Internal wrapper structure (only visible in .mm files)
struct IGLPlatform {
    std::shared_ptr<igl::shell::Platform> platform;
    CAMetalLayer* metalLayer;
    id<CAMetalDrawable> currentDrawable;
    id<MTLTexture> depthTexture;
    int32_t width;
    int32_t height;
};

#endif // __OBJC__

#endif // IGL_PLATFORM_INTERNAL_H
