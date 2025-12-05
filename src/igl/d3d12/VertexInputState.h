/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/VertexInputState.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class VertexInputState final : public IVertexInputState {
 public:
  explicit VertexInputState(const VertexInputStateDesc& desc) : desc_(desc) {}
  ~VertexInputState() override = default;

  const VertexInputStateDesc& getDesc() const { return desc_; }

 private:
  VertexInputStateDesc desc_;
};

} // namespace igl::d3d12
