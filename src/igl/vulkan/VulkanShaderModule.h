/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanHelpers.h>

namespace igl {
namespace vulkan {

Result compileShader(VkDevice device,
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
  VulkanShaderModule(VkDevice device, VkShaderModule shaderModule, const char* entryPoint);
  ~VulkanShaderModule();

  VkShaderModule getVkShaderModule() const {
    return vkShaderModule_;
  }

  const char* getEntryPoint() const {
    return entryPoint_;
  }

 private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkShaderModule vkShaderModule_ = VK_NULL_HANDLE;
  const char* entryPoint_ = nullptr;
};

} // namespace vulkan
} // namespace igl
