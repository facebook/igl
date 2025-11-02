/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12Headers.h>
#include <vector>
#include <string>

namespace igl::d3d12 {

/**
 * @brief DXC (DirectX Shader Compiler) wrapper for modern HLSL compilation
 *
 * Replaces legacy FXC (D3DCompile) with DXC for:
 * - Shader Model 6.0+ support
 * - 10-20% better shader performance
 * - Modern optimization passes
 * - Future features (raytracing, mesh shaders, wave intrinsics)
 */
class DXCCompiler {
 public:
  DXCCompiler();
  ~DXCCompiler();

  /**
   * @brief Initialize DXC compiler (call once)
   * @return Result indicating success or failure
   */
  Result initialize();

  /**
   * @brief Check if DXC is available and initialized
   */
  bool isInitialized() const { return initialized_; }

  /**
   * @brief Compile HLSL source to DXIL bytecode (Shader Model 6.0+)
   *
   * @param source HLSL source code
   * @param sourceLength Length of source code
   * @param entryPoint Entry point function name (e.g., "main")
   * @param target Shader target profile (e.g., "vs_6_0", "ps_6_0", "cs_6_0")
   * @param debugName Debug name for error messages
   * @param flags Compilation flags (D3DCOMPILE_* constants)
   * @param outBytecode Output DXIL bytecode
   * @param outErrors Output compilation errors/warnings
   * @return Result indicating success or failure
   */
  Result compile(
      const char* source,
      size_t sourceLength,
      const char* entryPoint,
      const char* target,
      const char* debugName,
      uint32_t flags,
      std::vector<uint8_t>& outBytecode,
      std::string& outErrors
  );

 private:
  Microsoft::WRL::ComPtr<IDxcUtils> utils_;
  Microsoft::WRL::ComPtr<IDxcCompiler3> compiler_;
  Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
  bool initialized_ = false;
};

} // namespace igl::d3d12
