/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import <UIKit/UIApplication.h>
#import <UIKit/UIResponder.h>

#import <shell/shared/platform/Platform.h>

#import "AppDelegate.h"

int main(int argc, char* argv[]) {
  igl::shell::Platform::initializeCommandLineArgs(argc, argv);

  @autoreleasepool {
    return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
  }
}
