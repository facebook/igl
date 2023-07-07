/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>
#import <igl/Common.h>

@interface ViewController : NSViewController <MTKViewDelegate>

@property (nonatomic) NSView* iglView;

- (instancetype)initWithFrame:(CGRect)frame
                  backendType:(igl::BackendType)backendType
          preferLatestVersion:(bool)preferLatestVersion NS_DESIGNATED_INITIALIZER;

// For openGL only
- (instancetype)initWithFrame:(CGRect)frame
                  backendType:(igl::BackendType)backendType
                 majorVersion:(int)majorVersion
                 minorVersion:(int)minorVersion NS_DESIGNATED_INITIALIZER;

- (void)initModule;
- (void)render;
- (void)teardown;
- (CGRect)frame;
@end
