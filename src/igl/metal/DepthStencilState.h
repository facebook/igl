/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <Metal/Metal.h>
#include <igl/DepthStencilState.h>

namespace igl::metal {

class DepthStencilState final : public IDepthStencilState {
  friend class Device;

 public:
  IGL_INLINE id<MTLDepthStencilState> get() const {
    return value_;
  }
  DepthStencilState(id<MTLDepthStencilState> value);
  ~DepthStencilState() override = default;
  static MTLCompareFunction convertCompareFunction(CompareFunction value);
  static MTLStencilOperation convertStencilOperation(StencilOperation value);
  static MTLStencilDescriptor* convertStencilDescriptor(const StencilStateDesc& desc);

  id<MTLDepthStencilState> value_;
};

} // namespace igl::metal
