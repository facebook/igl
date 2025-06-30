/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/command_buffer_allocator/ICommandBufferAllocator.h>

#include <igl/CommandBuffer.h>

namespace iglu::command_buffer_allocator {
ICommandBufferAllocator::CommandBufferScope::CommandBufferScope(
    ICommandBufferAllocator& deviceContext,
    igl::ICommandBuffer& commandBuffer,
    bool shouldFinalizeCommandBuffer) noexcept :
  deviceContext_(deviceContext),
  commandBuffer_(commandBuffer),
  shouldFinalizeCommandBuffer_(shouldFinalizeCommandBuffer) {}

ICommandBufferAllocator::CommandBufferScope::~CommandBufferScope() noexcept {
  if (shouldFinalizeCommandBuffer_) {
    deviceContext_.finalizeCommandBuffer();
  }
}

igl::ICommandBuffer& ICommandBufferAllocator::CommandBufferScope::commandBuffer() noexcept {
  return commandBuffer_;
}

const igl::ICommandBuffer& ICommandBufferAllocator::CommandBufferScope::commandBuffer()
    const noexcept {
  return commandBuffer_;
}

} // namespace iglu::command_buffer_allocator
