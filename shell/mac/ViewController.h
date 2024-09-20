/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>
#import <igl/Common.h>
#import <igl/DeviceFeatures.h>
#import <shell/shared/renderSession/IRenderSessionFactory.h>

NS_ASSUME_NONNULL_BEGIN
@interface ViewController : NSViewController <MTKViewDelegate>

@property (nonatomic) NSView* iglView;

- (instancetype)initWithFrame:(CGRect)frame
                      factory:(igl::shell::IRenderSessionFactory&)factory
                       config:(igl::shell::RenderSessionConfig)config;

// Explicitly disable superclass' designated initializers
- (instancetype)initWithNibName:(nullable NSNibName)nibNameOrNil
                         bundle:(nullable NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (nullable instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

- (void)initModule;
- (void)render;
- (void)teardown;
- (CGRect)frame;
- (igl::ColorSpace)colorSpace;
@end
NS_ASSUME_NONNULL_END
