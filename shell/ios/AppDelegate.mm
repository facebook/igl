/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "AppDelegate.h"

#import "RenderSessionFactoryProvider.h"
#import "ViewController.h"

#include "RenderSessionFactoryAdapterInternal.hpp"
#include <igl/Macros.h>
#if IGL_BACKEND_OPENGL
#include <igl/opengl/GLIncludes.h>
#endif
#include <shell/shared/renderSession/RenderSessionConfig.h>

@interface AppDelegate () {
  RenderSessionFactoryProvider* factoryProvider_;
}
- (void)addTab:(igl::shell::RenderSessionConfig)config
    viewControllers:(NSMutableArray<UIViewController*>*)viewControllers;
@end

@implementation AppDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
  factoryProvider_ = [[RenderSessionFactoryProvider alloc] init];
  NSMutableArray<UIViewController*>* viewControllers = [NSMutableArray array];

  std::vector<igl::shell::RenderSessionConfig> suggestedSessionConfigs = {
#if IGL_BACKEND_METAL
      {
          .displayName = "Metal",
          .backendVersion = {.flavor = igl::BackendFlavor::Metal,
                             .majorVersion = 3,
                             .minorVersion = 0},
          .swapchainColorTextureFormat = igl::TextureFormat::BGRA_SRGB,
          .depthTextureFormat = igl::TextureFormat::S8_UInt_Z32_UNorm,
      },
#endif
#if IGL_BACKEND_OPENGL
#if GL_ES_VERSION_3_0
      {
          .displayName = "OpenGL ES 3.0",
          .backendVersion = {.flavor = igl::BackendFlavor::OpenGL_ES,
                             .majorVersion = 3,
                             .minorVersion = 0},
          .swapchainColorTextureFormat = igl::TextureFormat::BGRA_SRGB,
          .depthTextureFormat = igl::TextureFormat::S8_UInt_Z24_UNorm,
      },
#endif
      {
          .displayName = "OpenGL ES 2.0",
          .backendVersion = {.flavor = igl::BackendFlavor::OpenGL_ES,
                             .majorVersion = 2,
                             .minorVersion = 0},
          .swapchainColorTextureFormat = igl::TextureFormat::BGRA_SRGB,
          .depthTextureFormat = igl::TextureFormat::S8_UInt_Z24_UNorm,
      },
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
  };

  const auto requestedSessionConfigs = factoryProvider_.adapter->factory->requestedSessionConfigs(
      igl::shell::ShellType::iOS, std::move(suggestedSessionConfigs));
  for (const auto& sessionConfig : requestedSessionConfigs) {
    [self addTab:sessionConfig viewControllers:viewControllers];
  }

  UITabBarController* tabBarController = [[UITabBarController alloc] initWithNibName:nil
                                                                              bundle:nil];
  tabBarController.delegate = self;
  tabBarController.viewControllers = viewControllers;
  tabBarController.tabBar.translucent = NO;
  if (@available(iOS 13.0, *)) {
    tabBarController.tabBar.backgroundColor = UIColor.systemBackgroundColor;
  }

  self.window.rootViewController = tabBarController;
  [self.window makeKeyAndVisible];

  return YES;
}

- (void)addTab:(igl::shell::RenderSessionConfig)config
    viewControllers:(NSMutableArray<UIViewController*>*)viewControllers {
  bool supported = false;
#if IGL_BACKEND_METAL
  if (config.backendVersion.flavor == igl::BackendFlavor::Metal) {
    supported = true;
  }
#endif
#if IGL_BACKEND_OPENGL
  if (config.backendVersion.flavor == igl::BackendFlavor::OpenGL_ES) {
    supported = true;
  }
#endif
// @fb-only
  // @fb-only
    // @fb-only
  // @fb-only
// @fb-only
  if (!IGL_DEBUG_VERIFY(supported)) {
    return;
  }

  UIViewController* viewController = [[ViewController alloc] init:config
                                                  factoryProvider:factoryProvider_
                                                            frame:self.window.frame];
  viewController.tabBarItem =
      [[UITabBarItem alloc] initWithTitle:[NSString stringWithUTF8String:config.displayName.c_str()]
                                    image:nil
                                      tag:0];
  [viewControllers addObject:viewController];
}

@end
