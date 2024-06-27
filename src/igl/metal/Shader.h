/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <Metal/Metal.h>
#include <igl/Shader.h>
#include <string>
#include <vector>

namespace igl::metal {

class ShaderModule final : public IShaderModule {
  friend class Device;

 public:
  ShaderModule(ShaderModuleInfo info, id<MTLFunction> value);
  ~ShaderModule() override = default;

  IGL_INLINE id<MTLFunction> get() const {
    return value_;
  }

  id<MTLFunction> value_;
};

class ShaderLibrary final : public IShaderLibrary {
 public:
  explicit ShaderLibrary(std::vector<std::shared_ptr<IShaderModule>> modules);
};

class ShaderStages final : public IShaderStages {
 public:
  explicit ShaderStages(ShaderStagesDesc desc);
};

} // namespace igl::metal
