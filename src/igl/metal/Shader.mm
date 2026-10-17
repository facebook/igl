/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/Shader.h>

namespace igl::metal {

metal::ShaderModule::ShaderModule(ShaderModuleInfo info, id<MTLFunction> value) :
  IShaderModule(std::move(info)), value(value) {}

metal::ShaderLibrary::ShaderLibrary(std::vector<std::shared_ptr<IShaderModule>> modules) :
  IShaderLibrary(std::move(modules)) {}

DynamicLibrary::DynamicLibrary(id<MTLDynamicLibrary> value) : value_(value) {
  const char* installName = value.installName.UTF8String;
  installName_ = installName != nullptr ? installName : "";
}

id<MTLDynamicLibrary> DynamicLibrary::get() const {
  return value_;
}

const std::string& DynamicLibrary::getInstallName() const noexcept {
  return installName_;
}

metal::ShaderStages::ShaderStages(ShaderStagesDesc desc) : IShaderStages(std::move(desc)) {}

ShaderStages::ShaderStages(ShaderStagesDesc desc,
                           std::vector<std::shared_ptr<DynamicLibrary>> linkedLibraries) :
  IShaderStages(std::move(desc)), linkedLibraries_(std::move(linkedLibraries)) {}

const std::vector<std::shared_ptr<DynamicLibrary>>& ShaderStages::getLinkedLibraries()
    const noexcept {
  return linkedLibraries_;
}

} // namespace igl::metal
