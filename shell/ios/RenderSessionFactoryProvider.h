/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

// NOTE: This is a pure Obj-C compatible header (no C++) to simplify bridging with Swift

#import <Foundation/NSObject.h>

#import "RenderSessionFactoryAdapter.h"

typedef int IglBackendFlavor;
typedef int IglOpenglRenderingAPI;

@protocol RenderSessionFactoryAdapter <NSObject>
- (RenderSessionFactoryAdapterPtr)adapter;
@end

@interface RenderSessionFactoryProvider : NSObject <RenderSessionFactoryAdapter>
- (instancetype)init;
@end
