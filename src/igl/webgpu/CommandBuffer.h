/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <igl/CommandBuffer.h>
#include <igl/webgpu/Common.h>

namespace igl::webgpu {

class Buffer;
class WebGPUContext;

/// @brief Implements the igl::ICommandBuffer interface for WebGPU with one WGPUCommandEncoder.
class CommandBuffer final : public ICommandBuffer {
 public:
  CommandBuffer(WebGPUContext& ctx, CommandBufferDesc desc);
  ~CommandBuffer() override;

  CommandBuffer(const CommandBuffer&) = delete;
  CommandBuffer& operator=(const CommandBuffer&) = delete;
  CommandBuffer(CommandBuffer&&) = delete;
  CommandBuffer& operator=(CommandBuffer&&) = delete;

  std::unique_ptr<IRenderCommandEncoder> createRenderCommandEncoder(
      const RenderPassDesc& renderPass,
      const std::shared_ptr<IFramebuffer>& framebuffer,
      const Dependencies& dependencies,
      Result* IGL_NULLABLE outResult) override;
  std::unique_ptr<IComputeCommandEncoder> createComputeCommandEncoder() override;

  void present(const std::shared_ptr<ITexture>& surface) const override;
  void waitUntilScheduled() override;
  void waitUntilCompleted() override;

  void pushDebugGroupLabel(const char* IGL_NONNULL label, const Color& color) const override;
  void popDebugGroupLabel() const override;

  void copyBuffer(IBuffer& src,
                  IBuffer& dst,
                  uint64_t srcOffset,
                  uint64_t dstOffset,
                  uint64_t size) override;
  void copyTextureToBuffer(ITexture& src,
                           IBuffer& dst,
                           uint64_t dstOffset,
                           uint32_t level,
                           uint32_t layer) override;

  [[nodiscard]] WGPUCommandEncoder IGL_NULLABLE getWGPUCommandEncoder() const noexcept {
    return encoder_.get();
  }
  /// Serial of this command buffer in the context's ResourceTracker.
  [[nodiscard]] uint64_t getSerial() const noexcept {
    return serial_;
  }
  [[nodiscard]] bool isSubmitted() const noexcept {
    return submitted_;
  }
  /// Finishes the encoder and submits it to the queue; a command buffer is submitted at most once.
  [[nodiscard]] Result submit();

 private:
  // Copies `sources` (one per slice) with rows that are not multiples of 256 bytes.
  [[nodiscard]] bool copyUnalignedRowsToBuffer(const std::vector<WGPUTexelCopyTextureInfo>& sources,
                                               const WGPUExtent3D& extent,
                                               uint64_t tightBytesPerRow,
                                               Buffer& buffer,
                                               uint64_t dstOffset);

  WebGPUContext& ctx_;
  Handle<WGPUCommandEncoder> encoder_;
  const uint64_t serial_;
  bool submitted_ = false;
};

} // namespace igl::webgpu
