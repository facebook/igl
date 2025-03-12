/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/CommandBuffer.h>

namespace iglu::sentinel {

/**
 * Sentinel CommandBuffer intended for safe use where access to a real command buffer is not
 * available.
 * Use cases include returning a reference to a command buffer from a raw pointer when a
 * valid command buffer is not available.
 * All methods return nullptr, the default value or an error.
 */
class CommandBuffer final : public igl::ICommandBuffer {
 public:
  explicit CommandBuffer(bool shouldAssert = true);

  [[nodiscard]] std::unique_ptr<igl::IRenderCommandEncoder> createRenderCommandEncoder(
      const igl::RenderPassDesc& /*renderPass*/,
      const std::shared_ptr<igl::IFramebuffer>& /*framebuffer*/,
      const igl::Dependencies& /*dependencies*/,
      igl::Result* IGL_NULLABLE /*outResult*/) final;
  [[nodiscard]] std::unique_ptr<igl::IComputeCommandEncoder> createComputeCommandEncoder() final;
  void present(const std::shared_ptr<igl::ITexture>& /*surface*/) const final;
  void waitUntilScheduled() final;
  void waitUntilCompleted() final;
  void pushDebugGroupLabel(const char* IGL_NONNULL /*label*/,
                           const igl::Color& /*color*/ = igl::Color(1, 1, 1, 1)) const final;
  void popDebugGroupLabel() const final;
  void copyBuffer(igl::IBuffer& src,
                  igl::IBuffer& dst,
                  uint64_t srcOffset,
                  uint64_t dstOffset,
                  uint64_t size) final;
  void copyTextureToBuffer(igl::ITexture& src,
                           igl::IBuffer& dst,
                           uint64_t dstOffset,
                           uint32_t level,
                           uint32_t layer) final;

 private:
  [[maybe_unused]] bool shouldAssert_;
};

} // namespace iglu::sentinel
