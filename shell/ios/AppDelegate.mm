/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "AppDelegate.h"

#import "ViewController.h"

#include <igl/Macros.h>
#if IGL_BACKEND_OPENGL
#include <igl/opengl/GLIncludes.h>
#endif

@implementation AppDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];

  NSMutableArray<UIViewController*>* viewControllers = [NSMutableArray array];

#if IGL_BACKEND_METAL
  { // Metal
    UIViewController* viewController = [[ViewController alloc] initForMetal:self.window.frame];
    viewController.tabBarItem = [[UITabBarItem alloc] initWithTitle:@"Metal" image:nil tag:0];
    [viewControllers addObject:viewController];
  }
#endif

#if IGL_BACKEND_OPENGL
#if GL_ES_VERSION_3_0
  { // OpenGL ES 3
    UIViewController* viewController =
        [[ViewController alloc] initForOpenGL:igl::opengl::RenderingAPI::GLES3
                                        frame:self.window.frame];
    viewController.tabBarItem = [[UITabBarItem alloc] initWithTitle:@"OpenGL ES 3" image:nil tag:1];
    [viewControllers addObject:viewController];
  }
#endif
  { // OpenGL ES 2
    UIViewController* viewController =
        [[ViewController alloc] initForOpenGL:igl::opengl::RenderingAPI::GLES2
                                        frame:self.window.frame];
    viewController.tabBarItem = [[UITabBarItem alloc] initWithTitle:@"OpenGL ES 2" image:nil tag:2];
    [viewControllers addObject:viewController];
  }
#endif

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

@end
