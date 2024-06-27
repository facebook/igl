/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/VulkanShaderModule.h>

namespace igl::vulkan {

VulkanShaderModule::VulkanShaderModule(const VulkanFunctionTable& vf,
                                       VkDevice device,
                                       VkShaderModule shaderModule,
                                       util::SpvModuleInfo&& moduleInfo) :
  vf_(vf), device_(device), vkShaderModule_(shaderModule), moduleInfo_(std::move(moduleInfo)) {}

VulkanShaderModule::~VulkanShaderModule() {
  vf_.vkDestroyShaderModule(device_, vkShaderModule_, nullptr);
}

} // namespace igl::vulkan
