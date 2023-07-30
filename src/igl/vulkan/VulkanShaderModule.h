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

typedef struct glslang_resource_s glslang_resource_t;

namespace lvk {
namespace vulkan {

Result compileShader(VkDevice device,
                     VkShaderStageFlagBits stage,
                     const char* code,
                     VkShaderModule* outShaderModule,
                     const glslang_resource_t* glslLangResource = nullptr);

class VulkanShaderModule final {
 public:
  VulkanShaderModule() = default;
  VulkanShaderModule(VkDevice device, VkShaderModule shaderModule, const char* entryPoint);
  ~VulkanShaderModule();

  VulkanShaderModule(const VulkanShaderModule&) = delete;
  VulkanShaderModule& operator=(const VulkanShaderModule&) = delete;

  VulkanShaderModule(VulkanShaderModule&& other) :
    device_(other.device_), entryPoint_(other.entryPoint_) {
    std::swap(vkShaderModule_, other.vkShaderModule_);
  }

  VulkanShaderModule& operator=(VulkanShaderModule&& other) noexcept {
    VulkanShaderModule tmp(std::move(other));
    std::swap(device_, tmp.device_);
    std::swap(vkShaderModule_, tmp.vkShaderModule_);
    return *this;
  }

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
} // namespace lvk
