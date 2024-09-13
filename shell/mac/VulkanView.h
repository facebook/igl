/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import <AppKit/AppKit.h>

#include <memory>
#include <shell/shared/platform/Platform.h>

@interface VulkanView : NSView {
}
- (void)startTimer;
- (void)stopTimer;
- (void)prepareVulkan:(std::shared_ptr<igl::shell::Platform>)platform;
- (void)viewDidChangeBackingProperties;
- (void)frameDidChange:(NSNotification*)notification;
- (void)updateSwapchain;
@end
