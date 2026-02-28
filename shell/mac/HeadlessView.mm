/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "HeadlessView.h"

#import "ViewController.h"

#import <AppKit/NSApplication.h>
#import <AppKit/NSTabViewController.h>
#import <AppKit/NSTabViewItem.h>
#import <AppKit/NSTrackingArea.h>
#import <AppKit/NSWindow.h>
#import <CoreVideo/CVDisplayLink.h>

@interface HeadlessView () {
  // NOLINTNEXTLINE(readability-identifier-naming)
  CVDisplayLinkRef _displayLink; // display link for managing rendering thread
  // NOLINTNEXTLINE(readability-identifier-naming)
  NSTrackingArea* _trackingArea; // needed for mouseMoved: events
}
@property (weak) ViewController* viewController;
@end

@implementation HeadlessView

@synthesize viewController = _viewController;

- (void)dealloc {
  CVDisplayLinkRelease(_displayLink);
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

static CVReturn displayLinkCallback(
    CVDisplayLinkRef /*displayLink*/, // NOLINT(readability-identifier-naming)
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
  CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);

  // Set the renderer output callback function
  CVDisplayLinkSetOutputCallback(_displayLink, &displayLinkCallback, (__bridge void*)self);
}

- (void)startTimer {
  CVDisplayLinkStart(_displayLink);
}

- (void)stopTimer {
  CVDisplayLinkStop(_displayLink);
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
