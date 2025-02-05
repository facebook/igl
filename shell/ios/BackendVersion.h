/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

// NOTE: This is a pure Obj-C compatible header (no C++) to simplify bridging with Swift

#import <Foundation/NSObject.h>

// MUST match igl/Common.h
typedef NS_ENUM(NSUInteger, BackendFlavor) {
  BackendFlavorInvalid,
  BackendFlavorOpenGL,
  BackendFlavorOpenGL_ES,
  BackendFlavorMetal,
  BackendFlavorVulkan,
  // @fb-only
};

@interface BackendVersion : NSObject
- (instancetype)init:(BackendFlavor)flavor
        majorVersion:(UInt8)majorVersion
        minorVersion:(UInt8)minorVersion;

@property (readonly) BackendFlavor flavor;
@property (readonly) UInt8 majorVersion;
@property (readonly) UInt8 minorVersion;
@end
