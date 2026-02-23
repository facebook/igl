/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "BackendVersion.h"

@interface BackendVersion () {
  BackendFlavor _flavor;
  UInt8 _majorVersion;
  UInt8 _minorVersion;
}
@end

@implementation BackendVersion

- (instancetype)init:(BackendFlavor)flavor
        majorVersion:(UInt8)majorVersion
        minorVersion:(UInt8)minorVersion {
  if (self = [super init]) {
    self->_flavor = flavor;
    self->_majorVersion = majorVersion;
    self->_minorVersion = minorVersion;
  }
  return self;
}

- (BackendFlavor)flavor {
  return _flavor;
}

- (UInt8)majorVersion {
  return _majorVersion;
}

- (UInt8)minorVersion {
  return _minorVersion;
}

@end
