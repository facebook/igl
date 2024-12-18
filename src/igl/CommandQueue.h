/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>

namespace igl {

// Forward struct declarations
// Forward class declarations
struct CommandBufferDesc;
class ICommandBuffer;

/**
 * This is a placeholder for future use
 */
struct CommandQueueDesc {};

/**
 * Contains the current frame's draw count and last frame's draw count.
 * ICommandQueue controls these values and moves current draw count to last frame draw count through
 * the endFrame function.
 */
struct CommandQueueStatistics {
  uint32_t currentDrawCount = 0;
  uint32_t lastFrameDrawCount = 0;
};

/// GPU Fence Handle
using SubmitHandle = uint64_t;

/**
 * Overarching structure used to create specific command buffers that accept device commands.
 * There are three different command queue types: compute, graphics, and memory transfer.
 * The key operations command queue provides are to create a command buffer to accept commands,
 * through the createCommandBuffer function, and then a submit command buffer which accepts those
 * commands into the queue through the submit function.  Simple draw statistics are tracked for
 * current and last frame which can be interacted with through the getLastFrameDrawCount and
 * endFrame functions.
 */
class ICommandQueue {
 public:
  virtual ~ICommandQueue() = default;
  virtual std::shared_ptr<ICommandBuffer> createCommandBuffer(const CommandBufferDesc& desc,
                                                              Result* IGL_NULLABLE outResult) = 0;
  virtual SubmitHandle submit(const ICommandBuffer& commandBuffer, bool endOfFrame = false) = 0;
  [[nodiscard]] uint32_t getLastFrameDrawCount() const {
    return statistics.lastFrameDrawCount;
  }
  void endFrame() {
    statistics.lastFrameDrawCount = statistics.currentDrawCount;
    statistics.currentDrawCount = 0;
  }

 protected:
  void incrementDrawCount(uint32_t newDrawCount) {
    statistics.currentDrawCount += newDrawCount;
  }

 private:
  CommandQueueStatistics statistics;
};

} // namespace igl
