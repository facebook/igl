/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <vector>

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/util/SpvReflection.h>

namespace igl::vulkan {

/**
 * @brief RAII wrapper for a Vulkan shader module.
 */
class VulkanShaderModule final {
 public:
  /** @brief Instantiates a shader module wrapper with the module and the device that owns it */
  VulkanShaderModule(const VulkanFunctionTable& vf,
                     VkDevice device,
                     VkShaderModule shaderModule,
                     util::SpvModuleInfo&& moduleInfo);
  ~VulkanShaderModule();

  /** @brief Returns the underlying Vulkan shader module */
  [[nodiscard]] VkShaderModule getVkShaderModule() const {
    return vkShaderModule_;
  }

  [[nodiscard]] const util::SpvModuleInfo& getSpvModuleInfo() const {
    return moduleInfo_;
  }

 private:
  const VulkanFunctionTable& vf_;
  VkDevice device_ = VK_NULL_HANDLE;
  VkShaderModule vkShaderModule_ = VK_NULL_HANDLE;
  util::SpvModuleInfo moduleInfo_ = {};
};

} // namespace igl::vulkan
