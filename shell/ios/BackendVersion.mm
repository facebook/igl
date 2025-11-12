/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "BackendVersion.h"

@interface BackendVersion () {
  BackendFlavor flavor;
  UInt8 majorVersion;
  UInt8 minorVersion;
}
@end

@implementation BackendVersion

- (instancetype)init:(BackendFlavor)flavor
        majorVersion:(UInt8)majorVersion
        minorVersion:(UInt8)minorVersion {
  if (self = [super init]) {
    self->flavor = flavor;
    self->majorVersion = majorVersion;
    self->minorVersion = minorVersion;
  }
  return self;
}

- (BackendFlavor)flavor {
  return flavor;
}

- (UInt8)majorVersion {
  return majorVersion;
}

- (UInt8)minorVersion {
  return minorVersion;
}

@end
