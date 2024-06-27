/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <Metal/Metal.h>
#include <igl/Common.h>
#include <igl/ComputePipelineState.h>

namespace igl::metal {

class ComputePipelineState final : public IComputePipelineState {
  friend class Device;

 public:
  explicit ComputePipelineState(id<MTLComputePipelineState> value,
                                MTLComputePipelineReflection* reflection);
  ~ComputePipelineState() override = default;

  IGL_INLINE id<MTLComputePipelineState> get() {
    return value_;
  }

  std::shared_ptr<IComputePipelineReflection> computePipelineReflection() override {
    return nullptr;
  }

 private:
  id<MTLComputePipelineState> value_;
  MTLComputePipelineReflection* reflection_;
};

} // namespace igl::metal
