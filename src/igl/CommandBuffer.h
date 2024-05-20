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

/**
 * Struct containing data about the command buffer usage. Currently used to track the number of draw
 * calls performed by this command buffer (see specific method usage below).
 */
struct CommandBufferStatistics {
  uint32_t currentDrawCount = 0;
};

/**
 * @brief ICommandBuffer represents an object which accepts and stores commands to be executed on
 * the GPU.
 *
 * Commands can be added to the CommandBuffer using a CommandEncoder; ICommandBuffer can currently
 * be used to create two types of command encoders: RenderCommandEncoders (render commands using
 * fragment and/or vertex shaders) and ComputeCommandEncoders (compute commands using compute
 * shaders).
 *
 * ICommandBuffer::present() schedules the results of the commands encoded in the buffer to be
 * presented on the screen as soon as possible. It should be called after the commands are encoded
 * but before the commands are submitted (via a CommandQueue).
 *
 * ICommandBuffer also includes methods for synchronizing CPU code execution based on when the GPU
 * executes the commands encoded in the CommandBuffer.
 */
class ICommandBuffer {
 public:
  virtual ~ICommandBuffer() = default;

  /**
   * @brief Create a RenderCommandEncoder for encoding rendering commands into this CommandBuffer.
   * @returns a pointer to the RenderCommandEncoder
   */
  virtual std::unique_ptr<IRenderCommandEncoder> createRenderCommandEncoder(
      const RenderPassDesc& renderPass,
      std::shared_ptr<IFramebuffer> framebuffer,
      const Dependencies& dependencies,
      Result* IGL_NULLABLE outResult) = 0;

  // Use an overload here instead of a default parameter in a pure virtual function.
  std::unique_ptr<IRenderCommandEncoder> createRenderCommandEncoder(
      const RenderPassDesc& renderPass,
      std::shared_ptr<IFramebuffer> framebuffer,
      const Dependencies* IGL_NULLABLE dependencies = nullptr) {
    return createRenderCommandEncoder(
        renderPass, std::move(framebuffer), dependencies ? *dependencies : Dependencies{}, nullptr);
  }

  /**
   * @brief Create a ComputeCommandEncoder for encoding compute commands into this CommandBuffer.
   * @returns a pointer to the ComputeCommandEncoder
   */
  virtual std::unique_ptr<IComputeCommandEncoder> createComputeCommandEncoder() = 0;

  /**
   * @brief presents the results of the encoded GPU commands the screen as soon as possible (once
   * the commands have completed executing). Should be called before submitting commands via a
   * CommandQueue.
   * @param surface takes a texture param representing a drawable that depends on the results of the
   * GPU commands. Note: this param is unused when using the OpenGL backend.
   */
  virtual void present(std::shared_ptr<ITexture> surface) const = 0;

  /**
   * @brief Blocks execution of the current thread until the commands encoded in this
   * CommandBuffer have been scheduled for execution.
   */
  virtual void waitUntilScheduled() = 0;

  /**
   * @brief Blocks execution of the current thread until the commands encoded in this CommandBuffer
   * have been executed on the GPU.
   */
  virtual void waitUntilCompleted() = 0;

  /**
   * @brief Pushes a debug label onto a stack of debug string labels into the captured frame data.
   *
   * If supported by the backend GPU driver, this allows you to easily associate subsequent
   * commands in the captured call stack with this label.
   *
   * When all commands for this label have been sent to the encoder, call popDebugGroupLabel()
   * to pop the label off the stack.
   */
  virtual void pushDebugGroupLabel(const char* IGL_NONNULL label,
                                   const igl::Color& color = igl::Color(1, 1, 1, 1)) const = 0;

  /**
   * @brief Pops a most recent debug label off a stack of debug string labels.
   *
   * This should be preceded by pushDebugGroupLabel().
   */
  virtual void popDebugGroupLabel() const = 0;

  /**
   * @returns the number of draw operations tracked by this CommandBuffer. This is tracked manually
   * via calls to incrementCurrentDrawCount().
   */
  uint32_t getCurrentDrawCount() const {
    return statistics_.currentDrawCount;
  }
  /**
   * @brief Increment a counter representing the number of draw operations tracked by this
   * CommandBuffer.
   */
  void incrementCurrentDrawCount() {
    statistics_.currentDrawCount++;
  }

 private:
  CommandBufferStatistics statistics_;
};

} // namespace igl
