/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/RenderPipelineState.h>

using namespace igl;

namespace igl::metal {

RenderPipelineState::RenderPipelineState(id<MTLRenderPipelineState> value,
                                         MTLRenderPipelineReflection* reflection,
                                         const RenderPipelineDesc& desc) :
  IRenderPipelineState(desc), value_(value) {
  reflection_ = reflection ? std::make_shared<RenderPipelineReflection>(reflection) : nullptr;
}

std::shared_ptr<IRenderPipelineReflection> RenderPipelineState::renderPipelineReflection() {
  return reflection_;
}

void RenderPipelineState::setRenderPipelineReflection(
    const IRenderPipelineReflection& renderPipelineReflection) {
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
  (void)renderPipelineReflection;
}

int RenderPipelineState::getIndexByName(const igl::NameHandle& name, ShaderStage stage) const {
  if (reflection_ == nullptr) {
    return -1;
  }
  return reflection_->getIndexByName(std::string(name), stage);
}

int RenderPipelineState::getIndexByName(const std::string& name, ShaderStage stage) const {
  if (reflection_ == nullptr) {
    return -1;
  }
  return reflection_->getIndexByName(name, stage);
}

MTLColorWriteMask RenderPipelineState::convertColorWriteMask(ColorWriteMask value) {
  MTLColorWriteMask result = MTLColorWriteMaskNone;
  if (value & ColorWriteBitsRed) {
    result |= MTLColorWriteMaskRed;
  }
  if (value & ColorWriteBitsGreen) {
    result |= MTLColorWriteMaskGreen;
  }
  if (value & ColorWriteBitsBlue) {
    result |= MTLColorWriteMaskBlue;
  }
  if (value & ColorWriteBitsAlpha) {
    result |= MTLColorWriteMaskAlpha;
  }
  return result;
}
} // namespace igl::metal
