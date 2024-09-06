/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import <MetalKit/MetalKit.h>
#import <UIKit/UIKit.h>
#import <igl/IGL.h>

@interface ViewController : UIViewController <MTKViewDelegate>

- (instancetype)init:(igl::BackendVersion)backendVersion frame:(CGRect)frame;

@end
