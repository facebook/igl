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

class IComputeCommandEncoder;
class ISamplerState;
class ITexture;
struct RenderPassDesc;

/**
 * Currently a no-op structure.
 */
struct CommandBufferDesc {
  std::string debugName;
};

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

  virtual std::unique_ptr<IComputeCommandEncoder> createComputeCommandEncoder() = 0;
  virtual void present(std::shared_ptr<ITexture> surface) const = 0;
  virtual void waitUntilCompleted() = 0;
};

} // namespace igl
