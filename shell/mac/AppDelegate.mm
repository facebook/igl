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
                                             backendType:igl::BackendType::Invalid
                                     preferLatestVersion:true];

  tinyHeadlessTabViewItem.viewController = viewController;
  tinyHeadlessTabViewItem.label = @"Headless";
  [self.tabViewController addTabViewItem:tinyHeadlessTabViewItem];
#endif

#if IGL_BACKEND_METAL
  // Metal tab
  NSTabViewItem* tinyMetalTabViewItem = [[NSTabViewItem alloc] initWithIdentifier:nil];
  viewController = [[ViewController alloc] initWithFrame:frame
                                             backendType:igl::BackendType::Metal
                                     preferLatestVersion:true];
  tinyMetalTabViewItem.viewController = viewController;

  tinyMetalTabViewItem.label = @"Metal";
  [self.tabViewController addTabViewItem:tinyMetalTabViewItem];
#endif

#if IGL_BACKEND_OPENGL
  // OpenGL tab
  NSTabViewItem* tinyOGL4TabViewItem = [[NSTabViewItem alloc] initWithIdentifier:nil];
  viewController = [[ViewController alloc] initWithFrame:frame
                                             backendType:igl::BackendType::OpenGL
                                            majorVersion:4
                                            minorVersion:1];
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
  // @fb-only
  // @fb-only
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
                                             backendType:igl::BackendType::Vulkan
                                     preferLatestVersion:true];

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
