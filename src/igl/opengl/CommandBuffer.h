/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/CommandBuffer.h>

namespace igl::opengl {
class IContext;

class CommandBuffer final : public ICommandBuffer,
                            public std::enable_shared_from_this<CommandBuffer> {
 public:
  explicit CommandBuffer(std::shared_ptr<IContext> context, CommandBufferDesc desc);
  ~CommandBuffer() override;

  std::unique_ptr<IRenderCommandEncoder> createRenderCommandEncoder(
      const RenderPassDesc& renderPass,
      const std::shared_ptr<IFramebuffer>& framebuffer,
      const Dependencies& dependencies,
      Result* outResult) override;

  std::unique_ptr<IComputeCommandEncoder> createComputeCommandEncoder() override;

  void present(const std::shared_ptr<ITexture>& surface) const override;

  void waitUntilScheduled() override;

  void waitUntilCompleted() override;

  void pushDebugGroupLabel(const char* label, const igl::Color& color) const override;

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

  IContext& getContext() const;

 private:
  std::shared_ptr<IContext> context_;
};

} // namespace igl::opengl
