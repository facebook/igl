/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#import <Metal/MTLCommandQueue.h>
#include <igl/CommandQueue.h>
#include <igl/metal/Device.h>

namespace igl::metal {

class BufferSynchronizationManager;

class CommandQueue final : public ICommandQueue {
 public:
  CommandQueue(igl::metal::Device& device,
               id<MTLCommandQueue> value,
               const std::shared_ptr<BufferSynchronizationManager>& syncManager,
               DeviceStatistics& deviceStatistics) noexcept;
  std::shared_ptr<ICommandBuffer> createCommandBuffer(const CommandBufferDesc& desc,
                                                      Result* outResult) override;
  SubmitHandle submit(const igl::ICommandBuffer& commandBuffer, bool endOfFrame = false) override;

  IGL_INLINE id<MTLCommandQueue> get() const {
    return value_;
  }

 private:
  void startCapture(id<MTLCommandQueue> queue);
  void stopCapture();

  id<MTLCommandQueue> value_;
  std::shared_ptr<BufferSynchronizationManager> bufferSyncManager_;
  DeviceStatistics& deviceStatistics_;
  igl::metal::Device& device_;
};

} // namespace igl::metal
