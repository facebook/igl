/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/VertexInputState.h>

namespace igl::webgpu {

/// @brief A validated vertex input descriptor, compiled into render pipelines.
class VertexInputState final : public IVertexInputState {
 public:
  explicit VertexInputState(const VertexInputStateDesc& desc) : desc_(desc) {}

  [[nodiscard]] const VertexInputStateDesc& getDesc() const noexcept {
    return desc_;
  }

 private:
  VertexInputStateDesc desc_;
};

} // namespace igl::webgpu
