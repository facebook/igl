/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "MetalView.h"

// NOLINTBEGIN(readability-identifier-naming)
@implementation MetalView {
  NSTrackingArea* _trackingArea; // needed for mouseMoved: events
  IBOutlet NSViewController* _viewController;
}
// NOLINTEND(readability-identifier-naming)

- (id)initWithFrame:(NSRect)frame device:(nullable id<MTLDevice>)device {
  if (self = [super initWithFrame:frame device:device]) {
    [self addFullScreenTrackingArea];
  }
  return self;
}

- (void)setViewController:(NSViewController*)newController {
  if (_viewController) {
    NSResponder* _Nullable controllerNextResponder = [_viewController nextResponder];
    [super setNextResponder:controllerNextResponder];
    [_viewController setNextResponder:nil];
  }
  _viewController = newController;
  if (newController) {
    NSResponder* _Nullable ownNextResponder = [self nextResponder];
    [super setNextResponder:_viewController];
    [_viewController setNextResponder:ownNextResponder];
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
  if (_viewController) {
    [_viewController setNextResponder:newNextResponder];
    return;
  }
  [super setNextResponder:newNextResponder];
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)keyUp:(NSEvent*)event {
  if (_viewController) {
    [_viewController keyUp:event];
  }
}

- (void)keyDown:(NSEvent*)event {
  if (_viewController) {
    [_viewController keyDown:event];
  }
}

@end
