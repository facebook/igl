/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <webgpu/webgpu.h>
#include <igl/Shader.h>
#include <igl/Uniform.h>

namespace igl::webgpu {

/// @brief What a module-scope `@group @binding` variable declares.
enum class WgslBindingKind : uint8_t {
  UniformBuffer,
  StorageBuffer,
  ReadOnlyStorageBuffer,
  Texture,
  DepthTexture,
  MultisampledTexture,
  StorageTexture,
  Sampler,
  ComparisonSampler,
};

/// @brief The component type of a sampled texture declaration (`texture_2d<f32>` is Float).
enum class WgslSampledType : uint8_t { Float, Sint, Uint };

/// @brief A struct member laid out with WGSL's (single) memory layout algorithm.
struct WgslStructMember {
  std::string name;
  /// The member type as written, with aliases resolved.
  std::string type;
  uint32_t offset = 0;
  uint32_t size = 0;
  uint32_t align = 0;
  /// IGL type of a scalar/vector/matrix member or of the elements of an array member.
  UniformType uniformType = UniformType::Invalid;
  /// Element count of an array member (0 for a runtime-sized array), 1 otherwise.
  uint32_t arrayLength = 1;
  uint32_t arrayStride = 0;
};

struct WgslStruct {
  std::string name;
  std::vector<WgslStructMember> members;
  uint32_t size = 0;
  uint32_t align = 0;
};

struct WgslBinding {
  std::string name;
  uint32_t group = 0;
  uint32_t binding = 0;
  WgslBindingKind kind = WgslBindingKind::UniformBuffer;
  /// Buffers: the store type as written (a struct name for structured buffers).
  std::string type;
  /// Buffers: size of the store type; a trailing runtime-sized array counts one element.
  uint32_t bufferSize = 0;
  /// Textures.
  WGPUTextureViewDimension viewDimension = WGPUTextureViewDimension_Undefined;
  WgslSampledType sampledType = WgslSampledType::Float;
  /// Storage textures.
  WGPUTextureFormat storageFormat = WGPUTextureFormat_Undefined;
  WGPUStorageTextureAccess storageAccess = WGPUStorageTextureAccess_Undefined;
};

struct WgslEntryPoint {
  std::string name;
  ShaderStage stage = ShaderStage::Vertex;
  /// 0 for dimensions WGSL computes from override expressions.
  std::array<uint32_t, 3> workgroupSize = {0, 0, 0};
};

struct WgslOverride {
  std::string name;
  /// The `@id`, if the declaration has one.
  std::optional<uint32_t> id;
  std::string type;
};

/// @brief Module-scope declarations of a WGSL module, read without compiling it.
///
/// Only declarations are parsed: structs, aliases, `const` integers, `override`s, `@group
/// @binding` variables and entry point signatures. Function bodies are skipped, so a binding is
/// reported whether or not an entry point uses it.
struct WgslReflection {
  std::vector<WgslBinding> bindings;
  std::vector<WgslStruct> structs;
  std::vector<WgslEntryPoint> entryPoints;
  std::vector<WgslOverride> overrides;

  [[nodiscard]] const WgslEntryPoint* IGL_NULLABLE findEntryPoint(std::string_view name) const;
  [[nodiscard]] const WgslStruct* IGL_NULLABLE findStruct(std::string_view name) const;
  [[nodiscard]] const WgslBinding* IGL_NULLABLE findBinding(uint32_t group, uint32_t binding) const;
};

/// Parses `source`. Returns ArgumentInvalid, with the line number, for input it cannot read.
[[nodiscard]] Result parseWgslReflection(std::string_view source, WgslReflection& outReflection);

} // namespace igl::webgpu
