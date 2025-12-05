/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/DepthStencilState.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class DepthStencilState final : public IDepthStencilState {
 public:
  explicit DepthStencilState(const DepthStencilStateDesc& desc) : desc_(desc) {}
  ~DepthStencilState() override = default;

  const DepthStencilStateDesc& getDesc() const { return desc_; }

 private:
  DepthStencilStateDesc desc_;
};

} // namespace igl::d3d12
