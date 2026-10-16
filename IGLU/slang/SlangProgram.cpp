/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/slang/SlangProgram.h>

#include <igl/DeviceFeatures.h>
#include <igl/NameHandle.h>
#include <igl/ShaderCreator.h>

namespace iglu::slang {

namespace {

// GLSL 410/ES 300 for render stages and GLSL 430/ES 310 for compute; anything older has no
// Slang code. IGL reports GLSL versions as `#version` spells them (ES 3.1 is 3.10).
const SourceChunks* IGL_NULLABLE glslFor(const igl::IDevice& device, const EntryPoint& entryPoint) {
  const igl::ShaderVersion version = device.getShaderVersion();
  const int v = version.majorVersion * 100 + version.minorVersion;
  const bool compute = entryPoint.stage == igl::ShaderStage::Compute;
  if (version.family == igl::ShaderFamily::GlslEs) {
    return v >= (compute ? 310 : 300) && !entryPoint.essl.empty() ? &entryPoint.essl : nullptr;
  }
  if (version.family == igl::ShaderFamily::Glsl) {
    return v >= (compute ? 430 : 410) && !entryPoint.glsl.empty() ? &entryPoint.glsl : nullptr;
  }
  return nullptr;
}

uint32_t entryPointBit(const Program& program, std::string_view name) {
  const EntryPoint* entryPoint = findEntryPoint(program, name);
  return entryPoint != nullptr ? 1u << static_cast<uint32_t>(entryPoint - program.entryPoints) : 0u;
}

void setUnsupported(igl::Result* IGL_NULLABLE outResult, const Program& program) {
  igl::Result::setResult(
      outResult,
      igl::Result::Code::Unsupported,
      std::string("Slang program ") + program.name + " has no code for this backend");
}

} // namespace

std::string SourceChunks::join() const {
  std::string source;
  for (size_t i = 0; i < count; ++i) {
    source += chunks[i];
  }
  return source;
}

const EntryPoint* IGL_NULLABLE findEntryPoint(const Program& program, std::string_view name) {
  for (size_t i = 0; i < program.numEntryPoints; ++i) {
    if (name == program.entryPoints[i].name) {
      return &program.entryPoints[i];
    }
  }
  return nullptr;
}

std::unique_ptr<igl::IShaderStages> createRenderStages(const igl::IDevice& device,
                                                       const Program& program,
                                                       std::string_view vertexEntryPoint,
                                                       std::string_view fragmentEntryPoint,
                                                       igl::Result* IGL_NULLABLE outResult) {
  const EntryPoint* vertex = findEntryPoint(program, vertexEntryPoint);
  const EntryPoint* fragment = findEntryPoint(program, fragmentEntryPoint);
  if (vertex == nullptr || fragment == nullptr || vertex->stage != igl::ShaderStage::Vertex ||
      fragment->stage != igl::ShaderStage::Fragment) {
    igl::Result::setResult(outResult,
                           igl::Result::Code::ArgumentInvalid,
                           std::string("No such vertex/fragment entry points in ") + program.name);
    return nullptr;
  }
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (device.getBackendType()) {
  case igl::BackendType::Vulkan:
    if (vertex->spirv == nullptr || fragment->spirv == nullptr) {
      break;
    }
    return igl::ShaderStagesCreator::fromModuleBinaryInput(device,
                                                           vertex->spirv,
                                                           vertex->spirvWords * sizeof(uint32_t),
                                                           "main",
                                                           vertex->name,
                                                           fragment->spirv,
                                                           fragment->spirvWords * sizeof(uint32_t),
                                                           "main",
                                                           fragment->name,
                                                           outResult);
  case igl::BackendType::Metal:
    if (program.msl.empty()) {
      break;
    }
    return igl::ShaderStagesCreator::fromLibraryStringInput(
        device, program.msl.join().c_str(), vertex->name, fragment->name, program.name, outResult);
  case igl::BackendType::D3D12:
    if (vertex->hlsl.empty() || fragment->hlsl.empty()) {
      break;
    }
    return igl::ShaderStagesCreator::fromModuleStringInput(device,
                                                           vertex->hlsl.join().c_str(),
                                                           vertex->name,
                                                           vertex->name,
                                                           fragment->hlsl.join().c_str(),
                                                           fragment->name,
                                                           fragment->name,
                                                           outResult);
  case igl::BackendType::WebGPU: {
    if (program.wgsl.empty()) {
      break;
    }
    const std::string wgsl = program.wgsl.join();
    return igl::ShaderStagesCreator::fromModuleStringInput(device,
                                                           wgsl.c_str(),
                                                           vertex->name,
                                                           vertex->name,
                                                           wgsl.c_str(),
                                                           fragment->name,
                                                           fragment->name,
                                                           outResult);
  }
  case igl::BackendType::OpenGL: {
    const SourceChunks* vertexGlsl = glslFor(device, *vertex);
    const SourceChunks* fragmentGlsl = glslFor(device, *fragment);
    if (vertexGlsl == nullptr || fragmentGlsl == nullptr) {
      break;
    }
    return igl::ShaderStagesCreator::fromModuleStringInput(device,
                                                           vertexGlsl->join().c_str(),
                                                           "main",
                                                           vertex->name,
                                                           fragmentGlsl->join().c_str(),
                                                           "main",
                                                           fragment->name,
                                                           outResult);
  }
  case igl::BackendType::Invalid:
  case igl::BackendType::Custom:
  default:
    break;
  }
  setUnsupported(outResult, program);
  return nullptr;
}

std::unique_ptr<igl::IShaderStages> createMeshRenderStages(const igl::IDevice& device,
                                                           const Program& meshProgram,
                                                           std::string_view taskEntryPoint,
                                                           std::string_view meshEntryPoint,
                                                           const Program& fragmentProgram,
                                                           std::string_view fragmentEntryPoint,
                                                           igl::Result* IGL_NULLABLE outResult) {
  const EntryPoint* task = taskEntryPoint.empty() ? nullptr
                                                  : findEntryPoint(meshProgram, taskEntryPoint);
  const EntryPoint* mesh = findEntryPoint(meshProgram, meshEntryPoint);
  const EntryPoint* fragment = findEntryPoint(fragmentProgram, fragmentEntryPoint);
  if ((!taskEntryPoint.empty() && (task == nullptr || task->stage != igl::ShaderStage::Task)) ||
      mesh == nullptr || mesh->stage != igl::ShaderStage::Mesh || fragment == nullptr ||
      fragment->stage != igl::ShaderStage::Fragment) {
    igl::Result::setResult(outResult,
                           igl::Result::Code::ArgumentInvalid,
                           std::string("No such task/mesh entry points in ") + meshProgram.name +
                               " or fragment entry point in " + fragmentProgram.name);
    return nullptr;
  }
  const bool vulkan = device.getBackendType() == igl::BackendType::Vulkan;
  const bool metal = device.getBackendType() == igl::BackendType::Metal;
  const auto hasCode = [vulkan, metal](const Program& program, const EntryPoint& entryPoint) {
    return vulkan ? entryPoint.spirv != nullptr : metal && !program.msl.empty();
  };
  if ((task != nullptr && !hasCode(meshProgram, *task)) || !hasCode(meshProgram, *mesh) ||
      !hasCode(fragmentProgram, *fragment)) {
    setUnsupported(outResult, meshProgram);
    return nullptr;
  }
  const auto module = [&device, vulkan, outResult](const Program& program,
                                                   const EntryPoint& entryPoint) {
    return vulkan ? igl::ShaderModuleCreator::fromBinaryInput(
                        device,
                        entryPoint.spirv,
                        entryPoint.spirvWords * sizeof(uint32_t),
                        {.stage = entryPoint.stage, .entryPoint = "main"},
                        entryPoint.name,
                        outResult)
                  : igl::ShaderModuleCreator::fromStringInput(
                        device,
                        program.msl.join().c_str(),
                        {.stage = entryPoint.stage, .entryPoint = entryPoint.name},
                        entryPoint.name,
                        outResult);
  };
  std::shared_ptr<igl::IShaderModule> taskModule;
  if (task != nullptr) {
    taskModule = module(meshProgram, *task);
  }
  std::shared_ptr<igl::IShaderModule> meshModule = module(meshProgram, *mesh);
  std::shared_ptr<igl::IShaderModule> fragmentModule = module(fragmentProgram, *fragment);
  if ((task != nullptr && taskModule == nullptr) || meshModule == nullptr ||
      fragmentModule == nullptr) {
    return nullptr;
  }
  return igl::ShaderStagesCreator::fromMeshRenderModules(
      device, std::move(taskModule), std::move(meshModule), std::move(fragmentModule), outResult);
}

std::unique_ptr<igl::IShaderStages> createComputeStages(const igl::IDevice& device,
                                                        const Program& program,
                                                        std::string_view computeEntryPoint,
                                                        igl::Result* IGL_NULLABLE outResult) {
  const EntryPoint* compute = findEntryPoint(program, computeEntryPoint);
  if (compute == nullptr || compute->stage != igl::ShaderStage::Compute) {
    igl::Result::setResult(outResult,
                           igl::Result::Code::ArgumentInvalid,
                           std::string("No such compute entry point in ") + program.name);
    return nullptr;
  }
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (device.getBackendType()) {
  case igl::BackendType::Vulkan:
    if (compute->spirv == nullptr) {
      break;
    }
    return igl::ShaderStagesCreator::fromModuleBinaryInput(device,
                                                           compute->spirv,
                                                           compute->spirvWords * sizeof(uint32_t),
                                                           "main",
                                                           compute->name,
                                                           outResult);
  case igl::BackendType::Metal:
    if (program.msl.empty()) {
      break;
    }
    return igl::ShaderStagesCreator::fromModuleStringInput(
        device, program.msl.join().c_str(), compute->name, compute->name, outResult);
  case igl::BackendType::D3D12:
    if (compute->hlsl.empty()) {
      break;
    }
    return igl::ShaderStagesCreator::fromModuleStringInput(
        device, compute->hlsl.join().c_str(), compute->name, compute->name, outResult);
  case igl::BackendType::WebGPU:
    if (program.wgsl.empty()) {
      break;
    }
    return igl::ShaderStagesCreator::fromModuleStringInput(
        device, program.wgsl.join().c_str(), compute->name, compute->name, outResult);
  case igl::BackendType::OpenGL: {
    const SourceChunks* glsl = glslFor(device, *compute);
    if (glsl == nullptr) {
      break;
    }
    return igl::ShaderStagesCreator::fromModuleStringInput(
        device, glsl->join().c_str(), "main", compute->name, outResult);
  }
  case igl::BackendType::Invalid:
  case igl::BackendType::Custom:
  default:
    break;
  }
  setUnsupported(outResult, program);
  return nullptr;
}

igl::Dimensions workgroupSize(const EntryPoint& entryPoint) {
  return {entryPoint.workgroupSize[0], entryPoint.workgroupSize[1], entryPoint.workgroupSize[2]};
}

void addGlResourceNames(const Program& program,
                        std::string_view vertexEntryPoint,
                        std::string_view fragmentEntryPoint,
                        igl::RenderPipelineDesc& desc) {
  const uint32_t vertexMask = entryPointBit(program, vertexEntryPoint);
  const uint32_t fragmentMask = entryPointBit(program, fragmentEntryPoint);
  for (size_t i = 0; i < program.numGlResources; ++i) {
    const GlResourceName& resource = program.glResources[i];
    const bool vertex = (resource.entryPointMask & vertexMask) != 0;
    const bool fragment = (resource.entryPointMask & fragmentMask) != 0;
    switch (resource.kind) {
    case GlResourceKind::Sampler:
      if (vertex) {
        desc.vertexUnitSamplerMap[resource.index] = igl::genNameHandle(resource.name);
      }
      if (fragment) {
        desc.fragmentUnitSamplerMap[resource.index] = igl::genNameHandle(resource.name);
      }
      break;
    case GlResourceKind::UniformBlock:
      if (vertex || fragment) {
        desc.uniformBlockBindingMap[resource.index].emplace_back(
            igl::genNameHandle(resource.name), igl::genNameHandle(resource.instanceName));
      }
      break;
    case GlResourceKind::StorageBlock:
      break;
    }
  }
}

void addGlResourceNames(const Program& program,
                        std::string_view computeEntryPoint,
                        igl::ComputePipelineDesc& desc) {
  const uint32_t mask = entryPointBit(program, computeEntryPoint);
  for (size_t i = 0; i < program.numGlResources; ++i) {
    const GlResourceName& resource = program.glResources[i];
    if (resource.kind == GlResourceKind::StorageBlock && (resource.entryPointMask & mask) != 0) {
      desc.buffersMap[resource.index] = igl::genNameHandle(resource.name);
    }
  }
}

} // namespace iglu::slang
