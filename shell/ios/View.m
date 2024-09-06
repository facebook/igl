/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "View.h"

#import <QuartzCore/CAMetalLayer.h>

@implementation BaseView {
  id<TouchDelegate> __weak _delegate;
}

- (instancetype)initWithTouchDelegate:(id<TouchDelegate>)delegate {
  if (self = [super init]) {
    _delegate = delegate;
  }
  return self;
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [_delegate touchBegan:touches.anyObject];
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(nullable UIEvent*)event {
  [_delegate touchMoved:touches.anyObject];
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(nullable UIEvent*)event {
  [_delegate touchEnded:touches.anyObject];
}

- (void)touchesCanceled:(NSSet<UITouch*>*)touches withEvent:(nullable UIEvent*)event {
  [_delegate touchEnded:touches.anyObject];
}

@end

@implementation MetalView {
  id<TouchDelegate> __weak _touchDelegate;
}

- (void)setTouchDelegate:(id<TouchDelegate>)delegate {
  _touchDelegate = delegate;
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [_touchDelegate touchBegan:touches.anyObject];
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(nullable UIEvent*)event {
  [_touchDelegate touchMoved:touches.anyObject];
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(nullable UIEvent*)event {
  [_touchDelegate touchEnded:touches.anyObject];
}

- (void)touchesCanceled:(NSSet<UITouch*>*)touches withEvent:(nullable UIEvent*)event {
  [_touchDelegate touchEnded:touches.anyObject];
}

+ (Class)layerClass {
#if TARGET_OS_SIMULATOR
  if (@available(iOS 13.0, *)) {
    return [CAMetalLayer class];
  }
  return [CALayer class];
#else
  return [CAMetalLayer class];
#endif
}

@end

@implementation OpenGLView

- (void)layoutSubviews {
  [super layoutSubviews];
  if (self.viewSizeChangeDelegate) {
    [self.viewSizeChangeDelegate onViewSizeChange];
  }
}

+ (Class)layerClass {
  return [CAEAGLLayer class];
}

@end
