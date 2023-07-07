/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <Metal/Metal.h>
#include <igl/CommandBuffer.h>

namespace igl {
namespace metal {

class CommandBuffer final : public ICommandBuffer,
                            public std::enable_shared_from_this<CommandBuffer> {
 public:
  explicit CommandBuffer(id<MTLCommandBuffer> value);
  ~CommandBuffer() override = default;

  std::unique_ptr<IComputeCommandEncoder> createComputeCommandEncoder() override;

  std::unique_ptr<IRenderCommandEncoder> createRenderCommandEncoder(
      const RenderPassDesc& renderPass,
      std::shared_ptr<IFramebuffer> framebuffer,
      Result* outResult) override;

  void present(std::shared_ptr<ITexture> surface) const override;

  void pushDebugGroupLabel(const std::string& label, const igl::Color& color) const override;

  void popDebugGroupLabel() const override;

  void waitUntilScheduled() override;

  void waitUntilCompleted() override;

  IGL_INLINE id<MTLCommandBuffer> get() const {
    return value_;
  }

 private:
  id<MTLCommandBuffer> value_;
};

} // namespace metal
} // namespace igl
