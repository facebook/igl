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
  CVDisplayLinkRef displayLink_; // display link for managing rendering thread
  std::shared_ptr<igl::shell::Platform> shellPlatform_;
  IBOutlet NSViewController* viewController;
}
@end

@implementation VulkanView

- (void)dealloc {
  CVDisplayLinkRelease(displayLink_);
}

- (void)prepareVulkan:(std::shared_ptr<igl::shell::Platform>)platform {
  NSApplication* app = [NSApplication sharedApplication];
  NSWindow* window = [app windows][0];
  NSTabViewController* tabController = (NSTabViewController*)window.contentViewController;
  NSTabViewItem* item = tabController.tabViewItems[tabController.selectedTabViewItemIndex];

  ViewController* controller = (ViewController*)item.viewController;
  self->viewController = controller;
  shellPlatform_ = platform;
  self.postsFrameChangedNotifications = YES;

  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(frameDidChange:)
                                               name:NSViewFrameDidChangeNotification
                                             object:self];

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
  auto view = (__bridge VulkanView*)userdata;
  [view->viewController performSelectorOnMainThread:@selector(render)
                                         withObject:nil
                                      waitUntilDone:NO];
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
  auto& device = static_cast<igl::vulkan::Device&>(shellPlatform_->getDevice());
  const igl::vulkan::VulkanContext& vulkanContext = device.getVulkanContext();
  auto extents = vulkanContext.getSwapchainExtent();
  if (imageRect.size.width != extents.width || imageRect.size.height != extents.height) {
    device.getVulkanContext().initSwapchain(imageRect.size.width, imageRect.size.height);
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
