/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/CommandQueue.h>

#include <Foundation/Foundation.h>
#include <igl/metal/BufferSynchronizationManager.h>
#include <igl/metal/CommandBuffer.h>
#include <igl/metal/DeviceStatistics.h>
#include <igl/metal/Timer.h>

// @brief Number of command buffers to be automatically captured for GPU debugging. Zero (0)
// means no command buffers will be recorded and the capture code is deactivated.
constexpr uint32_t kIGLMetalNumberCommandBuffersToCapture = 0;
// @brief Beginning command buffer to be captured
constexpr uint32_t kIGLMetalBeginCommandBufferToCapture = 0;
// @brief Ending command buffer. Only command buffers within [kIGLMetalBeginCommandBufferToCapture,
// kIGLMetalEndCommandBufferToCapture) will be captured.
constexpr uint32_t kIGLMetalEndCommandBufferToCapture =
    kIGLMetalBeginCommandBufferToCapture + kIGLMetalNumberCommandBuffersToCapture;

namespace igl::metal {

CommandQueue::CommandQueue(Device& device,
                           id<MTLCommandQueue> value,
                           const std::shared_ptr<BufferSynchronizationManager>& syncManager,
                           DeviceStatistics& deviceStatistics) noexcept :
  value_(value),
  bufferSyncManager_(syncManager),
  deviceStatistics_(deviceStatistics),
  device_(device) {
  if constexpr (kIGLMetalNumberCommandBuffersToCapture > 0 &&
                kIGLMetalBeginCommandBufferToCapture == 0) {
    startCapture(value_);
  }
}

std::shared_ptr<ICommandBuffer> CommandQueue::createCommandBuffer(const CommandBufferDesc& desc,
                                                                  Result* outResult) {
  id<MTLCommandBuffer> metalObject = [value_ commandBuffer];
  metalObject.label = [NSString stringWithUTF8String:desc.debugName.c_str()];
  auto resource = std::make_shared<CommandBuffer>(device_, metalObject, desc);
  Result::setOk(outResult);
  return resource;
}

SubmitHandle CommandQueue::submit(const igl::ICommandBuffer& commandBuffer, bool endOfFrame) {
  incrementDrawCount(commandBuffer.getCurrentDrawCount());
  deviceStatistics_.incrementDrawCount(commandBuffer.getCurrentDrawCount());

  if (endOfFrame) {
    bufferSyncManager_->markCommandBufferAsEndOfFrame(commandBuffer);
  }

  const auto& metalCommandBuffer = static_cast<const CommandBuffer&>(commandBuffer);
  std::shared_ptr<ITimer> timer = metalCommandBuffer.desc.timer;
  if (timer) {
    [metalCommandBuffer.get() addCompletedHandler:^(id<MTLCommandBuffer> cb) {
      CFTimeInterval gpuDuration = cb.GPUEndTime - cb.GPUStartTime;
      static_cast<Timer&>(*timer).executionTime_ =
          static_cast<uint64_t>(gpuDuration * 1e9); // Convert to nanoseconds
    }];
  }

  [metalCommandBuffer.get() commit];

  if (endOfFrame) {
    bufferSyncManager_->manageEndOfFrameSync();
  }

  if constexpr (kIGLMetalNumberCommandBuffersToCapture > 0) {
    static uint32_t currentCommandBuffer = 0;
    if ((currentCommandBuffer + 1) == kIGLMetalBeginCommandBufferToCapture) {
      // Start capturing one command buffer earlier
      startCapture(value_);
    } else if (currentCommandBuffer >= kIGLMetalEndCommandBufferToCapture) {
      stopCapture();
    }
    ++currentCommandBuffer;
  }

  return SubmitHandle{};
}

void CommandQueue::startCapture(id<MTLCommandQueue> queue) {
  MTLCaptureManager* captureManager = [MTLCaptureManager sharedCaptureManager];
  MTLCaptureDescriptor* captureDescriptor = [[MTLCaptureDescriptor alloc] init];
  captureDescriptor.captureObject = queue;

  NSError* error;
  if (![captureManager startCaptureWithDescriptor:captureDescriptor error:&error]) {
    NSLog(@"Failed to start capture, error %@", error);
  }
}

void CommandQueue::stopCapture() {
  MTLCaptureManager* captureManager = [MTLCaptureManager sharedCaptureManager];
  [captureManager stopCapture];
}

} // namespace igl::metal
