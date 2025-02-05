/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "RenderSessionFactoryProvider.h"

#import "RenderSessionFactoryAdapterInternal.hpp"
#include <memory>
#include <shell/shared/renderSession/DefaultRenderSessionFactory.h>

@interface RenderSessionFactoryProvider () {
  std::unique_ptr<igl::shell::IRenderSessionFactory> factory_;
  RenderSessionFactoryAdapter adapter_;
}
@end

@implementation RenderSessionFactoryProvider

- (instancetype)init {
  if (self = [super init]) {
    factory_ = igl::shell::createDefaultRenderSessionFactory();
    adapter_.factory = factory_.get();
  }
  return self;
}

// @protocol RenderSessionFactoryAdapter
- (RenderSessionFactoryAdapterPtr)adapter {
  return &adapter_;
}

@end
