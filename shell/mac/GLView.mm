/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "GLView.h"

#import "AppDelegate.h"
#import "ViewController.h"

#import <Foundation/Foundation.h>

@interface GLView () {
  CVDisplayLinkRef _displayLink; // display link for managing rendering thread
}
@property (weak) ViewController* viewController;
@end

@implementation GLView {
  NSTrackingArea* _trackingArea; // needed for mouseMoved: events
}

@synthesize viewController = _viewController;

- (id)initWithFrame:(NSRect)frame {
  if (self = [super initWithFrame:frame]) {
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

- (void)dealloc {
  CVDisplayLinkRelease(_displayLink);
}

- (void)prepareOpenGL {
  [super prepareOpenGL];

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

static CVReturn displayLinkCallback(CVDisplayLinkRef /*displayLink*/,
                                    const CVTimeStamp* /*now*/,
                                    const CVTimeStamp* /*outputTime*/,
                                    CVOptionFlags /*flagsIn*/,
                                    CVOptionFlags* /*flagsOut*/,
                                    void* userdata) {
  // TODO: For some reason, OpenGL is crashing when called from display link thread
  // so we use setNeedsDisplay for now.
  [(__bridge GLView*)userdata performSelectorOnMainThread:@selector(invalidateFrame)
                                               withObject:nil
                                            waitUntilDone:NO];
  return kCVReturnSuccess;
}

- (void)initTimer {
  // Synchronize buffer swaps with vertical refresh rate
  GLint swapInt = 1;
  [[self openGLContext] setValues:&swapInt forParameter:NSOpenGLContextParameterSwapInterval];

  // Create a display link capable of being used with all active displays
  CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);

  // Set the renderer output callback function
  CVDisplayLinkSetOutputCallback(_displayLink, &displayLinkCallback, (__bridge void*)self);

  // Set the display link for the current renderer
  NSOpenGLContext* glContext = [self openGLContext];
  CGLContextObj cglContext = [glContext CGLContextObj];
  CGLPixelFormatObj cglPixelFormat = [[glContext pixelFormat] CGLPixelFormatObj];
  CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(_displayLink, cglContext, cglPixelFormat);
}

- (void)startTimer {
  CVDisplayLinkStart(_displayLink);
}

- (void)stopTimer {
  CVDisplayLinkStop(_displayLink);
}

- (void)invalidateFrame {
  [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)bounds {
  [self.viewController render];
}

@end
