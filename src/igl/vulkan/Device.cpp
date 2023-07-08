/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Device.h>

#include <cstring>
#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/CommandQueue.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/ComputePipelineState.h>
#include <igl/vulkan/Framebuffer.h>
#include <igl/vulkan/PlatformDevice.h>
#include <igl/vulkan/RenderPipelineState.h>
#include <igl/vulkan/SamplerState.h>
#include <igl/vulkan/ShaderModule.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanShaderModule.h>

#if IGL_SHADER_DUMP && IGL_DEBUG
#include <filesystem>
#include <fstream>
#endif // IGL_SHADER_DUMP && IGL_DEBUG

namespace {

bool supportsFormat(VkPhysicalDevice physicalDevice, VkFormat format) {
  VkFormatProperties properties;
  vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
  return properties.bufferFeatures != 0 || properties.linearTilingFeatures != 0 ||
         properties.optimalTilingFeatures != 0;
}

VkShaderStageFlagBits shaderStageToVkShaderStage(igl::ShaderStage stage) {
  switch (stage) {
  case igl::Stage_Vertex:
    return VK_SHADER_STAGE_VERTEX_BIT;
  case igl::Stage_Fragment:
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  case igl::Stage_Compute:
    return VK_SHADER_STAGE_COMPUTE_BIT;
  };
  return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
}

} // namespace

namespace igl {
namespace vulkan {

Device::Device(std::unique_ptr<VulkanContext> ctx) : ctx_(std::move(ctx)), platformDevice_(*this) {}

std::shared_ptr<ICommandQueue> Device::createCommandQueue(CommandQueueType type,
                                                          Result* outResult) {
  auto resource = std::make_shared<CommandQueue>(*this, type);
  Result::setOk(outResult);
  return resource;
}

std::unique_ptr<IBuffer> Device::createBuffer(const BufferDesc& desc,
                                              Result* outResult) const noexcept {
  auto buffer = std::make_unique<vulkan::Buffer>(*this);

  const auto result = buffer->create(desc);

  if (!IGL_VERIFY(result.isOk())) {
    return nullptr;
  }

  if (!desc.data) {
    return buffer;
  }

  const auto uploadResult = buffer->upload(desc.data, BufferRange(desc.length, 0u));
  IGL_VERIFY(uploadResult.isOk());
  Result::setResult(outResult, uploadResult);

  return buffer;
}

std::shared_ptr<ISamplerState> Device::createSamplerState(const SamplerStateDesc& desc,
                                                          Result* outResult) const {
  auto samplerState = std::make_shared<vulkan::SamplerState>(*this);

  Result::setResult(outResult, samplerState->create(desc));

  return samplerState;
}

std::shared_ptr<ITexture> Device::createTexture(const TextureDesc& desc,
                                                Result* outResult) const noexcept {
  const auto sanitized = sanitize(desc);

  auto texture = std::make_shared<vulkan::Texture>(*this, desc.format);

  const Result res = texture->create(sanitized);

  Result::setResult(outResult, res);

  return res.isOk() ? texture : nullptr;
}

std::shared_ptr<IComputePipelineState> Device::createComputePipeline(
    const ComputePipelineDesc& desc,
    Result* outResult) const {
  if (IGL_UNEXPECTED(desc.shaderStages == nullptr)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Missing shader stages");
    return nullptr;
  }
  if (!IGL_VERIFY(desc.shaderStages->getModule(Stage_Compute))) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Missing compute shader");
    return nullptr;
  }

  Result::setOk(outResult);
  return std::make_shared<ComputePipelineState>(*this, desc);
}

std::shared_ptr<IRenderPipelineState> Device::createRenderPipeline(const RenderPipelineDesc& desc,
                                                                   Result* outResult) const {
  if (IGL_UNEXPECTED(desc.shaderStages == nullptr)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Missing shader stages");
    return nullptr;
  }

  const bool hasColorAttachments = !desc.targetDesc.colorAttachments.empty();
  const bool hasDepthAttachment = desc.targetDesc.depthAttachmentFormat != TextureFormat::Invalid;
  const bool hasAnyAttachments = hasColorAttachments || hasDepthAttachment;
  if (!IGL_VERIFY(hasAnyAttachments)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Need at least one attachment");
    return nullptr;
  }

  if (!IGL_VERIFY(desc.shaderStages->getModule(Stage_Vertex))) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Missing vertex shader");
    return nullptr;
  }

  if (!IGL_VERIFY(desc.shaderStages->getModule(Stage_Fragment))) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Missing fragment shader");
    return nullptr;
  }

  return std::make_shared<RenderPipelineState>(*this, desc);
}

std::shared_ptr<IShaderModule> Device::createShaderModule(const ShaderModuleDesc& desc,
                                                          Result* outResult) const {
  std::shared_ptr<VulkanShaderModule> vulkanShaderModule;
  Result result;
  if (desc.dataSize) {
    // binary
    vulkanShaderModule = createShaderModule(desc.data, desc.dataSize, desc.debugName, &result);
  } else {
    // text
    vulkanShaderModule = createShaderModule(desc.stage, desc.data, desc.debugName, &result);
  }

  if (!result.isOk()) {
    Result::setResult(outResult, std::move(result));
    return nullptr;
  }
  Result::setResult(outResult, std::move(result));
  return std::make_shared<ShaderModule>(desc, std::move(vulkanShaderModule));
}

std::shared_ptr<VulkanShaderModule> Device::createShaderModule(const void* data,
                                                               size_t length,
                                                               const std::string& debugName,
                                                               Result* outResult) const {
  VkDevice device = ctx_->device_->getVkDevice();

#if IGL_SHADER_DUMP && IGL_DEBUG
  uint64_t hash = 0;
  IGL_ASSERT(length % sizeof(uint32_t) == 0);
  auto words = reinterpret_cast<const uint32_t*>(data);
  for (int i = 0; i < (length / sizeof(uint32_t)); i++) {
    hash ^= std::hash<uint32_t>()(words[i]);
  }
  // Replace filename with your own path according to the platform and recompile.
  // Ex. for Android your filepath should be specific to the package name:
  // /sdcard/Android/data/<packageName>/files/
  std::string filename = fmt::format("{}{}{}.spv", PATH_HERE, debugName, std::to_string(hash));
  if (!std::filesystem::exists(filename)) {
    std::ofstream spirvFile;
    spirvFile.open(filename, std::ios::out | std::ios::binary);
    for (int i = 0; i < length / (int)sizeof(uint32_t); i++) {
      spirvFile.write(reinterpret_cast<const char*>(&words[i]), sizeof(uint32_t));
    }
    spirvFile.close();
  }
#endif // IGL_SHADER_DUMP && IGL_DEBUG

  VkShaderModule vkShaderModule = VK_NULL_HANDLE;
  const VkResult result = ivkCreateShaderModuleFromSPIRV(device, data, length, &vkShaderModule);

  setResultFrom(outResult, result);

  if (result != VK_SUCCESS) {
    return nullptr;
  }

  if (!debugName.empty()) {
    // set debug name
    VK_ASSERT(ivkSetDebugObjectName(
        device, VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)vkShaderModule, debugName.c_str()));
  }

  return std::make_shared<VulkanShaderModule>(device, vkShaderModule);
}

std::shared_ptr<VulkanShaderModule> Device::createShaderModule(ShaderStage stage,
                                                               const char* source,
                                                               const std::string& debugName,
                                                               Result* outResult) const {
  VkDevice device = ctx_->device_->getVkDevice();
  const VkShaderStageFlagBits vkStage = shaderStageToVkShaderStage(stage);
  IGL_ASSERT(vkStage != VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);
  IGL_ASSERT(source);

  std::string sourcePatched;

  if (!source || !*source) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "Shader source is empty");
    return nullptr;
  }

  if (strstr(source, "#version ") == nullptr) {
    std::string extraExtensions;

    // GL_EXT_debug_printf extension
    if (ctx_->extensions_.enabled(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME)) {
      extraExtensions += "#extension GL_EXT_debug_printf : enable\n";
    }

    // there's no header provided in the shader source, let's insert our own header
    if (vkStage == VK_SHADER_STAGE_VERTEX_BIT || vkStage == VK_SHADER_STAGE_COMPUTE_BIT) {
      sourcePatched += R"(
      #version 460
      )" + extraExtensions +
                       R"(
      #extension GL_EXT_nonuniform_qualifier : require
      #extension GL_EXT_buffer_reference : require
      #extension GL_EXT_buffer_reference_uvec2 : require
      #extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
      )";
    }
    if (vkStage == VK_SHADER_STAGE_FRAGMENT_BIT) {
      sourcePatched += R"(
      #version 460
      )" + extraExtensions +
                       R"(
      #extension GL_EXT_nonuniform_qualifier : require
      #extension GL_EXT_buffer_reference_uvec2 : require
      #extension GL_EXT_shader_explicit_arithmetic_types_float16 : require

      layout (set = 0, binding = 0) uniform texture2D kTextures2D[];
      layout (set = 0, binding = 1) uniform texture2DArray kTextures2DArray[];
      layout (set = 0, binding = 2) uniform texture3D kTextures3D[];
      layout (set = 0, binding = 3) uniform textureCube kTexturesCube[];
      layout (set = 0, binding = 4) uniform sampler kSamplers[];
      layout (set = 0, binding = 5) uniform samplerShadow kSamplersShadow[];
      // binding #6 is reserved for STORAGE_IMAGEs: check VulkanContext.cpp

      vec4 textureBindless2D(uint textureid, uint samplerid, vec2 uv) {
        return texture(sampler2D(kTextures2D[textureid], kSamplers[samplerid]), uv);
      }
      float textureBindless2DShadow(uint textureid, uint samplerid, vec3 uvw) {
        return texture(sampler2DShadow(kTextures2D[textureid], kSamplersShadow[samplerid]), uvw);
      }
      ivec2 textureBindlessSize2D(uint textureid, uint samplerid) {
        return textureSize(sampler2D(kTextures2D[textureid], kSamplers[samplerid]), 0);
      }
      vec4 textureBindlessCube(uint textureid, uint samplerid, vec3 uvw) {
        return texture(samplerCube(kTexturesCube[textureid], kSamplers[samplerid]), uvw);
      }
      )";
    }
    sourcePatched += source;
    source = sourcePatched.c_str();
  }

  glslang_resource_t glslangResource;
  ivkGlslangResource(&glslangResource, &ctx_->getVkPhysicalDeviceProperties());

  VkShaderModule vkShaderModule = VK_NULL_HANDLE;
  const Result result =
      igl::vulkan::compileShader(device, vkStage, source, &vkShaderModule, &glslangResource);

  Result::setResult(outResult, result);

  if (!result.isOk()) {
    return nullptr;
  }

  if (!debugName.empty()) {
    // set debug name
    VK_ASSERT(ivkSetDebugObjectName(
        device, VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)vkShaderModule, debugName.c_str()));
  }

  return std::make_shared<VulkanShaderModule>(device, vkShaderModule);
}

std::shared_ptr<IFramebuffer> Device::createFramebuffer(const FramebufferDesc& desc,
                                                        Result* outResult) {
  auto resource = std::make_shared<Framebuffer>(*this, desc);
  Result::setOk(outResult);
  return resource;
}

const PlatformDevice& Device::getPlatformDevice() const noexcept {
  return platformDevice_;
}

} // namespace vulkan
} // namespace igl
