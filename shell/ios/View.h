/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import <MetalKit/MetalKit.h>
#import <UIKit/UIKit.h>

@protocol TouchDelegate <NSObject>
- (void)touchBegan:(UITouch*)touch;
- (void)touchEnded:(UITouch*)touch;
- (void)touchMoved:(UITouch*)touch;
@end

@protocol ViewSizeChangeDelegate <NSObject>
- (void)onViewSizeChange;
@end

@interface BaseView : UIView
- (instancetype)initWithTouchDelegate:(id<TouchDelegate>)delegate;
@end

@interface MetalView : MTKView
- (void)setTouchDelegate:(id<TouchDelegate>)delegate;
@end

@interface OpenGLView : BaseView
@property (nonatomic, weak, nullable) id<ViewSizeChangeDelegate> viewSizeChangeDelegate;
@end
