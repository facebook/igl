/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Foundation/Foundation.h>

#include <igl/metal/Shader.h>

namespace igl::metal {

metal::ShaderModule::ShaderModule(ShaderModuleInfo info, id<MTLFunction> value) :
  IShaderModule(std::move(info)), value_(value) {}

metal::ShaderLibrary::ShaderLibrary(std::vector<std::shared_ptr<IShaderModule>> modules) :
  IShaderLibrary(std::move(modules)) {}

metal::ShaderStages::ShaderStages(ShaderStagesDesc desc) : IShaderStages(std::move(desc)) {}

} // namespace igl::metal
