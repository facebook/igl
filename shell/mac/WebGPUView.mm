/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "WebGPUView.h"

#import "DisplayLinkRatePacing.h"
#import "ViewController.h"

#import <QuartzCore/CAMetalLayer.h>
#include <memory>

@interface WebGPUView () {
  CVDisplayLinkRef _displayLink;
  std::unique_ptr<igl::shell::DisplayLinkRatePacing> _ratePacing;
  __weak ViewController* _viewController;
}
@end

@implementation WebGPUView

- (void)dealloc {
  if (_ratePacing) {
    // detach() stops the link and waits for a running callback, which reads _ratePacing.
    _ratePacing->detach();
  }
  _ratePacing.reset();
  if (_displayLink != nullptr) {
    CVDisplayLinkStop(_displayLink);
    CVDisplayLinkRelease(_displayLink);
  }
}

- (void)prepareWebGPU:(igl::shell::Platform*)platform controller:(ViewController*)controller {
  _viewController = controller;
  CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);
  CVDisplayLinkSetOutputCallback(_displayLink, &displayLinkCallback, (__bridge void*)self);
  if (platform != nullptr && _displayLink != nullptr) {
    _ratePacing = std::make_unique<igl::shell::DisplayLinkRatePacing>(*platform, _displayLink);
  }
}

// NOLINTNEXTLINE(misc-use-anonymous-namespace)
static CVReturn displayLinkCallback(CVDisplayLinkRef /*displayLink*/,
                                    const CVTimeStamp* /*now*/,
                                    const CVTimeStamp* /*outputTime*/,
                                    CVOptionFlags /*flagsIn*/,
                                    CVOptionFlags* /*flagsOut*/,
                                    void* userdata) {
  auto view = (__bridge WebGPUView*)userdata;
  if (view->_ratePacing && !view->_ratePacing->shouldRender()) {
    return kCVReturnSuccess;
  }
  ViewController* controller = view->_viewController;
  [controller performSelectorOnMainThread:@selector(render) withObject:nil waitUntilDone:NO];
  return kCVReturnSuccess;
}

- (void)startTimer {
  if (_displayLink != nullptr && !CVDisplayLinkIsRunning(_displayLink)) {
    CVDisplayLinkStart(_displayLink);
  }
}

- (void)stopTimer {
  if (_displayLink != nullptr) {
    CVDisplayLinkStop(_displayLink);
  }
}

- (void)detachRatePacing {
  if (_ratePacing) {
    _ratePacing->detach();
  }
  _ratePacing.reset();
}

- (CGSize)drawableSize {
  const NSRect backing = [self convertRectToBacking:self.bounds];
  return backing.size;
}

- (BOOL)wantsUpdateLayer {
  return YES;
}

- (void)viewDidChangeBackingProperties {
  [super viewDidChangeBackingProperties];
  self.layer.contentsScale = self.window.backingScaleFactor;
}

- (CALayer*)makeBackingLayer {
  CAMetalLayer* layer = [CAMetalLayer layer];
  layer.contentsScale = [NSScreen mainScreen].backingScaleFactor;
  return layer;
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)keyUp:(NSEvent*)event {
  [_viewController keyUp:event];
}

- (void)keyDown:(NSEvent*)event {
  [_viewController keyDown:event];
}

@end
