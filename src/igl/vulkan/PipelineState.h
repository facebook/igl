/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/vulkan/Common.h>

namespace igl {
namespace vulkan {

class VulkanDescriptorSetLayout;

class PipelineState {
 public:
  std::shared_ptr<VulkanDescriptorSetLayout> dslCombinedImageSamplers_;
  std::shared_ptr<VulkanDescriptorSetLayout> dslUniformBuffers_;
  std::shared_ptr<VulkanDescriptorSetLayout> dslStorageBuffers_;
};

} // namespace vulkan
} // namespace igl
