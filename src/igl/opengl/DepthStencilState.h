/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/DepthStencilState.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>

namespace igl {
class ICommandBuffer;

namespace opengl {

class DepthStencilState final : public WithContext, public IDepthStencilState {
  friend class igl::opengl::Device;

 public:
  explicit DepthStencilState(IContext& context);
  ~DepthStencilState() override = default;

  Result create(const DepthStencilStateDesc& desc);
  void bind(uint32_t frontStencilReferenceValue, uint32_t backStencilReferenceValue);
  void unbind();

  static GLenum convertCompareFunction(igl::CompareFunction value);
  static GLenum convertStencilOperation(igl::StencilOperation value);

 private:
  DepthStencilStateDesc desc_;
};

} // namespace opengl
} // namespace igl
