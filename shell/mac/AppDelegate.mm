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
#include <shell/shared/renderSession/DefaultRenderSessionFactory.h>
#include <shell/shared/renderSession/RenderSessionConfig.h>

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
  case igl::ColorSpace::BT709_LINEAR:
  case igl::ColorSpace::BT709_NONLINEAR:
  case igl::ColorSpace::BT2020_LINEAR:
  case igl::ColorSpace::HDR10_ST2084:
  case igl::ColorSpace::DOLBYVISION:
  case igl::ColorSpace::HDR10_HLG:
  case igl::ColorSpace::BT2020_NONLINEAR:
  case igl::ColorSpace::BT601_NONLINEAR:
  case igl::ColorSpace::BT2100_HLG_NONLINEAR:
  case igl::ColorSpace::BT2100_PQ_NONLINEAR:
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
    return [NSColorSpace sRGBColorSpace];
  }
  IGL_UNREACHABLE_RETURN([NSColorSpace sRGBColorSpace]);
}
#endif
} // namespace

@interface AppDelegate ()

@property (weak) IBOutlet NSWindow* window;
@property NSTabViewController* tabViewController;
@property std::shared_ptr<igl::shell::IRenderSessionFactory> factory;

- (void)addTab:(igl::shell::RenderSessionWindowConfig)windowConfig
    sessionConfig:(igl::shell::RenderSessionConfig)sessionConfig
            frame:(CGRect)frame;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification {
  [self setupViewController];
  [self.window makeKeyAndOrderFront:nil];
  [self.window makeFirstResponder:self.tabViewController.view];
}

- (void)setupViewController {
  self.tabViewController = [[NSTabViewController alloc] init];
  self.factory = igl::shell::createDefaultRenderSessionFactory();

  igl::shell::RenderSessionWindowConfig suggestedWindowConfig = {
      .width = 1024,
      .height = 768,
      .windowMode = igl::shell::WindowMode::Window,
  };
  constexpr auto kColorFramebufferFormat = igl::TextureFormat::BGRA_SRGB;
  std::vector<igl::shell::RenderSessionConfig> suggestedSessionConfigs = {
#if IGL_BACKEND_HEADLESS
      {
          .displayName = "Headless",
          .backendVersion = {.flavor = igl::BackendFlavor::Invalid,
                             .majorVersion = 0,
                             .minorVersion = 0},
          .swapchainColorTextureFormat = igl::TextureFormat::RGBA_SRGB, // special case that should
                                                                        // probably go through
                                                                        // swiftshader instead
      },
#endif
#if IGL_BACKEND_METAL
      {
          .displayName = "Metal",
          .backendVersion = {.flavor = igl::BackendFlavor::Metal,
                             .majorVersion = 3,
                             .minorVersion = 0},
          .swapchainColorTextureFormat = kColorFramebufferFormat,
      },
#endif
#if IGL_BACKEND_OPENGL
      {
          .displayName = "OGL 4.1",
          .backendVersion = {.flavor = igl::BackendFlavor::OpenGL,
                             .majorVersion = 4,
                             .minorVersion = 1},
          .swapchainColorTextureFormat = kColorFramebufferFormat,
      },
      // clang-format off
      // @fb-only
          // clang-format on
          // @fb-only
          // @fb-only
                             // @fb-only
                             // @fb-only
          // @fb-only
      // @fb-only
#endif
// @fb-only
         // clang-format off
      // @fb-only
          // clang-format on
          // @fb-only
          // @fb-only
                             // @fb-only
                             // @fb-only
          // @fb-only
      // @fb-only
// @fb-only
#if IGL_BACKEND_VULKAN
      {
          .displayName = "Vulkan",
          .backendVersion = {.flavor = igl::BackendFlavor::Vulkan,
                             .majorVersion = 1,
                             .minorVersion = 1},
          .swapchainColorTextureFormat = kColorFramebufferFormat,
      },
#endif
  };

  const auto requestedWindowConfig =
      self.factory->requestedWindowConfig(igl::shell::ShellType::Mac, suggestedWindowConfig);

  IGL_DEBUG_ASSERT(requestedWindowConfig.windowMode == igl::shell::WindowMode::Window ||
                   requestedWindowConfig.windowMode == igl::shell::WindowMode::MaximizedWindow);

  CGRect frame = requestedWindowConfig.windowMode == igl::shell::WindowMode::Window
                     ? [self.window frame]
                     : [[NSScreen mainScreen] frame];
  if (requestedWindowConfig.windowMode == igl::shell::WindowMode::Window) {
    frame.size = CGSizeMake(requestedWindowConfig.width, requestedWindowConfig.height);
  }

  const auto requestedSessionConfigs = self.factory->requestedSessionConfigs(
      igl::shell::ShellType::Mac, std::move(suggestedSessionConfigs));
  for (const auto& sessionConfig : requestedSessionConfigs) {
    [self addTab:requestedWindowConfig sessionConfig:sessionConfig frame:frame];
  }
}

- (void)addTab:(igl::shell::RenderSessionWindowConfig)windowConfig
    sessionConfig:(igl::shell::RenderSessionConfig)sessionConfig
            frame:(CGRect)frame {
  ViewController* viewController = nullptr;
  bool supported = false;
#if IGL_BACKEND_HEADLESS
  if (sessionConfig.backendVersion.flavor == igl::BackendFlavor::Invalid) {
    supported = true;
  }
#endif
#if IGL_BACKEND_METAL
  if (sessionConfig.backendVersion.flavor == igl::BackendFlavor::Metal) {
    supported = true;
  }
#endif
#if IGL_BACKEND_OPENGL
  if (sessionConfig.backendVersion.flavor == igl::BackendFlavor::OpenGL) {
    supported = true;
    NSColorSpace* metalColorSpace = colorSpaceToNSColorSpace(viewController.colorSpace);
    [self.window setColorSpace:metalColorSpace];
  }
#endif
// @fb-only
  // @fb-only
    // @fb-only
  // @fb-only
// @fb-only
#if IGL_BACKEND_VULKAN
  if (sessionConfig.backendVersion.flavor == igl::BackendFlavor::Vulkan) {
    supported = true;
  }
#endif
  if (!IGL_DEBUG_VERIFY(supported)) {
    return;
  }

  NSTabViewItem* tabViewItem = [[NSTabViewItem alloc] initWithIdentifier:nil];

  viewController = [[ViewController alloc] initWithFrame:frame
                                                 factory:*self.factory
                                                  config:sessionConfig];

  tabViewItem.viewController = viewController;
  tabViewItem.label = [NSString stringWithUTF8String:sessionConfig.displayName.c_str()];
  [self.tabViewController addTabViewItem:tabViewItem];

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
