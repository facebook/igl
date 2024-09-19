/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

// @lint-ignore-every CLANGTIDY NonLocalizedStringChecker
#import "AppDelegate.h"

#import "ViewController.h"

#import <igl/Common.h>

namespace {

#if IGL_BACKEND_OPENGL
NSColorSpace* colorSpaceToNSColorSpace(igl::ColorSpace colorSpace) {
  switch (colorSpace) {
  case igl::ColorSpace::SRGB_LINEAR:
    return [NSColorSpace sRGBColorSpace]; // closest thing to linear srgb
  case igl::ColorSpace::SRGB_NONLINEAR:
    return [NSColorSpace sRGBColorSpace];
  case igl::ColorSpace::DISPLAY_P3_NONLINEAR:
    return [NSColorSpace displayP3ColorSpace];
  case igl::ColorSpace::DISPLAY_P3_LINEAR:
    return [NSColorSpace displayP3ColorSpace];
  case igl::ColorSpace::EXTENDED_SRGB_LINEAR:
    return [NSColorSpace extendedSRGBColorSpace];
  case igl::ColorSpace::DCI_P3_NONLINEAR:
    return [NSColorSpace displayP3ColorSpace];
  case igl::ColorSpace::BT709_LINEAR:
    IGL_UNREACHABLE_RETURN([NSColorSpace sRGBColorSpace]);
  case igl::ColorSpace::BT709_NONLINEAR:
    IGL_UNREACHABLE_RETURN([NSColorSpace sRGBColorSpace]);
  case igl::ColorSpace::BT2020_LINEAR:
    IGL_UNREACHABLE_RETURN([NSColorSpace sRGBColorSpace]);
  case igl::ColorSpace::HDR10_ST2084:
    IGL_UNREACHABLE_RETURN([NSColorSpace sRGBColorSpace]);
  case igl::ColorSpace::DOLBYVISION:
    IGL_UNREACHABLE_RETURN([NSColorSpace sRGBColorSpace]);
  case igl::ColorSpace::HDR10_HLG:
    IGL_UNREACHABLE_RETURN([NSColorSpace sRGBColorSpace]);
  case igl::ColorSpace::ADOBERGB_LINEAR:
    return [NSColorSpace adobeRGB1998ColorSpace];
  case igl::ColorSpace::ADOBERGB_NONLINEAR:
    return [NSColorSpace adobeRGB1998ColorSpace];
  case igl::ColorSpace::PASS_THROUGH:
    return nil;
  case igl::ColorSpace::EXTENDED_SRGB_NONLINEAR:
    return [NSColorSpace extendedSRGBColorSpace];
  case igl::ColorSpace::DISPLAY_NATIVE_AMD:
    return [NSColorSpace deviceRGBColorSpace];
  case igl::ColorSpace::BT2020_NONLINEAR:
  case igl::ColorSpace::BT601_NONLINEAR:
  case igl::ColorSpace::BT2100_HLG_NONLINEAR:
  case igl::ColorSpace::BT2100_PQ_NONLINEAR:
    IGL_UNREACHABLE_RETURN([NSColorSpace sRGBColorSpace]);
  }
  IGL_UNREACHABLE_RETURN([NSColorSpace sRGBColorSpace]);
}
#endif
} // namespace

@interface AppDelegate ()

@property (weak) IBOutlet NSWindow* window;
@property NSTabViewController* tabViewController;

@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification {
  [self setupViewController];
  [self.window makeKeyAndOrderFront:nil];
  [self.window makeFirstResponder:self.tabViewController.view];
}

- (void)setupViewController {
  self.tabViewController = [[NSTabViewController alloc] init];
  auto frame = [self.window frame];
  (void)frame;
  ViewController* viewController = nullptr;

#if IGL_BACKEND_HEADLESS
  // Headless tab
  NSTabViewItem* tinyHeadlessTabViewItem = [[NSTabViewItem alloc] initWithIdentifier:nil];
  viewController = [[ViewController alloc] initWithFrame:frame
                                          backendVersion:{igl::BackendFlavor::Invalid, 0, 0}];

  tinyHeadlessTabViewItem.viewController = viewController;
  tinyHeadlessTabViewItem.label = @"Headless";
  [self.tabViewController addTabViewItem:tinyHeadlessTabViewItem];
#endif

#if IGL_BACKEND_METAL
  // Metal tab
  NSTabViewItem* tinyMetalTabViewItem = [[NSTabViewItem alloc] initWithIdentifier:nil];
  viewController = [[ViewController alloc] initWithFrame:frame
                                          backendVersion:{igl::BackendFlavor::Metal, 3, 0}];
  tinyMetalTabViewItem.viewController = viewController;

  tinyMetalTabViewItem.label = @"Metal";
  [self.tabViewController addTabViewItem:tinyMetalTabViewItem];
#endif

#if IGL_BACKEND_OPENGL
  // OpenGL tab
  NSTabViewItem* tinyOGL4TabViewItem = [[NSTabViewItem alloc] initWithIdentifier:nil];
  viewController = [[ViewController alloc] initWithFrame:frame
                                          backendVersion:{igl::BackendFlavor::OpenGL, 4, 1}];
  tinyOGL4TabViewItem.viewController = viewController;

  tinyOGL4TabViewItem.label = @"OGL 4.1";
  [self.tabViewController addTabViewItem:tinyOGL4TabViewItem];
  // @fb-only
  // @fb-only
  // @fb-only
      // @fb-only
                             // @fb-only
  // @fb-only
  // @fb-only

  NSColorSpace* metalColorSpace = colorSpaceToNSColorSpace(viewController.colorSpace);
  [self.window setColorSpace:metalColorSpace];
#endif

// @fb-only
  // @fb-only
  // @fb-only
  // @fb-only
      // @fb-only
                             // @fb-only
  // @fb-only

  // @fb-only
  // @fb-only
// @fb-only

#if IGL_BACKEND_VULKAN
  // Vulkan tab
  NSTabViewItem* tinyVulkanTabViewItem = [[NSTabViewItem alloc] initWithIdentifier:nil];
  viewController = [[ViewController alloc] initWithFrame:frame
                                          backendVersion:{igl::BackendFlavor::Vulkan, 1, 3}];

  tinyVulkanTabViewItem.viewController = viewController;
  tinyVulkanTabViewItem.label = @"Vulkan";
  [self.tabViewController addTabViewItem:tinyVulkanTabViewItem];
#endif

  self.window.contentViewController = self.tabViewController;
  [self.window setFrame:viewController.frame display:YES animate:false];
}

- (void)tearDownViewController {
  if (self.tabViewController) {
    for (NSInteger i = 0; i < [[self.tabViewController tabViewItems] count]; ++i) {
      NSTabViewItem* item = [self.tabViewController tabViewItems][i];
      ViewController* controller = (ViewController*)item.viewController;
      [controller teardown];
    }
  }
  self.tabViewController = nil;
  self.window.contentViewController = nil;
}

- (IBAction)didClickTearDownViewController:(id)sender {
  [self tearDownViewController];
}

- (IBAction)didClickReloadViewController:(id)sender {
  [self tearDownViewController];
  [self setupViewController];
}

- (void)applicationWillTerminate:(NSNotification*)aNotification {
  // Insert code here to tear down your application
  [self tearDownViewController];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
  return YES;
}

@end
