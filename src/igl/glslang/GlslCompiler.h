/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <glslang/Include/glslang_c_interface.h>
#include <vector>
#include <igl/Common.h>
#include <igl/Shader.h>

namespace igl::glslang {

void initializeCompiler() noexcept;

/// Maps an IGL optimization strategy onto glslang's SPIR-V optimizer options. Exposed for testing;
/// compileShader() applies the result. ShaderOptimization::Default reproduces IGL's historical
/// flags (optimizer on, optimize-for-size), and debug info is generated for every strategy.
[[nodiscard]] glslang_spv_options_t getSpvOptions(const ShaderCompilerOptions& options) noexcept;

/// Target Vulkan/SPIR-V version pair for compilation. The default preserves IGL's historical
/// behavior (Vulkan 1.2 / SPIR-V 1.5) so existing clients are byte-for-byte unchanged; pass an
/// older pair for physical devices whose Vulkan API version accepts nothing newer
/// (VUID-VkShaderModuleCreateInfo-pCode-08737).
struct GlslTargetVersion {
  glslang_target_client_version_t clientVersion = GLSLANG_TARGET_VULKAN_1_2;
  glslang_target_language_version_t spirvVersion = GLSLANG_TARGET_SPV_1_5;
};

/// Maps a Vulkan API version to the newest SPIR-V target it accepts. Unknown (0.0) maps to the
/// oldest target, whose output every Vulkan device accepts.
[[nodiscard]] inline GlslTargetVersion glslTargetForVulkanVersion(uint32_t major,
                                                                  uint32_t minor) noexcept {
  if (major < 1 || (major == 1 && minor < 1)) {
    return {.clientVersion = GLSLANG_TARGET_VULKAN_1_0, .spirvVersion = GLSLANG_TARGET_SPV_1_0};
  }
  if (major == 1 && minor < 2) {
    return {.clientVersion = GLSLANG_TARGET_VULKAN_1_1, .spirvVersion = GLSLANG_TARGET_SPV_1_3};
  }
  return {};
}

/// Compiles the given shader code into SPIR-V. `options` selects the optimization strategy (see
/// igl::ShaderOptimization); the defaults preserve IGL's historical compile behavior so existing
/// clients are byte-for-byte unchanged.
[[nodiscard]] Result compileShader(ShaderStage stage,
                                   const char* code,
                                   std::vector<uint32_t>& outSPIRV,
                                   const glslang_resource_t* glslLangResource,
                                   const ShaderCompilerOptions& options = {}) noexcept;

/// Target-aware overload for devices that cannot accept the default SPIR-V 1.5.
[[nodiscard]] Result compileShader(ShaderStage stage,
                                   const char* code,
                                   std::vector<uint32_t>& outSPIRV,
                                   const glslang_resource_t* glslLangResource,
                                   const ShaderCompilerOptions& options,
                                   GlslTargetVersion target) noexcept;

void finalizeCompiler() noexcept;

} // namespace igl::glslang
