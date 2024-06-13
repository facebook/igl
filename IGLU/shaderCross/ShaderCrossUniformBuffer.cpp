/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/shaderCross/ShaderCrossUniformBuffer.h>

namespace iglu {
namespace {
[[nodiscard]] ManagedUniformBufferInfo getSpirvCrossCompatibleManagedUniformBufferInfo(
    const std::string& uboBlockName,
    ManagedUniformBufferInfo info) noexcept {
  for (auto& uniform : info.uniforms) {
    uniform.name = uboBlockName + "." + uniform.name;
  }
  return info;
}
} // namespace

ShaderCrossUniformBuffer::ShaderCrossUniformBuffer(igl::IDevice& device,
                                                   const std::string& uboBlockName,
                                                   ManagedUniformBufferInfo info) :
  ManagedUniformBuffer(
      device,
      getSpirvCrossCompatibleManagedUniformBufferInfo(uboBlockName, std::move(info))) {}
} // namespace iglu
