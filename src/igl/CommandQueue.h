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
 * Enumeration used in CommandQueueDesc to create a command queue of the correct type.
 * Options are Compute, Graphics, and MemoryTransfer which directly correct to graphics libraries
 * standard queues.
 */
enum class CommandQueueType {
  Compute, /// Supports Compute commands
  Graphics, /// Supports Graphics commands
  Transfer, /// Supports Memory commands
};

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
                                                              Result* outResult) = 0;
  virtual void submit(const ICommandBuffer& commandBuffer, bool endOfFrame = false) = 0;
};

} // namespace igl
