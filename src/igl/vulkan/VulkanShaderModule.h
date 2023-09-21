/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanFunctions.h>
#include <igl/vulkan/VulkanHelpers.h>

namespace igl {
namespace vulkan {

Result compileShader(const VulkanFunctionTable& vf,
                     VkDevice device,
                     VkShaderStageFlagBits stage,
                     const char* code,
                     VkShaderModule* outShaderModule,
                     const glslang_resource_t* glslLangResource = nullptr);

/**
 * @brief RAII wrapper for a Vulkan shader module.
 */
class VulkanShaderModule final {
 public:
  /** @brief Instantiates a shader module wrapper with the module and the device that owns it */
  VulkanShaderModule(const VulkanFunctionTable& vf, VkDevice device, VkShaderModule shaderModule);
  ~VulkanShaderModule();

  /** @brief Returns the underlying Vulkan shader module */
  VkShaderModule getVkShaderModule() const {
    return vkShaderModule_;
  }

 private:
  const VulkanFunctionTable& vf_;
  VkDevice device_ = VK_NULL_HANDLE;
  VkShaderModule vkShaderModule_ = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace igl
