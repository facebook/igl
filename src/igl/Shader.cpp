/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Shader.h>

#include <cstring>
#include <igl/Common.h>
#include <type_traits>

namespace {

bool safeDataCompare(const void* IGL_NULLABLE a,
                     size_t lengthA,
                     const void* IGL_NULLABLE b,
                     size_t lengthB) {
  if (lengthA != lengthB) {
    return false;
  }
  return (memcmp(a, b, lengthA) == 0);
}

size_t safeDataHash(const void* IGL_NULLABLE ptr, size_t length) {
  if (ptr == nullptr) {
    return 0;
  }
  size_t hash = 0;
  for (size_t i = 0; i < length; ++i) {
    hash ^= static_cast<const unsigned char*>(ptr)[i];
  }
  return hash;
}

bool safeCStrCompare(const char* IGL_NULLABLE a, const char* IGL_NULLABLE b) {
  if (a == b) {
    return true;
  }
  if (a == nullptr || b == nullptr) {
    return false;
  }
  return (strcmp(a, b) == 0);
}

size_t safeCStrHash(const char* IGL_NULLABLE s) {
  if (s == nullptr) {
    return 0;
  }
  return std::hash<std::string_view>()(std::string_view(s, strlen(s)));
}

} // namespace

namespace igl {

bool ShaderCompilerOptions::operator==(const ShaderCompilerOptions& other) const {
  return fastMathEnabled == other.fastMathEnabled;
}

bool ShaderCompilerOptions::operator!=(const ShaderCompilerOptions& other) const {
  return !(*this == other);
}

bool ShaderModuleInfo::operator==(const ShaderModuleInfo& other) const {
  return stage == other.stage && entryPoint == other.entryPoint;
}

bool ShaderModuleInfo::operator!=(const ShaderModuleInfo& other) const {
  return !operator==(other);
}

bool ShaderInput::isValid() const noexcept {
  if (type == ShaderInputType::String) {
    return source != nullptr && data == nullptr && length == 0;
  } else if (type == ShaderInputType::Binary) {
    return data != nullptr && length > 0 && source == nullptr;
  } else {
    return false;
  }
}

bool ShaderInput::operator==(const ShaderInput& other) const {
  return type == other.type && options == other.options &&
         (type == ShaderInputType::String ? safeCStrCompare(source, other.source) : true) &&
         (type == ShaderInputType::Binary ? safeDataCompare(data, length, other.data, other.length)
                                          : true);
}

bool ShaderInput::operator!=(const ShaderInput& other) const {
  return !operator==(other);
}

ShaderModuleDesc ShaderModuleDesc::fromStringInput(const char* IGL_NONNULL source,
                                                   igl::ShaderModuleInfo info,
                                                   std::string debugName) {
  ShaderModuleDesc desc;
  desc.input.type = ShaderInputType::String;
  desc.input.source = source;
  desc.info = std::move(info);
  desc.debugName = std::move(debugName);
  return desc;
}

ShaderModuleDesc ShaderModuleDesc::fromBinaryInput(const void* IGL_NONNULL data,
                                                   size_t dataLength,
                                                   igl::ShaderModuleInfo info,
                                                   std::string debugName) {
  ShaderModuleDesc desc;
  desc.input.type = ShaderInputType::Binary;
  desc.input.data = data;
  desc.input.length = dataLength;
  desc.info = std::move(info);
  desc.debugName = std::move(debugName);
  return desc;
}

bool ShaderModuleDesc::operator==(const ShaderModuleDesc& other) const {
  return info == other.info && input == other.input && debugName == other.debugName;
}

bool ShaderModuleDesc::operator!=(const ShaderModuleDesc& other) const {
  return !operator==(other);
}

ShaderLibraryDesc ShaderLibraryDesc::fromStringInput(const char* IGL_NONNULL librarySource,
                                                     std::vector<igl::ShaderModuleInfo> moduleInfo,
                                                     std::string libraryDebugName) {
  ShaderLibraryDesc libraryDesc;
  libraryDesc.input.type = ShaderInputType::String;
  libraryDesc.input.source = librarySource;
  libraryDesc.debugName = std::move(libraryDebugName);

  if (IGL_DEBUG_VERIFY(!moduleInfo.empty())) {
    libraryDesc.moduleInfo = std::move(moduleInfo);
  }

  return libraryDesc;
}

ShaderLibraryDesc ShaderLibraryDesc::fromBinaryInput(const void* IGL_NONNULL libraryData,
                                                     size_t libraryDataLength,
                                                     std::vector<igl::ShaderModuleInfo> moduleInfo,
                                                     std::string libraryDebugName) {
  ShaderLibraryDesc libraryDesc;
  libraryDesc.input.type = ShaderInputType::Binary;
  libraryDesc.input.data = libraryData;
  libraryDesc.input.length = libraryDataLength;
  libraryDesc.debugName = std::move(libraryDebugName);

  if (IGL_DEBUG_VERIFY(!moduleInfo.empty())) {
    libraryDesc.moduleInfo = std::move(moduleInfo);
  }

  return libraryDesc;
}

bool ShaderLibraryDesc::operator==(const ShaderLibraryDesc& other) const {
  return moduleInfo == other.moduleInfo && input == other.input && debugName == other.debugName;
}

bool ShaderLibraryDesc::operator!=(const ShaderLibraryDesc& other) const {
  return !operator==(other);
}

IShaderModule::IShaderModule(ShaderModuleInfo info) : info_(std::move(info)) {}

const ShaderModuleInfo& IShaderModule::info() const noexcept {
  return info_;
}

IShaderLibrary::IShaderLibrary(std::vector<std::shared_ptr<IShaderModule>> modules) :
  modules_(std::move(modules)) {}

std::shared_ptr<IShaderModule> IShaderLibrary::getShaderModule(const std::string& entryPoint) {
  for (const auto& sm : modules_) {
    if (sm->info().entryPoint == entryPoint) {
      return sm;
    }
  }
  return nullptr;
}

std::shared_ptr<IShaderModule> IShaderLibrary::getShaderModule(ShaderStage stage,
                                                               const std::string& entryPoint) {
  for (const auto& sm : modules_) {
    const auto& info = sm->info();
    if (info.stage == stage && info.entryPoint == entryPoint) {
      return sm;
    }
  }
  return nullptr;
}

ShaderStagesDesc ShaderStagesDesc::fromRenderModules(
    std::shared_ptr<IShaderModule> vertexModule,
    std::shared_ptr<IShaderModule> fragmentModule) {
  ShaderStagesDesc desc;
  desc.debugName = vertexModule->info().debugName + ", " + fragmentModule->info().debugName;
  desc.type = ShaderStagesType::Render;
  desc.vertexModule = std::move(vertexModule);
  desc.fragmentModule = std::move(fragmentModule);
  return desc;
}

ShaderStagesDesc ShaderStagesDesc::fromComputeModule(std::shared_ptr<IShaderModule> computeModule) {
  ShaderStagesDesc desc;
  desc.debugName = computeModule->info().debugName;
  desc.type = ShaderStagesType::Compute;
  desc.computeModule = std::move(computeModule);
  return desc;
}

IShaderStages::IShaderStages(ShaderStagesDesc desc) : desc_(std::move(desc)) {}

ShaderStagesType IShaderStages::getType() const noexcept {
  return desc_.type;
}

const std::shared_ptr<IShaderModule>& IShaderStages::getVertexModule() const noexcept {
  return desc_.vertexModule;
}

const std::shared_ptr<IShaderModule>& IShaderStages::getFragmentModule() const noexcept {
  return desc_.fragmentModule;
}

const std::shared_ptr<IShaderModule>& IShaderStages::getComputeModule() const noexcept {
  return desc_.computeModule;
}

bool IShaderStages::isValid() const noexcept {
  if (desc_.type == ShaderStagesType::Render) {
    return desc_.vertexModule && desc_.fragmentModule && !desc_.computeModule;
  } else if (desc_.type == ShaderStagesType::Compute) {
    return desc_.computeModule && !desc_.vertexModule && !desc_.fragmentModule;
  }

  return false;
}

} // namespace igl

namespace std {

size_t std::hash<igl::ShaderCompilerOptions>::operator()(
    const igl::ShaderCompilerOptions& key) const {
  const size_t hash = std::hash<bool>()(key.fastMathEnabled);
  return hash;
}

size_t std::hash<igl::ShaderModuleInfo>::operator()(const igl::ShaderModuleInfo& key) const {
  static_assert(std::is_same_v<uint8_t, std::underlying_type<igl::ShaderStage>::type>);
  size_t hash = std::hash<uint8_t>()(static_cast<uint8_t>(key.stage));
  hash ^= std::hash<std::string>()(key.entryPoint);
  return hash;
}

size_t std::hash<igl::ShaderInput>::operator()(const igl::ShaderInput& key) const {
  static_assert(std::is_same_v<uint8_t, std::underlying_type<igl::ShaderInputType>::type>);
  size_t hash = safeCStrHash(key.source);
  hash ^= safeDataHash(key.data, key.length);
  hash ^= std::hash<uint8_t>()(EnumToValue(key.type));
  return hash;
}

size_t std::hash<igl::ShaderModuleDesc>::operator()(const igl::ShaderModuleDesc& key) const {
  static_assert(std::is_same_v<uint8_t, std::underlying_type<igl::ShaderInputType>::type>);
  size_t hash = std::hash<igl::ShaderModuleInfo>()(key.info);
  hash ^= std::hash<igl::ShaderInput>()(key.input);
  hash ^= std::hash<std::string>()(key.debugName);
  return hash;
}

size_t std::hash<igl::ShaderLibraryDesc>::operator()(const igl::ShaderLibraryDesc& key) const {
  static_assert(std::is_same_v<uint8_t, std::underlying_type<igl::ShaderInputType>::type>);
  size_t hash = std::hash<size_t>()(key.moduleInfo.size());
  for (const auto& info : key.moduleInfo) {
    hash ^= std::hash<igl::ShaderModuleInfo>()(info);
  }
  hash ^= std::hash<igl::ShaderInput>()(key.input);
  hash ^= std::hash<std::string>()(key.debugName);
  return hash;
}

} // namespace std
