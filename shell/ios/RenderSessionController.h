/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

// NOTE: This is a pure Obj-C compatible header (no C++) to simplify bridging with Swift

#import <CoreGraphics/CGGeometry.h>
#import <Foundation/NSObject.h>

#import "BackendVersion.h"
#import "IglShellPlatformAdapter.h"
#import "IglSurfaceTexturesAdapter.h"
#import "RenderSessionFactoryProvider.h"

typedef int IglBackendFlavor;
typedef int IglOpenglRenderingAPI;

@protocol IglSurfaceTexturesProvider <NSObject>
- (IglSurfacesTextureAdapterPtr)createSurfaceTextures;
@end

@protocol IglShellPlatformAdapter <NSObject>
- (IglShellPlatformAdapterPtr)adapter;
@end

@interface RenderSessionController : NSObject <IglShellPlatformAdapter>
- (instancetype)initWithBackendVersion:(BackendVersion*)backendVersion
                       factoryProvider:(RenderSessionFactoryProvider*)factoryProvider
                       surfaceProvider:(id<IglSurfaceTexturesProvider>)provider;

- (void)initializeDevice;

- (void)start;
- (void)stop;
- (void)tick;
- (void)releaseSessionFrameBuffer;

- (void)setFrame:(CGRect)frame;
@end
