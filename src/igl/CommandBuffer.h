/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Buffer.h>
#include <igl/Common.h>
#include <igl/Framebuffer.h>
#include <igl/RenderCommandEncoder.h>

namespace igl {

class IComputePipelineState;
class ISamplerState;
class ITexture;
struct RenderPassDesc;

class ICommandBuffer {
 public:
  virtual ~ICommandBuffer() = default;

  virtual std::unique_ptr<IRenderCommandEncoder> createRenderCommandEncoder(
      const RenderPassDesc& renderPass,
      std::shared_ptr<IFramebuffer> framebuffer,
      Result* outResult) = 0;
  // Use an overload here instead of a default parameter in a pure virtual function.
  std::unique_ptr<IRenderCommandEncoder> createRenderCommandEncoder(
      const RenderPassDesc& renderPass,
      std::shared_ptr<IFramebuffer> framebuffer) {
    return createRenderCommandEncoder(renderPass, std::move(framebuffer), nullptr);
  }

  virtual void present(std::shared_ptr<ITexture> surface) const = 0;
  virtual void waitUntilCompleted() = 0;

  virtual void pushDebugGroupLabel(const std::string& label,
                                   const igl::Color& color = igl::Color(1, 1, 1, 1)) const = 0;
  virtual void insertDebugEventLabel(const std::string& label,
                                     const igl::Color& color = igl::Color(1, 1, 1, 1)) const = 0;
  virtual void popDebugGroupLabel() const = 0;

  virtual void useComputeTexture(const std::shared_ptr<ITexture>& texture) = 0;
  virtual void bindPushConstants(size_t offset, const void* data, size_t length) = 0;
  virtual void bindComputePipelineState(
      const std::shared_ptr<IComputePipelineState>& pipelineState) = 0;
  virtual void dispatchThreadGroups(const Dimensions& threadgroupCount) = 0;
};

} // namespace igl
