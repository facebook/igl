/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/VertexInputState.h>

namespace igl::vulkan {

class Device;
class RenderCommandEncoder;

/// @brief Implements the igl::IVertexInputState interface
class VertexInputState final : public IVertexInputState {
 public:
  explicit VertexInputState(VertexInputStateDesc desc) : desc_(std::move(desc)) {}
  ~VertexInputState() override = default;

  friend class Device;
  friend class RenderCommandEncoder;
  friend class RenderPipelineState;

 private:
  VertexInputStateDesc desc_;
};

} // namespace igl::vulkan
