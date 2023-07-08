/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Shader.h>
#include <igl/vulkan/Common.h>
#include <vector>

namespace igl {

namespace vulkan {

class Device;
class VulkanShaderModule;

class ShaderModule final : public IShaderModule {
 public:
  ShaderModule(ShaderModuleDesc info, std::shared_ptr<VulkanShaderModule> shaderModule);
  ~ShaderModule() override = default;

  static VkShaderModule getVkShaderModule(const IShaderModule* shaderModule);

 private:
  std::shared_ptr<VulkanShaderModule> module_;
};

} // namespace vulkan
} // namespace igl
