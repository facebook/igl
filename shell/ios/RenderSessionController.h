/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// NOTE: This is a pure Obj-C compatible header (no C++) to simplify bridging with Swift

#import <CoreGraphics/CGGeometry.h>
#import <Foundation/NSObject.h>

#import "IglShellPlatformAdapter.h"
#import "IglSurfaceTexturesAdapter.h"

typedef int IglBackendType;
typedef int IglOpenglRenderingAPI;

@protocol IglSurfaceTexturesProvider <NSObject>
- (IglSurfacesTextureAdapterPtr)createSurfaceTextures;
@end

@protocol IglShellPlatformAdapter <NSObject>
- (IglShellPlatformAdapterPtr)adapter;
@end

@interface RenderSessionController : NSObject <IglShellPlatformAdapter>
- (instancetype)initWithIglBackend:(IglBackendType)backendType
                   surfaceProvider:(id<IglSurfaceTexturesProvider>)provider;

#if IGL_BACKEND_OPENGL
- (instancetype)initWithOpenGLRenderingAPI:(IglOpenglRenderingAPI)renderingAPI
                           surfaceProvider:(id<IglSurfaceTexturesProvider>)provider;
#endif

- (void)initializeDevice;

- (void)start;
- (void)stop;
- (void)tick;

- (void)setFrame:(CGRect)frame;
@end
