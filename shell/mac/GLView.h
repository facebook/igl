/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import <AppKit/AppKit.h>

// NOLINTNEXTLINE(clang-diagnostic-deprecated-declarations)
@interface GLView : NSOpenGLView {
}
- (void)startTimer;
- (void)stopTimer;
@end
