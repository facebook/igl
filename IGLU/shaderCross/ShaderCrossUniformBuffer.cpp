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
  // Record the block name so the OpenGL bind path can fall back to a real UBO buffer binding when
  // the program keeps the block native (GLSL ES 3.x) instead of flattening it to the plain
  // `<block>.<member>` uniforms named above.
  info.blockName = uboBlockName;
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
