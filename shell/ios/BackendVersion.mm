/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "BackendVersion.h"

@interface BackendVersion () {
  BackendFlavor flavor_;
  UInt8 majorVersion_;
  UInt8 minorVersion_;
}
@end

@implementation BackendVersion

- (instancetype)init:(BackendFlavor)flavor
        majorVersion:(UInt8)majorVersion
        minorVersion:(UInt8)minorVersion {
  if (self = [super init]) {
    flavor_ = flavor;
    majorVersion_ = majorVersion;
    minorVersion_ = minorVersion;
  }
  return self;
}

- (BackendFlavor)flavor {
  return flavor_;
}

- (UInt8)majorVersion {
  return majorVersion_;
}

- (UInt8)minorVersion {
  return minorVersion_;
}

@end
