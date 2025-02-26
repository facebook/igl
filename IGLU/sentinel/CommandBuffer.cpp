/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#include <IGLU/sentinel/CommandBuffer.h>

#include <IGLU/sentinel/Assert.h>
#include <igl/IGL.h>

namespace iglu::sentinel {

CommandBuffer::CommandBuffer(bool shouldAssert) : shouldAssert_(shouldAssert) {}

std::unique_ptr<igl::IRenderCommandEncoder> CommandBuffer::createRenderCommandEncoder(
    const igl::RenderPassDesc& /*renderPass*/,
    const std::shared_ptr<igl::IFramebuffer>& /*framebuffer*/,
    const igl::Dependencies& /*dependencies*/,
    igl::Result* IGL_NULLABLE /*outResult*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

std::unique_ptr<igl::IComputeCommandEncoder> CommandBuffer::createComputeCommandEncoder() {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

void CommandBuffer::present(const std::shared_ptr<igl::ITexture>& /*surface*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

void CommandBuffer::waitUntilScheduled() {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

void CommandBuffer::waitUntilCompleted() {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

void CommandBuffer::pushDebugGroupLabel(const char* /*label*/, const igl::Color& /*color*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

void CommandBuffer::popDebugGroupLabel() const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

void CommandBuffer::copyBuffer(igl::IBuffer& src,
                               igl::IBuffer& dst,
                               uint64_t srcOffset,
                               uint64_t dstOffset,
                               uint64_t size) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

} // namespace iglu::sentinel
