/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <utility>
#include <igl/Shader.h>
#include <igl/webgpu/Common.h>

namespace igl::webgpu {

class WebGPUContext;

/// @brief A WGSL shader module and the entry point IGL selected from it.
class ShaderModule final : public IShaderModule {
 public:
  /// Compiles the WGSL source in `desc.input`. Compilation errors are returned in `outResult`.
  [[nodiscard]] static std::shared_ptr<ShaderModule> create(const WebGPUContext& ctx,
                                                            const ShaderModuleDesc& desc,
                                                            Result* IGL_NULLABLE outResult);

  ShaderModule(ShaderModuleInfo info, Handle<WGPUShaderModule> module);

  [[nodiscard]] WGPUShaderModule IGL_NULLABLE getWGPUShaderModule() const noexcept {
    return module_.get();
  }

 private:
  Handle<WGPUShaderModule> module_;
};

class ShaderStages final : public IShaderStages {
 public:
  explicit ShaderStages(ShaderStagesDesc desc) : IShaderStages(std::move(desc)) {}
};

} // namespace igl::webgpu
