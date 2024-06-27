/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/CommandQueue.h>

namespace igl::opengl {
class IContext;
class Device;

class CommandQueue final : public ICommandQueue {
 public:
  std::shared_ptr<ICommandBuffer> createCommandBuffer(const CommandBufferDesc& desc,
                                                      Result* outResult) override;
  SubmitHandle submit(const ICommandBuffer& commandBuffer, bool endOfFrame = false) override;

  void setInitialContext(std::shared_ptr<IContext> context);

 private:
  std::shared_ptr<IContext> context_;
  uint32_t activeCommandBuffers_ = 0;
};

} // namespace igl::opengl
