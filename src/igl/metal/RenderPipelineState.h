/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <Metal/Metal.h>
#include <igl/Common.h>
#include <igl/RenderPipelineState.h>
#include <igl/metal/RenderPipelineReflection.h>

namespace igl::metal {

class RenderPipelineState final : public IRenderPipelineState {
  friend class Device;

 public:
  explicit RenderPipelineState(id<MTLRenderPipelineState> value,
                               MTLRenderPipelineReflection* reflection,
                               const RenderPipelineDesc& desc);
  ~RenderPipelineState() override = default;

  IGL_INLINE id<MTLRenderPipelineState> get() {
    return value_;
  }
  [[nodiscard]] int getIndexByName(const igl::NameHandle& name, ShaderStage stage) const override;
  [[nodiscard]] int getIndexByName(const std::string& name, ShaderStage stage) const override;

  std::shared_ptr<IRenderPipelineReflection> renderPipelineReflection() override;
  void setRenderPipelineReflection(
      const IRenderPipelineReflection& renderPipelineReflection) override;

  [[nodiscard]] CullMode getCullMode() const {
    return desc_.cullMode;
  }
  [[nodiscard]] WindingMode getWindingMode() const {
    return desc_.frontFaceWinding;
  }
  [[nodiscard]] PolygonFillMode getPolygonFillMode() const {
    return desc_.polygonFillMode;
  }
  static MTLColorWriteMask convertColorWriteMask(ColorWriteMask value);

 private:
  id<MTLRenderPipelineState> value_;
  std::shared_ptr<RenderPipelineReflection> reflection_;
};

} // namespace igl::metal
