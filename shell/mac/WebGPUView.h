/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import <AppKit/NSView.h> // IWYU pragma: keep
#include <shell/shared/platform/Platform.h>

@class ViewController;

/// A CAMetalLayer-backed view (set wantsLayer) for the WebGPU backend; a display link drives the
/// controller's render. The controller owns the igl::webgpu::Surface created from the layer.
@interface WebGPUView : NSView
- (void)prepareWebGPU:(igl::shell::Platform*)platform controller:(ViewController*)controller;
- (void)startTimer;
- (void)stopTimer;
/// Stops the display link and removes the presentation-rate backend from the platform. Call before
/// the platform passed to prepareWebGPU is destroyed.
- (void)detachRatePacing;
/// The layer's size in pixels.
- (CGSize)drawableSize;
@end
