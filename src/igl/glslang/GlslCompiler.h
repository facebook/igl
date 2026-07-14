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

/// Compiles the given shader code into SPIR-V. `options` selects the optimization strategy (see
/// igl::ShaderOptimization); the default preserves IGL's historical compile behavior so existing
/// clients are byte-for-byte unchanged.
[[nodiscard]] Result compileShader(ShaderStage stage,
                                   const char* code,
                                   std::vector<uint32_t>& outSPIRV,
                                   const glslang_resource_t* glslLangResource,
                                   const ShaderCompilerOptions& options = {}) noexcept;

void finalizeCompiler() noexcept;

} // namespace igl::glslang
