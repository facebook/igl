/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "VulkanView.h"

#import "AppDelegate.h"
#import "ViewController.h"

#import <Foundation/Foundation.h>

#if IGL_BACKEND_VULKAN
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanSwapchain.h>
#endif

@interface VulkanView () {
  CVDisplayLinkRef _displayLink; // display link for managing rendering thread
  igl::shell::Platform* _shellPlatform;
  IBOutlet NSViewController* _viewController;
}
@end

@implementation VulkanView

- (void)dealloc {
  CVDisplayLinkRelease(_displayLink);
  _shellPlatform = nullptr;
}

- (void)prepareVulkan:(igl::shell::Platform*)platform {
  NSApplication* app = [NSApplication sharedApplication];
  NSWindow* window = [app windows][0];
  NSTabViewController* tabController = (NSTabViewController*)window.contentViewController;
  NSTabViewItem* item = tabController.tabViewItems[tabController.selectedTabViewItemIndex];

  ViewController* controller = (ViewController*)item.viewController;
  self->_viewController = controller;
  _shellPlatform = platform;
  self.postsFrameChangedNotifications = YES;

  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(frameDidChange:)
                                               name:NSViewFrameDidChangeNotification
                                             object:self];

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
  auto view = (__bridge VulkanView*)userdata;
  [view->_viewController performSelectorOnMainThread:@selector(render)
                                          withObject:nil
                                       waitUntilDone:NO];
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

/** Indicates that the view wants to draw using the backing layer instead of using drawRect:.  */
- (BOOL)wantsUpdateLayer {
  return YES;
}

- (void)viewDidChangeBackingProperties {
  [self updateSwapchain];
}

- (void)updateSwapchain {
  const NSRect contentRect = [self frame];
  const NSRect imageRect = [self convertRectToBacking:contentRect];
  const float xscale = imageRect.size.width / contentRect.size.width;

  if (self.layer != nil && xscale != [self.layer contentsScale]) {
    [self.layer setContentsScale:xscale];
  }

#if IGL_BACKEND_VULKAN
  if (_shellPlatform != nullptr) {
    auto& device = static_cast<igl::vulkan::Device&>(_shellPlatform->getDevice());
    const igl::vulkan::VulkanContext& vulkanContext = device.getVulkanContext();
    auto extents = vulkanContext.getSwapchainExtent();
    if (imageRect.size.width != extents.width || imageRect.size.height != extents.height) {
      device.getVulkanContext().initSwapchain(imageRect.size.width, imageRect.size.height);
    }
  }
#endif // IGL_BACKEND_VULKAN
}

- (void)frameDidChange:(NSNotification*)notification {
  [self updateSwapchain];
}

/** Returns a Metal-compatible layer. */
+ (Class)layerClass {
  return [CAMetalLayer class];
}

/** If the wantsLayer property is set to YES, this method will be invoked to return a layer
 * instance. */
- (CALayer*)makeBackingLayer {
  CALayer* layer = [self.class.layerClass layer];
  NSScreen* screen = [NSScreen mainScreen];
  CGFloat factor = [screen backingScaleFactor];
  layer.contentsScale = factor;
  return layer;
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
