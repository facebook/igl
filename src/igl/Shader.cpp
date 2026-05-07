/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Shader.h>

#include <cstring>
#include <igl/IGLSafeC.h>

namespace {

bool safeDataCompare(const void* IGL_NULLABLE a,
                     size_t lengthA,
                     const void* IGL_NULLABLE b,
                     size_t lengthB) {
  if (lengthA != lengthB) {
    return false;
  }
  // Handle null pointers;
  if (a == nullptr || b == nullptr) {
    // If both are null, consider them equal. If only ond is null, they are not equal
    return a == b;
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

size_t getConstantValueSize(ConstantValueType type) noexcept {
  switch (type) {
  case ConstantValueType::Invalid:
    return 0;
  case ConstantValueType::Float1:
    return 4;
  case ConstantValueType::Float2:
    return 8;
  case ConstantValueType::Float3:
    return 12;
  case ConstantValueType::Float4:
    return 16;
  case ConstantValueType::Boolean1:
    return 4;
  case ConstantValueType::Boolean2:
    return 8;
  case ConstantValueType::Boolean3:
    return 12;
  case ConstantValueType::Boolean4:
    return 16;
  case ConstantValueType::Int1:
    return 4;
  case ConstantValueType::Int2:
    return 8;
  case ConstantValueType::Int3:
    return 12;
  case ConstantValueType::Int4:
    return 16;
  case ConstantValueType::Mat2x2:
    return 16;
  case ConstantValueType::Mat3x3:
    return 36;
  case ConstantValueType::Mat4x4:
    return 64;
  }
  return 0;
}

bool ShaderCompilerOptions::operator==(const ShaderCompilerOptions& other) const {
  return fastMathEnabled == other.fastMathEnabled;
}

bool ShaderCompilerOptions::operator!=(const ShaderCompilerOptions& other) const {
  return !(*this == other);
}

FunctionConstantValues& FunctionConstantValues::setConstantValue(uint8_t index,
                                                                 ConstantValueType type,
                                                                 const void* IGL_NONNULL value) {
  IGL_DEBUG_ASSERT(type != ConstantValueType::Invalid);
  IGL_DEBUG_ASSERT(value);
  const size_t dataSize = getConstantValueSize(type);
  IGL_DEBUG_ASSERT(dataSize);

  if (values_.size() <= index) {
    values_.resize(index + 1);
  }
  auto& entry = values_[index];
  if (getConstantValueSize(entry.type) != dataSize) {
    // New entry, or the size changed: append to `data_`. Bytes for the previous slot, if
    // any, are left as a small gap; backends index by offset and never read them.
    entry.offset = static_cast<uint32_t>(data_.size());
    data_.resize(data_.size() + dataSize);
  }
  entry.type = type;
  checked_memcpy(data_.data() + entry.offset, data_.size() - entry.offset, value, dataSize);
  return *this;
}

bool FunctionConstantValues::operator==(const FunctionConstantValues& other) const {
  // Compare logically (per-binding type + bytes) rather than the raw `data_` buffer, because
  // setConstantValue can leave orphan gap bytes when an entry is overwritten with a different
  // type/size. Two FCVs with the same logical content but different construction histories
  // would otherwise spuriously compare unequal.
  if (values_.size() != other.values_.size()) {
    return false;
  }
  for (size_t i = 0; i < values_.size(); ++i) {
    const auto& a = values_[i];
    const auto& b = other.values_[i];
    if (a.type != b.type) {
      return false;
    }
    if (a.type == ConstantValueType::Invalid) {
      continue;
    }
    const auto size = getConstantValueSize(a.type);
    if (memcmp(data_.data() + a.offset, other.data_.data() + b.offset, size) != 0) {
      return false;
    }
  }
  return true;
}

bool ShaderModuleInfo::operator==(const ShaderModuleInfo& other) const {
  return stage == other.stage && entryPoint == other.entryPoint &&
         functionConstantValues == other.functionConstantValues;
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
                                                   ShaderModuleInfo info,
                                                   std::string debugName) {
  return ShaderModuleDesc{
      .info = std::move(info),
      .input = {.source = source, .type = ShaderInputType::String},
      .debugName = std::move(debugName),
  };
}

ShaderModuleDesc ShaderModuleDesc::fromBinaryInput(const void* IGL_NONNULL data,
                                                   size_t dataLength,
                                                   ShaderModuleInfo info,
                                                   std::string debugName) {
  return ShaderModuleDesc{
      .info = std::move(info),
      .input = {.data = data, .length = dataLength, .type = ShaderInputType::Binary},
      .debugName = std::move(debugName),
  };
}

bool ShaderModuleDesc::operator==(const ShaderModuleDesc& other) const {
  return info == other.info && input == other.input && debugName == other.debugName;
}

bool ShaderModuleDesc::operator!=(const ShaderModuleDesc& other) const {
  return !operator==(other);
}

ShaderLibraryDesc ShaderLibraryDesc::fromStringInput(const char* IGL_NONNULL librarySource,
                                                     std::vector<ShaderModuleInfo> moduleInfo,
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
                                                     std::vector<ShaderModuleInfo> moduleInfo,
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
    if (sm && sm->info().entryPoint == entryPoint) {
      return sm;
    }
  }
  return nullptr;
}

std::shared_ptr<IShaderModule> IShaderLibrary::getShaderModule(ShaderStage stage,
                                                               const std::string& entryPoint) {
  for (const auto& sm : modules_) {
    if (sm) {
      const auto& info = sm->info();
      if (info.stage == stage && info.entryPoint == entryPoint) {
        return sm;
      }
    }
  }
  return nullptr;
}

ShaderStagesDesc ShaderStagesDesc::fromRenderModules(
    std::shared_ptr<IShaderModule> vertexModule,
    std::shared_ptr<IShaderModule> fragmentModule) {
  std::string debugName = (vertexModule ? vertexModule->info().debugName : std::string()) + ", " +
                          (fragmentModule ? fragmentModule->info().debugName : std::string());
  return ShaderStagesDesc{
      .vertexModule = std::move(vertexModule),
      .fragmentModule = std::move(fragmentModule),
      .type = ShaderStagesType::Render,
      .debugName = std::move(debugName),
  };
}

ShaderStagesDesc ShaderStagesDesc::fromMeshRenderModules(
    std::shared_ptr<IShaderModule> taskModule,
    std::shared_ptr<IShaderModule> meshModule,
    std::shared_ptr<IShaderModule> fragmentModule) {
  std::string debugName = (taskModule ? taskModule->info().debugName : std::string()) + ", " +
                          (meshModule ? meshModule->info().debugName : std::string()) + ", " +
                          (fragmentModule ? fragmentModule->info().debugName : std::string());
  return ShaderStagesDesc{
      .fragmentModule = std::move(fragmentModule),
      .taskModule = std::move(taskModule),
      .meshModule = std::move(meshModule),
      .type = ShaderStagesType::RenderMeshShader,
      .debugName = std::move(debugName),
  };
}

ShaderStagesDesc ShaderStagesDesc::fromComputeModule(std::shared_ptr<IShaderModule> computeModule) {
  std::string debugName = computeModule ? computeModule->info().debugName : "igl/Shader.cpp";
  return ShaderStagesDesc{
      .computeModule = std::move(computeModule),
      .type = ShaderStagesType::Compute,
      .debugName = std::move(debugName),
  };
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

const std::shared_ptr<IShaderModule>& IShaderStages::getTaskModule() const noexcept {
  return desc_.taskModule;
}

const std::shared_ptr<IShaderModule>& IShaderStages::getMeshModule() const noexcept {
  return desc_.meshModule;
}

bool IShaderStages::isValid() const noexcept {
  if (desc_.type == ShaderStagesType::Render) {
    return desc_.vertexModule && desc_.fragmentModule && !desc_.computeModule;
  } else if (desc_.type == ShaderStagesType::Compute) {
    return desc_.computeModule && !desc_.vertexModule && !desc_.fragmentModule;
  } else if (desc_.type == ShaderStagesType::RenderMeshShader) {
    return desc_.meshModule && desc_.fragmentModule && !desc_.computeModule && !desc_.vertexModule;
  }

  return false;
}

} // namespace igl

namespace std {
// @fb-only

size_t hash<igl::ShaderCompilerOptions>::operator()(const igl::ShaderCompilerOptions& key) const {
  const size_t result = std::hash<bool>()(key.fastMathEnabled);
  return result;
}

size_t hash<igl::FunctionConstantValues>::operator()(const igl::FunctionConstantValues& key) const {
  // Hash logically (per-binding type + constant bytes), matching FunctionConstantValues::==.
  // Orphan gap bytes in `data_` from prior different-size overwrites are intentionally not
  // hashed, so two FCVs with the same logical content always hash the same.
  size_t result = 0;
  const auto& values = key.getConstantValues();
  const uint8_t* data = key.getData().data();
  for (size_t i = 0; i < values.size(); ++i) {
    const auto& entry = values[i];
    if (entry.type == igl::ConstantValueType::Invalid) {
      continue;
    }
    result ^= std::hash<size_t>()(i);
    result ^= std::hash<uint8_t>()(static_cast<uint8_t>(entry.type));
    const size_t size = igl::getConstantValueSize(entry.type);
    for (size_t b = 0; b < size; ++b) {
      result ^= std::hash<uint8_t>()(data[entry.offset + b]);
    }
  }
  return result;
}

size_t hash<igl::ShaderModuleInfo>::operator()(const igl::ShaderModuleInfo& key) const {
  static_assert(std::is_same_v<uint8_t, std::underlying_type<igl::ShaderStage>::type>);
  size_t result = std::hash<uint8_t>()(static_cast<uint8_t>(key.stage));
  result ^= std::hash<string>()(key.entryPoint);
  result ^= std::hash<igl::FunctionConstantValues>()(key.functionConstantValues);
  return result;
}

size_t hash<igl::ShaderInput>::operator()(const igl::ShaderInput& key) const {
  static_assert(std::is_same_v<uint8_t, std::underlying_type<igl::ShaderInputType>::type>);
  size_t result = safeCStrHash(key.source);
  result ^= safeDataHash(key.data, key.length);
  result ^= std::hash<uint8_t>()(EnumToValue(key.type));
  return result;
}

size_t hash<igl::ShaderModuleDesc>::operator()(const igl::ShaderModuleDesc& key) const {
  static_assert(std::is_same_v<uint8_t, std::underlying_type<igl::ShaderInputType>::type>);
  size_t result = std::hash<igl::ShaderModuleInfo>()(key.info);
  result ^= std::hash<igl::ShaderInput>()(key.input);
  result ^= std::hash<string>()(key.debugName);
  return result;
}

size_t hash<igl::ShaderLibraryDesc>::operator()(const igl::ShaderLibraryDesc& key) const {
  static_assert(std::is_same_v<uint8_t, std::underlying_type<igl::ShaderInputType>::type>);
  size_t result = std::hash<size_t>()(key.moduleInfo.size());
  for (const auto& info : key.moduleInfo) {
    result ^= std::hash<igl::ShaderModuleInfo>()(info);
  }
  result ^= std::hash<igl::ShaderInput>()(key.input);
  result ^= std::hash<string>()(key.debugName);
  return result;
}

// @fb-only
} // namespace std
