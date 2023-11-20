/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/DepthStencilState.h>

namespace igl {
namespace vulkan {

/// @brief Implements the igl::IDepthStencilState interface
class DepthStencilState final : public IDepthStencilState {
 public:
  explicit DepthStencilState(const DepthStencilStateDesc& desc) : desc_(desc) {}

  const DepthStencilStateDesc& getDepthStencilStateDesc() const {
    return desc_;
  }

 private:
  DepthStencilStateDesc desc_;
};

} // namespace vulkan
} // namespace igl
