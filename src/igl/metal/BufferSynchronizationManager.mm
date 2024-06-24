/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/BufferSynchronizationManager.h>

#include <igl/metal/CommandBuffer.h>

namespace igl::metal {

BufferSynchronizationManager::BufferSynchronizationManager(size_t maxInFlightBuffers) :
  maxInFlightBuffers_(maxInFlightBuffers) {
  // To manage the pool of buffers, we would normally initialize the semaphore with the pool size
  // (i.e. 'maxInFlightBuffers'). However, semaphore crashes at destruction time, if its current
  // value is less than its initial value. This seems to Apple's libdispatch's idiosyncrasy.
  // https://opensource.apple.com/source/libdispatch/libdispatch-187.7/src/semaphore.c

  // To workaround this, we init semaphore to 0 and then call dispatch_semaphore_signal() to
  // increase the value up to the size of buffer pool
  frameBoundarySemaphore_ = dispatch_semaphore_create(0);
  for (size_t i = 0; i < maxInFlightBuffers_; i++) {
    dispatch_semaphore_signal(frameBoundarySemaphore_);
  }
}

void BufferSynchronizationManager::markCommandBufferAsEndOfFrame(
    const igl::ICommandBuffer& commandBuffer) {
  // Set a completion handler for this cmd buffer
  __weak dispatch_semaphore_t semaphore = frameBoundarySemaphore_;
  [static_cast<const CommandBuffer&>(commandBuffer).get()
      addCompletedHandler:^(id<MTLCommandBuffer> mtlCommandBuffer) {
        // GPU work is complete
        // Signal the semaphore to start the CPU work
        // Increment the counting semaphore
        if (semaphore != nullptr) {
          dispatch_semaphore_signal(semaphore);
        }
      }];
}

void BufferSynchronizationManager::manageEndOfFrameSync() {
  // Decrement the counting semaphore and
  // if the resulting value is less than zero, block the current thread from executing further
  // until the semaphore's value is >= 0
  dispatch_semaphore_wait(frameBoundarySemaphore_, DISPATCH_TIME_FOREVER);

  // increment currentInFlightBufferIndex
  currentInFlightBufferIndex_ = (currentInFlightBufferIndex_ + 1) % maxInFlightBuffers_;
}

} // namespace igl::metal
