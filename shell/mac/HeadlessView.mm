/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "HeadlessView.h"

#import "ViewController.h"

#import <Foundation/Foundation.h>

@interface HeadlessView () {
  CVDisplayLinkRef displayLink_; // display link for managing rendering thread
  NSTrackingArea* trackingArea_; // needed for mouseMoved: events
}
@property (weak) ViewController* viewController;
@end

@implementation HeadlessView

- (void)dealloc {
  CVDisplayLinkRelease(displayLink_);
}

- (void)prepareHeadless {
  NSApplication* app = [NSApplication sharedApplication];
  NSWindow* window = [app windows][0];
  NSTabViewController* tabController = (NSTabViewController*)window.contentViewController;
  NSTabViewItem* item = tabController.tabViewItems[tabController.selectedTabViewItemIndex];

  ViewController* controller = (ViewController*)item.viewController;
  self.viewController = controller;
  [controller initModule];

  [self initTimer];
  [self startTimer];
}

static CVReturn DisplayLinkCallback(CVDisplayLinkRef /*displayLink*/,
                                    const CVTimeStamp* /*now*/,
                                    const CVTimeStamp* /*outputTime*/,
                                    CVOptionFlags /*flagsIn*/,
                                    CVOptionFlags* /*flagsOut*/,

                                    void* userdata) {
  auto view = (__bridge HeadlessView*)userdata;
  [view.viewController performSelectorOnMainThread:@selector(render)
                                        withObject:nil
                                     waitUntilDone:YES];
  return kCVReturnSuccess;
}

- (void)initTimer {
  // Create a display link capable of being used with all active displays
  CVDisplayLinkCreateWithActiveCGDisplays(&displayLink_);

  // Set the renderer output callback function
  CVDisplayLinkSetOutputCallback(displayLink_, &DisplayLinkCallback, (__bridge void*)self);
}

- (void)startTimer {
  CVDisplayLinkStart(displayLink_);
}

- (void)stopTimer {
  CVDisplayLinkStop(displayLink_);
}

- (void)addFullScreenTrackingArea {
  trackingArea_ =
      [[NSTrackingArea alloc] initWithRect:self.bounds
                                   options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
                                           NSTrackingActiveInKeyWindow
                                     owner:self
                                  userInfo:nil];
  [self addTrackingArea:trackingArea_];
}

- (void)updateTrackingAreas {
  [self removeTrackingArea:trackingArea_];
  [self addFullScreenTrackingArea];
}

@end
