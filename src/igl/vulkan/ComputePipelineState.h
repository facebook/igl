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

namespace igl {
namespace vulkan {

class Device;

class ComputePipelineState final : public IComputePipelineState {
 public:
  // Ignore modernize-pass-by-value
  // @lint-ignore CLANGTIDY
  ComputePipelineState(const igl::vulkan::Device& device, const ComputePipelineDesc& desc);
  ~ComputePipelineState() override;
  std::shared_ptr<IComputePipelineReflection> computePipelineReflection() override {
    return nullptr;
  }

  VkPipeline getVkPipeline() const;

 private:
  friend class Device;

  const igl::vulkan::Device& device_;
  ComputePipelineDesc desc_;

  mutable VkPipelineLayout vkPipelineLayout_ = VK_NULL_HANDLE;
  mutable VkPipeline pipeline_ = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace igl
