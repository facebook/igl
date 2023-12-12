/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/RenderPipelineState.h>

using namespace igl;

namespace igl {
namespace metal {

RenderPipelineState::RenderPipelineState(id<MTLRenderPipelineState> value,
                                         MTLRenderPipelineReflection* reflection,
                                         const RenderPipelineDesc& desc) :
  value_(value) {
  desc_ = desc;
  reflection_ = reflection ? std::make_shared<RenderPipelineReflection>(reflection) : nullptr;
}

std::shared_ptr<IRenderPipelineReflection> RenderPipelineState::renderPipelineReflection() {
  return reflection_;
}

void RenderPipelineState::setRenderPipelineReflection(
    const IRenderPipelineReflection& renderPipelineReflection) {
  IGL_ASSERT_NOT_IMPLEMENTED();
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

MTLColorWriteMask RenderPipelineState::convertColorWriteMask(ColorWriteBits value) {
  MTLColorWriteMask result = MTLColorWriteMaskNone;
  if (value & EnumToValue(ColorWriteMask::Red)) {
    result |= MTLColorWriteMaskRed;
  }
  if (value & EnumToValue(ColorWriteMask::Green)) {
    result |= MTLColorWriteMaskGreen;
  }
  if (value & EnumToValue(ColorWriteMask::Blue)) {
    result |= MTLColorWriteMaskBlue;
  }
  if (value & EnumToValue(ColorWriteMask::Alpha)) {
    result |= MTLColorWriteMaskAlpha;
  }
  return result;
}
} // namespace metal
} // namespace igl
