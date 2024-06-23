/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/ComputePipelineState.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/PipelineState.h>

namespace igl::vulkan {

class Device;

class ComputePipelineState final : public IComputePipelineState, public vulkan::PipelineState {
 public:
  ComputePipelineState(const igl::vulkan::Device& device, ComputePipelineDesc desc);
  ~ComputePipelineState() override;
  std::shared_ptr<IComputePipelineReflection> computePipelineReflection() override {
    return nullptr;
  }

  VkPipeline getVkPipeline() const;

  const ComputePipelineDesc& getComputePipelineDesc() const {
    return desc_;
  }

 private:
  const igl::vulkan::Device& device_;
  ComputePipelineDesc desc_;

  // a Vulkan pipeline owned by this ComputePipelineState object
  mutable VkPipeline pipeline_ = VK_NULL_HANDLE;
};

} // namespace igl::vulkan
