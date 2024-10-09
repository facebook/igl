/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "RenderSessionFactoryProvider.h"
#import <MetalKit/MetalKit.h>
#import <UIKit/UIKit.h>
#import <shell/shared/renderSession/RenderSessionConfig.h>

@interface ViewController : UIViewController <MTKViewDelegate>

- (instancetype)init:(igl::shell::RenderSessionConfig)config
     factoryProvider:(RenderSessionFactoryProvider*)factoryProvider
               frame:(CGRect)frame;

@end
