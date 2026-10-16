/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <igl/Common.h>
#include <igl/ComputePipelineState.h>
#include <igl/Device.h>
#include <igl/RenderPipelineState.h>
#include <igl/Shader.h>

/// Shader programs compiled from Slang by tools/slang/generate_slang.py. A generated header holds
/// one Program per Slang source, with code for every backend; these helpers pick the code that
/// matches the device and create shader stages from it. Create uniform buffers with
/// BufferAPIHintBits::UniformBlock: OpenGL binds only those as uniform blocks.
namespace iglu::slang {

/// Shader source split into pieces, because MSVC limits the length of one string literal.
struct SourceChunks {
  const char* const* chunks = nullptr;
  size_t count = 0;

  [[nodiscard]] bool empty() const {
    return count == 0;
  }
  [[nodiscard]] std::string join() const;
};

struct EntryPoint {
  igl::ShaderStage stage = igl::ShaderStage::Vertex;
  /// Entry point name in the MSL, HLSL and WGSL code. SPIR-V and GLSL entry points are "main".
  const char* name = nullptr;
  /// One SPIR-V module per entry point.
  const uint32_t* spirv = nullptr;
  size_t spirvWords = 0;
  /// GLSL 410 for render stages, GLSL 430 for compute.
  SourceChunks glsl;
  /// GLSL ES 300 for render stages, GLSL ES 310 for compute.
  SourceChunks essl;
  /// One HLSL module per entry point.
  SourceChunks hlsl;
  uint32_t workgroupSize[3] = {1, 1, 1};
};

enum class GlResourceKind : uint8_t {
  UniformBlock,
  StorageBlock,
  Sampler,
};

/// The name OpenGL knows a resource by, for backends that bind by name.
struct GlResourceName {
  GlResourceKind kind = GlResourceKind::UniformBlock;
  /// IGL buffer index or texture unit.
  uint32_t index = 0;
  /// Block or sampler uniform name.
  const char* name = nullptr;
  /// Block instance name; empty for samplers.
  const char* instanceName = "";
  /// Bit i is set when Program::entryPoints[i] uses the resource.
  uint32_t entryPointMask = 0;
};

/// Vertex attribute names follow the attribute's semantic (position, texcoord, color, normal,
/// tangent); the OpenGL and D3D12 backends match vertex input attributes by these names.
struct VertexAttribute {
  int location = 0;
  const char* name = nullptr;
};

struct Program {
  const char* name = nullptr;
  const EntryPoint* entryPoints = nullptr;
  size_t numEntryPoints = 0;
  /// Every entry point, in one library.
  SourceChunks msl;
  /// Every entry point, in one module.
  SourceChunks wgsl;
  const GlResourceName* glResources = nullptr;
  size_t numGlResources = 0;
  const VertexAttribute* vertexAttributes = nullptr;
  size_t numVertexAttributes = 0;
};

[[nodiscard]] const EntryPoint* IGL_NULLABLE findEntryPoint(const Program& program,
                                                            std::string_view name);

[[nodiscard]] std::unique_ptr<igl::IShaderStages> createRenderStages(
    const igl::IDevice& device,
    const Program& program,
    std::string_view vertexEntryPoint,
    std::string_view fragmentEntryPoint,
    igl::Result* IGL_NULLABLE outResult);

[[nodiscard]] std::unique_ptr<igl::IShaderStages> createComputeStages(
    const igl::IDevice& device,
    const Program& program,
    std::string_view computeEntryPoint,
    igl::Result* IGL_NULLABLE outResult);

/// Mesh shading on Vulkan and Metal: an optional task stage (none if `taskEntryPoint` is empty)
/// and a mesh stage from `meshProgram`, and a fragment stage from `fragmentProgram`, which may be
/// a different program.
[[nodiscard]] std::unique_ptr<igl::IShaderStages> createMeshRenderStages(
    const igl::IDevice& device,
    const Program& meshProgram,
    std::string_view taskEntryPoint,
    std::string_view meshEntryPoint,
    const Program& fragmentProgram,
    std::string_view fragmentEntryPoint,
    igl::Result* IGL_NULLABLE outResult);

/// Threads per threadgroup, for dispatchThreadGroups() and drawMeshTasks().
[[nodiscard]] igl::Dimensions workgroupSize(const EntryPoint& entryPoint);

/// Adds the names OpenGL binds the entry points' samplers and blocks by. Harmless on other
/// backends.
void addGlResourceNames(const Program& program,
                        std::string_view vertexEntryPoint,
                        std::string_view fragmentEntryPoint,
                        igl::RenderPipelineDesc& desc);
void addGlResourceNames(const Program& program,
                        std::string_view computeEntryPoint,
                        igl::ComputePipelineDesc& desc);

} // namespace iglu::slang
