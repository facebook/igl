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

class CommandQueue final : public ICommandQueue {
 public:
  CommandQueue() = default;
  ~CommandQueue() override = default;

  std::shared_ptr<ICommandBuffer> createCommandBuffer(const CommandBufferDesc& desc,
                                                       Result* IGL_NULLABLE outResult) override;
  SubmitHandle submit(const ICommandBuffer& commandBuffer, bool endOfFrame = false) override;

 private:
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
};

} // namespace igl::d3d12
