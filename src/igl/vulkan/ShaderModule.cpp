/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/ShaderModule.h>

#include <igl/Shader.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanShaderModule.h>

namespace igl {

namespace vulkan {

ShaderModule::ShaderModule(ShaderModuleDesc desc,
                           std::shared_ptr<VulkanShaderModule> shaderModule) :
  module_(std::move(shaderModule)), desc_(std::move(desc)) {
  IGL_ASSERT(module_.get());
}

VkShaderModule ShaderModule::getVkShaderModule(const IShaderModule* shaderModule) {
  const ShaderModule* sm = static_cast<const ShaderModule*>(shaderModule);

  return sm ? sm->module_->getVkShaderModule() : VK_NULL_HANDLE;
}

} // namespace vulkan
} // namespace igl
