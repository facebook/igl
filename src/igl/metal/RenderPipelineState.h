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

namespace igl {
namespace metal {

class RenderPipelineState final : public IRenderPipelineState {
  friend class Device;

 public:
  explicit RenderPipelineState(id<MTLRenderPipelineState> value,
                               MTLRenderPipelineReflection* reflection,
                               igl::CullMode cullMode,
                               igl::WindingMode frontFaceWinding,
                               igl::PolygonFillMode polygonFillMode);
  ~RenderPipelineState() override = default;

  IGL_INLINE id<MTLRenderPipelineState> get() {
    return value_;
  }
  int getIndexByName(const igl::NameHandle& name, ShaderStage stage) const override;
  int getIndexByName(const std::string& name, ShaderStage stage) const override;

  std::shared_ptr<IRenderPipelineReflection> renderPipelineReflection() override;
  void setRenderPipelineReflection(
      const IRenderPipelineReflection& renderPipelineReflection) override;

  CullMode getCullMode() const {
    return cullMode_;
  }
  WindingMode getWindingMode() const {
    return frontFaceWinding_;
  }
  PolygonFillMode getPolygonFillMode() const {
    return polygonFillMode_;
  }
  static MTLColorWriteMask convertColorWriteMask(ColorWriteBits value);

 private:
  id<MTLRenderPipelineState> value_;
  std::shared_ptr<RenderPipelineReflection> reflection_;
  CullMode cullMode_{igl::CullMode::Back};
  WindingMode frontFaceWinding_{igl::WindingMode::CounterClockwise};
  PolygonFillMode polygonFillMode_{igl::PolygonFillMode::Fill};
};

} // namespace metal
} // namespace igl
