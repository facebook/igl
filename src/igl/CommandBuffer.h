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
class ITimer;
struct RenderPassDesc;

/**
 * Currently a no-op structure.
 */
struct CommandBufferDesc {
  std::string debugName;
  std::shared_ptr<ITimer> timer;
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
  explicit ICommandBuffer(CommandBufferDesc iDesc) : desc(std::move(iDesc)) {}
  virtual ~ICommandBuffer() = default;

  /**
   * @brief Create a RenderCommandEncoder for encoding rendering commands into this CommandBuffer.
   * @returns a pointer to the RenderCommandEncoder
   */
  virtual std::unique_ptr<IRenderCommandEncoder> createRenderCommandEncoder(
      const RenderPassDesc& renderPass,
      const std::shared_ptr<IFramebuffer>& framebuffer,
      const Dependencies& dependencies,
      Result* IGL_NULLABLE outResult = nullptr) = 0;

  std::unique_ptr<IRenderCommandEncoder> createRenderCommandEncoder(
      const RenderPassDesc& renderPass,
      const std::shared_ptr<IFramebuffer>& framebuffer,
      Result* IGL_NULLABLE outResult = nullptr) {
    return createRenderCommandEncoder(renderPass, framebuffer, Dependencies(), outResult);
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
  virtual void present(const std::shared_ptr<ITexture>& surface) const = 0;

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
                                   const igl::Color& color = Color(1, 1, 1, 1)) const = 0;

  /**
   * @brief Pops a most recent debug label off a stack of debug string labels.
   *
   * This should be preceded by pushDebugGroupLabel().
   */
  virtual void popDebugGroupLabel() const = 0;

  /**
   * @brief Copy data between buffers.
   */
  virtual void copyBuffer(IBuffer& src,
                          IBuffer& dst,
                          uint64_t srcOffset,
                          uint64_t dstOffset,
                          uint64_t size) = 0;
  /**
   * @brief Copy texture data into a buffer.
   */
  virtual void copyTextureToBuffer(ITexture& src,
                                   IBuffer& dst,
                                   uint64_t dstOffset,
                                   uint32_t level = 0,
                                   uint32_t layer = 0) = 0;

  /**
   * @returns the number of draw operations tracked by this CommandBuffer. This is tracked manually
   * via calls to incrementCurrentDrawCount().
   */
  [[nodiscard]] uint32_t getCurrentDrawCount() const {
    return statistics_.currentDrawCount;
  }
  /**
   * @brief Increment a counter representing the number of draw operations tracked by this
   * CommandBuffer.
   */
  void incrementCurrentDrawCount() {
    statistics_.currentDrawCount++;
  }

  const CommandBufferDesc desc;
  const std::shared_ptr<ITimer> timer;

 private:
  CommandBufferStatistics statistics_;

  friend class ICommandQueue;
};

} // namespace igl
