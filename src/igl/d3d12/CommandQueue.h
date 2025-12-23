/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <igl/CommandQueue.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class Device;

class CommandQueue final : public ICommandQueue {
 public:
  explicit CommandQueue(Device& device);
  ~CommandQueue() override = default;

  std::shared_ptr<ICommandBuffer> createCommandBuffer(const CommandBufferDesc& desc,
                                                      Result* IGL_NULLABLE outResult) override;
  SubmitHandle submit(const ICommandBuffer& commandBuffer, bool endOfFrame = false) override;

  Device& getDevice() {
    return device_;
  }

 private:
  Device& device_;
  uint64_t scheduleFenceValue_ = 0; // Monotonically increasing fence value used for scheduling.
};

} // namespace igl::d3d12
