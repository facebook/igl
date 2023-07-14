/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <lvk/LVK.h>
#include <igl/vulkan/Common.h>

namespace igl {
namespace vulkan {

class Device;

class ComputePipelineState final : public IComputePipelineState {
 public:
  ComputePipelineState(igl::vulkan::Device& device, const ComputePipelineDesc& desc);
  ~ComputePipelineState() override;

  VkPipeline getVkPipeline() const;

 private:
  igl::vulkan::Device& device_;
  ComputePipelineDesc desc_;

  mutable VkPipeline pipeline_ = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace igl
