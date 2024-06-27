/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <Metal/Metal.h>
#include <igl/VertexInputState.h>

namespace igl::metal {

class VertexInputState final : public IVertexInputState {
  friend class Device;

 public:
  explicit VertexInputState(MTLVertexDescriptor* value);
  ~VertexInputState() override = default;

  IGL_INLINE MTLVertexDescriptor* get() const {
    return value_;
  }

  static MTLVertexFormat convertAttributeFormat(VertexAttributeFormat value);
  static MTLVertexStepFunction convertSampleFunction(VertexSampleFunction value);

  MTLVertexDescriptor* value_;
};

} // namespace igl::metal
