/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import <UIKit/UIApplication.h> // IWYU pragma: export
#import <UIKit/UIResponder.h> // IWYU pragma: export
#import <UIKit/UITabBarController.h> // IWYU pragma: export
#import <UIKit/UIWindow.h> // IWYU pragma: export

@interface AppDelegate : UIResponder <UIApplicationDelegate, UITabBarControllerDelegate>

@property (strong, nonatomic) UIWindow* window;

@end
