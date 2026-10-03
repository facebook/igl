/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <igl/CommandQueue.h>

namespace igl::webgpu {

class WebGPUContext;

/// @brief Implements the igl::ICommandQueue interface on the device's WGPUQueue.
class CommandQueue final : public ICommandQueue {
 public:
  explicit CommandQueue(WebGPUContext& ctx) : ctx_(ctx) {}

  std::shared_ptr<ICommandBuffer> createCommandBuffer(const CommandBufferDesc& desc,
                                                      Result* IGL_NULLABLE outResult) override;
  /// Returns the command buffer's serial, or 0 if it was not submitted.
  SubmitHandle submit(const ICommandBuffer& commandBuffer, bool endOfFrame = false) override;

 private:
  WebGPUContext& ctx_;
};

} // namespace igl::webgpu
