/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/CommandQueue.h>

namespace iglu::sentinel {

/**
 * Sentinel CommandQueue intended for safe use where access to a real command queue is not
 * available.
 * Use cases include returning a reference to a command queue from a raw pointer when a
 * valid command queue is not available.
 * All methods return nullptr, the default value or an error.
 */
class CommandQueue final : public igl::ICommandQueue {
 public:
  explicit CommandQueue(bool shouldAssert = true);

  [[nodiscard]] std::shared_ptr<igl::ICommandBuffer> createCommandBuffer(
      const igl::CommandBufferDesc& desc,
      igl::Result* IGL_NULLABLE outResult) final;
  igl::SubmitHandle submit(const igl::ICommandBuffer& commandBuffer, bool endOfFrame = false) final;

 private:
  [[maybe_unused]] bool shouldAssert_;
};

} // namespace iglu::sentinel
