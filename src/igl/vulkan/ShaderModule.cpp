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

namespace igl::vulkan {

ShaderModule::ShaderModule(ShaderModuleInfo info,
                           std::shared_ptr<VulkanShaderModule> shaderModule) :
  IShaderModule(std::move(info)), module_(std::move(shaderModule)) {
  IGL_ASSERT(module_);
}

VkShaderModule ShaderModule::getVkShaderModule(const std::shared_ptr<IShaderModule>& shaderModule) {
  const ShaderModule* sm = static_cast<ShaderModule*>(shaderModule.get());

  // @fb-only
  // @lint-ignore CLANGTIDY
  return sm ? sm->module_->getVkShaderModule() : VK_NULL_HANDLE;
}

ShaderStages::ShaderStages(ShaderStagesDesc desc) : IShaderStages(std::move(desc)) {}

ShaderLibrary::ShaderLibrary(std::vector<std::shared_ptr<IShaderModule>> modules) :
  IShaderLibrary(std::move(modules)) {}

} // namespace igl::vulkan
