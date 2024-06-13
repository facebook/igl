/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/managedUniformBuffer/ManagedUniformBuffer.h>

#include <string>

namespace iglu {
/// Extension for ManagedUniformBuffer that enables OpenGL bindings in the form
/// they implemented in SPIRV-Cross (UBOs are converted to plain uniforms).
class ShaderCrossUniformBuffer : public ManagedUniformBuffer {
 public:
  ShaderCrossUniformBuffer(igl::IDevice& device,
                           const std::string& uboBlockName,
                           ManagedUniformBufferInfo info);
};
} // namespace iglu
