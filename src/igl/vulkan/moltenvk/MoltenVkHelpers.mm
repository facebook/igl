/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <TargetConditionals.h>
#if TARGET_OS_OSX
#import <Cocoa/Cocoa.h>
#elif TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#endif
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <igl/Common.h>
#include <igl/vulkan/moltenvk/MoltenVkHelpers.h>

namespace igl::vulkan {

void* getCAMetalLayer(void* window) {
#if TARGET_OS_OSX
  auto* object = (__bridge NSObject*)window;

  if ([object isKindOfClass:[CAMetalLayer class]]) {
    return window;
  }
  auto layer = [CAMetalLayer layer];

  NSView* view = nil;
  if ([object isKindOfClass:[NSView class]]) {
    view = (NSView*)object;
  } else if ([object isKindOfClass:[NSWindow class]]) {
    auto* nsWindow = (__bridge NSWindow*)window;
    auto contentView = nsWindow.contentView;
    layer.delegate = contentView;
    view = nsWindow.contentView;
  } else {
    IGL_ASSERT_NOT_REACHED();
    return window;
  }
  view.layer = layer;
  view.wantsLayer = YES;

  NSScreen* screen = [NSScreen mainScreen];
  CGFloat factor = [screen backingScaleFactor];
  layer.contentsScale = factor;

  return (__bridge void*)layer;
#elif TARGET_OS_IPHONE
  auto* uiView = (__bridge UIView*)window;
  return (__bridge void*)uiView.layer;
#endif
}
} // namespace igl::vulkan
