/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/RenderPipelineReflection.h>

#include <igl/webgpu/Common.h>

namespace igl::webgpu {

namespace {

TextureType toTextureType(WGPUTextureViewDimension dimension) {
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (dimension) {
  case WGPUTextureViewDimension_2D:
    return TextureType::TwoD;
  case WGPUTextureViewDimension_2DArray:
    return TextureType::TwoDArray;
  case WGPUTextureViewDimension_Cube:
    return TextureType::Cube;
  case WGPUTextureViewDimension_3D:
    return TextureType::ThreeD;
  default:
    return TextureType::Invalid;
  }
}

bool isBuffer(WgslBindingKind kind) {
  return kind == WgslBindingKind::UniformBuffer || kind == WgslBindingKind::StorageBuffer ||
         kind == WgslBindingKind::ReadOnlyStorageBuffer;
}

bool isSampledTexture(WgslBindingKind kind) {
  return kind == WgslBindingKind::Texture || kind == WgslBindingKind::DepthTexture ||
         kind == WgslBindingKind::MultisampledTexture;
}

bool isSampler(WgslBindingKind kind) {
  return kind == WgslBindingKind::Sampler || kind == WgslBindingKind::ComparisonSampler;
}

} // namespace

RenderPipelineReflection::RenderPipelineReflection(
    const std::vector<std::pair<ShaderStage, const WgslReflection*>>& stages) {
  for (const auto& [stage, reflection] : stages) {
    if (reflection == nullptr) {
      continue;
    }
    for (const WgslBinding& binding : reflection->bindings) {
      if (binding.group == kBufferGroup && isBuffer(binding.kind)) {
        BufferArgDesc desc{
            .name = genNameHandle(binding.name),
            .bufferDataSize = binding.bufferSize,
            .bufferIndex = static_cast<int>(binding.binding),
            .shaderStage = stage,
        };
        if (const WgslStruct* s = reflection->findStruct(binding.type)) {
          desc.bufferAlignment = s->align;
          for (const WgslStructMember& member : s->members) {
            desc.members.push_back({
                .name = genNameHandle(member.name),
                .type = member.uniformType,
                .offset = member.offset,
                .arrayLength = member.arrayLength,
                .arrayStride = member.arrayStride,
            });
          }
        }
        buffers_.push_back(std::move(desc));
      } else if (binding.group == kTextureGroup && binding.binding % 2 == 0 &&
                 isSampledTexture(binding.kind)) {
        textures_.push_back({
            .name = binding.name,
            .type = toTextureType(binding.viewDimension),
            .textureIndex = static_cast<int>(binding.binding / 2),
            .shaderStage = stage,
        });
      } else if (binding.group == kTextureGroup && binding.binding % 2 == 1 &&
                 isSampler(binding.kind)) {
        samplers_.push_back({
            .name = binding.name,
            .samplerIndex = static_cast<int>(binding.binding / 2),
            .shaderStage = stage,
        });
      } else if (binding.group == kStorageTextureGroup &&
                 binding.kind == WgslBindingKind::StorageTexture) {
        textures_.push_back({
            .name = binding.name,
            .type = toTextureType(binding.viewDimension),
            .textureIndex = static_cast<int>(binding.binding),
            .shaderStage = stage,
        });
      }
    }
  }
}

int RenderPipelineReflection::getIndexByName(const std::string& name, ShaderStage stage) const {
  for (const BufferArgDesc& buffer : buffers_) {
    if (buffer.shaderStage == stage && buffer.name.toString() == name) {
      return buffer.bufferIndex;
    }
  }
  for (const TextureArgDesc& texture : textures_) {
    if (texture.shaderStage == stage && texture.name == name) {
      return texture.textureIndex;
    }
  }
  for (const SamplerArgDesc& sampler : samplers_) {
    if (sampler.shaderStage == stage && sampler.name == name) {
      return sampler.samplerIndex;
    }
  }
  return -1;
}

} // namespace igl::webgpu
