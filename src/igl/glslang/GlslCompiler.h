/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <glslang/Include/glslang_c_interface.h>

#include <igl/Common.h>
#include <igl/Shader.h>

#include <vector>

namespace igl::glslang {

void initializeCompiler() noexcept;

/// Compiles the given shader code into SPIR-V.
[[nodiscard]] igl::Result compileShader(igl::ShaderStage stage,
                                        const char* code,
                                        std::vector<uint32_t>& outSPIRV,
                                        const glslang_resource_t* glslLangResource) noexcept;

void finalizeCompiler() noexcept;

} // namespace igl::glslang
