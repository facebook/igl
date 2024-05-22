/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "MetalView.h"

@implementation MetalView {
  NSTrackingArea* _trackingArea; // needed for mouseMoved: events
  IBOutlet NSViewController* viewController;
}

- (id)initWithFrame:(NSRect)frame device:(nullable id<MTLDevice>)device {
  if (self = [super initWithFrame:frame device:device]) {
    [self addFullScreenTrackingArea];
  }
  return self;
}

- (void)setViewController:(NSViewController*)newController {
  if (viewController) {
    NSResponder* controllerNextResponder = [viewController nextResponder];
    [super setNextResponder:controllerNextResponder];
    [viewController setNextResponder:nil];
  }
  viewController = newController;
  if (newController) {
    NSResponder* ownNextResponder = [self nextResponder];
    [super setNextResponder:viewController];
    [viewController setNextResponder:ownNextResponder];
  }
}

- (void)addFullScreenTrackingArea {
  _trackingArea =
      [[NSTrackingArea alloc] initWithRect:self.bounds
                                   options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
                                           NSTrackingActiveInKeyWindow
                                     owner:self
                                  userInfo:nil];
  [self addTrackingArea:_trackingArea];
}

- (void)updateTrackingAreas {
  [self removeTrackingArea:_trackingArea];
  [self addFullScreenTrackingArea];
}

- (void)setNextResponder:(NSResponder*)newNextResponder {
  if (viewController) {
    [viewController setNextResponder:newNextResponder];
    return;
  }
  [super setNextResponder:newNextResponder];
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)keyUp:(NSEvent*)event {
  if (viewController) {
    [viewController keyUp:event];
  }
}

- (void)keyDown:(NSEvent*)event {
  if (viewController) {
    [viewController keyDown:event];
  }
}

@end
