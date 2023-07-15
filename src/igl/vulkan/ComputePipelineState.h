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

class ComputePipelineState final {
 public:
  ComputePipelineState() = default;
  ComputePipelineState(igl::vulkan::Device* device, const ComputePipelineDesc& desc);
  ~ComputePipelineState();

  ComputePipelineState(const ComputePipelineState&) = delete;
  ComputePipelineState& operator=(const ComputePipelineState&) = delete;

  ComputePipelineState(ComputePipelineState&& other);
  ComputePipelineState& operator=(ComputePipelineState&& other);

  VkPipeline getVkPipeline() const;

 private:
  igl::vulkan::Device* device_ = nullptr;
  ComputePipelineDesc desc_;

  mutable VkPipeline pipeline_ = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace igl
