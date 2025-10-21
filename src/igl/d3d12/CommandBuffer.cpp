/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/CommandBuffer.h>

namespace igl::d3d12 {

std::unique_ptr<IRenderCommandEncoder> CommandBuffer::createRenderCommandEncoder(
    const RenderPassDesc& /*renderPass*/,
    const std::shared_ptr<IFramebuffer>& /*framebuffer*/,
    const Dependencies& /*dependencies*/,
    Result* IGL_NULLABLE outResult) {
  Result::setResult(outResult, Result::Code::Unimplemented, "D3D12 RenderCommandEncoder not yet implemented");
  return nullptr;
}

std::unique_ptr<IComputeCommandEncoder> CommandBuffer::createComputeCommandEncoder() {
  return nullptr;
}

void CommandBuffer::present(const std::shared_ptr<ITexture>& /*surface*/) const {
  // Stub: Not yet implemented
}

void CommandBuffer::waitUntilScheduled() {
  // Stub: Not yet implemented
}

void CommandBuffer::waitUntilCompleted() {
  // Stub: Not yet implemented
}

void CommandBuffer::pushDebugGroupLabel(const char* /*label*/,
                                        const igl::Color& /*color*/) const {
  // Stub: Not yet implemented
}

void CommandBuffer::popDebugGroupLabel() const {
  // Stub: Not yet implemented
}

} // namespace igl::d3d12
