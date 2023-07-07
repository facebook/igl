/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import "MetalView.h"

@implementation MetalView {
  NSTrackingArea* _trackingArea; // needed for mouseMoved: events
}

- (id)initWithFrame:(NSRect)frame device:(nullable id<MTLDevice>)device {
  if (self = [super initWithFrame:frame device:device]) {
    [self addFullScreenTrackingArea];
  }
  return self;
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

@end
