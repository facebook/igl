/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import <MetalKit/MetalKit.h>
#import <UIKit/UIKit.h>
#import <igl/IGL.h>
#if IGL_BACKEND_OPENGL
#import <igl/opengl/IContext.h>
#endif

@interface ViewController : UIViewController <MTKViewDelegate>

- (instancetype)initForMetal:(CGRect)frame;
#if IGL_BACKEND_OPENGL
- (instancetype)initForOpenGL:(igl::opengl::RenderingAPI)renderingAPI frame:(CGRect)frame;
#endif

@end
