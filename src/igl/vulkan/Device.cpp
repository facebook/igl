/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Device.h>

#include <cstring>
#include <igl/glslang/GlslCompiler.h>
#include <igl/glslang/GlslangHelpers.h>
#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/CommandQueue.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/ComputePipelineState.h>
#include <igl/vulkan/EnhancedShaderDebuggingStore.h>
#include <igl/vulkan/Framebuffer.h>
#include <igl/vulkan/PlatformDevice.h>
#include <igl/vulkan/RenderPipelineState.h>
#include <igl/vulkan/SamplerState.h>
#include <igl/vulkan/ShaderModule.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanBuffer.h>
#include <igl/vulkan/VulkanShaderModule.h>

// Writes the shader code to disk for debugging. Used in `Device::createShaderModule()`
#if IGL_SHADER_DUMP && IGL_DEBUG
#include <filesystem>
#include <fstream>
#endif // IGL_SHADER_DUMP && IGL_DEBUG

namespace {

#if IGL_SHADER_DUMP && IGL_DEBUG
std::string sanitizeFileName(const std::string& fileName) {
  std::string result;
  for (const char c : fileName) {
    if (std::isalnum(c) || c == '.' || c == '_' || c == '-') {
      result += c;
    } else {
      result += '_';
    }
  }
  return result;
}
#endif // IGL_SHADER_DUMP && IGL_DEBUG

bool supportsFormat(const VulkanFunctionTable& vf,
                    VkPhysicalDevice physicalDevice,
                    VkFormat format) {
  VkFormatProperties properties;
  vf.vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
  return properties.bufferFeatures != 0 || properties.linearTilingFeatures != 0 ||
         properties.optimalTilingFeatures != 0;
}

VkShaderStageFlagBits shaderStageToVkShaderStage(igl::ShaderStage stage) {
  switch (stage) {
  case igl::ShaderStage::Vertex:
    return VK_SHADER_STAGE_VERTEX_BIT;
  case igl::ShaderStage::Fragment:
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  case igl::ShaderStage::Compute:
    return VK_SHADER_STAGE_COMPUTE_BIT;
  case igl::ShaderStage::Task:
    return VK_SHADER_STAGE_TASK_BIT_EXT;
  case igl::ShaderStage::Mesh:
    return VK_SHADER_STAGE_MESH_BIT_EXT;
  };
  return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
}

} // namespace

namespace igl::vulkan {

Device::Device(std::unique_ptr<VulkanContext> ctx) : ctx_(std::move(ctx)), platformDevice_(*this) {
  if (ctx_->enhancedShaderDebuggingStore_) {
    ctx_->enhancedShaderDebuggingStore_->initialize(this);
  }
}

std::shared_ptr<ICommandQueue> Device::createCommandQueueInternal(const CommandQueueDesc& desc,
                                                                  Result* IGL_NULLABLE outResult) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  Result::setOk(outResult);
  auto resource = std::make_shared<CommandQueue>(*this, desc);
  return resource;
}

std::unique_ptr<IBuffer> Device::createBufferInternal( // NOLINT(bugprone-exception-escape)
    const BufferDesc& desc,
    Result* IGL_NULLABLE outResult) const noexcept {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  auto buffer = std::make_unique<Buffer>(*this);

  const auto result = buffer->create(desc);

  if (!IGL_DEBUG_VERIFY(result.isOk())) {
    return nullptr;
  }

  if (!desc.data) {
    return buffer;
  }

  const auto uploadResult = buffer->upload(desc.data, BufferRange(desc.length, 0u));
  IGL_DEBUG_ASSERT(uploadResult.isOk());
  Result::setResult(outResult, uploadResult);

  if (hasResourceTracker()) {
    buffer->initResourceTracker(getResourceTracker(), desc.debugName);
  }

  return buffer;
}

std::shared_ptr<IDepthStencilState> Device::createDepthStencilStateInternal(
    const DepthStencilStateDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  Result::setOk(outResult);
  return std::make_shared<DepthStencilState>(desc);
}

std::unique_ptr<IShaderStages> Device::createShaderStagesInternal(const ShaderStagesDesc& desc,
                                                                  Result* IGL_NULLABLE
                                                                      outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  auto shaderStages = std::make_unique<ShaderStages>(desc);
  if (shaderStages == nullptr) {
    Result::setResult(
        outResult, Result::Code::RuntimeError, "Could not instantiate shader stages.");
  } else if (!shaderStages->isValid()) {
    Result::setResult(
        outResult, Result::Code::ArgumentInvalid, "Missing required shader module(s).");
  } else {
    Result::setOk(outResult);
  }

  if (hasResourceTracker()) {
    shaderStages->initResourceTracker(getResourceTracker(), desc.debugName);
  }

  return shaderStages;
}

std::shared_ptr<ISamplerState> Device::createSamplerStateInternal(const SamplerStateDesc& desc,
                                                                  Result* IGL_NULLABLE
                                                                      outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  auto samplerState = std::make_shared<SamplerState>(const_cast<Device&>(*this));

  Result::setResult(outResult, samplerState->create(desc));

  if (hasResourceTracker()) {
    samplerState->initResourceTracker(getResourceTracker(), desc.debugName);
  }

  return samplerState;
}

std::shared_ptr<ITexture> Device::createTextureInternal( // NOLINT(bugprone-exception-escape)
    const TextureDesc& desc,
    Result* IGL_NULLABLE outResult) const noexcept {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  const auto sanitized = sanitize(desc);

  auto texture = std::make_shared<Texture>(const_cast<Device&>(*this), desc.format);

  const Result res = texture->create(sanitized);

  if (hasResourceTracker()) {
    texture->initResourceTracker(getResourceTracker(), desc.debugName);
  }

  Result::setResult(outResult, res);

  return res.isOk() ? texture : nullptr;
}

std::shared_ptr<ITexture> Device::createTextureView( // NOLINT(bugprone-exception-escape)
    std::shared_ptr<ITexture> texture,
    const TextureViewDesc& desc,
    Result* IGL_NULLABLE outResult) const noexcept {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  if (!IGL_DEBUG_VERIFY(texture)) {
    Result::setResult(outResult,
                      Result(Result::Code::ArgumentInvalid, "A base texture should be specified"));
    return {};
  }

  const Texture& baseTexture = static_cast<Texture&>(*texture);

  auto newTexture = std::make_shared<Texture>(
      const_cast<Device&>(*this),
      desc.format == TextureFormat::Invalid ? baseTexture.getFormat() : desc.format);

  const Result res = newTexture->createView(baseTexture, desc);

  if (hasResourceTracker()) {
    newTexture->initResourceTracker(getResourceTracker(), desc.debugName);
  }

  Result::setResult(outResult, res);

  return res.isOk() ? newTexture : nullptr;
}

std::shared_ptr<IVertexInputState> Device::createVertexInputStateInternal(
    const VertexInputStateDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  // VertexInputState is compiled into the RenderPipelineState at a later stage. For now, we just
  // have to store the description.
  Result::setOk(outResult);

  return std::make_shared<VertexInputState>(desc);
}

std::shared_ptr<IComputePipelineState> Device::createComputePipelineInternal(
    const ComputePipelineDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  if (IGL_DEBUG_VERIFY_NOT(desc.shaderStages == nullptr)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Missing shader stages");
    return nullptr;
  }
  if (!IGL_DEBUG_VERIFY(desc.shaderStages->getType() == ShaderStagesType::Compute)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Shader stages not for compute");
    return nullptr;
  }
  if (!IGL_DEBUG_VERIFY(desc.shaderStages->getComputeModule())) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Missing compute shader");
    return nullptr;
  }

  Result::setOk(outResult);
  return std::make_shared<ComputePipelineState>(*this, desc);
}

std::shared_ptr<IRenderPipelineState> Device::createRenderPipelineInternal(
    const RenderPipelineDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  if (IGL_DEBUG_VERIFY_NOT(desc.shaderStages == nullptr)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Missing shader stages");
    return nullptr;
  }
  if (!IGL_DEBUG_VERIFY(desc.shaderStages->getType() == ShaderStagesType::Render || desc.shaderStages->getType() == ShaderStagesType::MeshRender)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Shader stages not for render");
    return nullptr;
  }

  const bool hasColorAttachments = !desc.targetDesc.colorAttachments.empty();
  const bool hasDepthAttachment = desc.targetDesc.depthAttachmentFormat != TextureFormat::Invalid;
  const bool hasAnyAttachments = hasColorAttachments || hasDepthAttachment;
  if (!IGL_DEBUG_VERIFY(hasAnyAttachments)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Need at least one attachment");
    return nullptr;
  }

  if (desc.shaderStages->getType() == ShaderStagesType::Render && !IGL_DEBUG_VERIFY(desc.shaderStages->getVertexModule())) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Missing vertex shader");
    return nullptr;
  }

  if (desc.shaderStages->getType() == ShaderStagesType::MeshRender && !IGL_DEBUG_VERIFY(desc.shaderStages->getMeshModule())) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Missing mesh shader");
    return nullptr;
  }

  if (!IGL_DEBUG_VERIFY(desc.shaderStages->getFragmentModule())) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Missing fragment shader");
    return nullptr;
  }

  return std::make_shared<RenderPipelineState>(*this, desc);
}

std::shared_ptr<IShaderModule> Device::createShaderModuleInternal(const ShaderModuleDesc& desc,
                                                                  Result* IGL_NULLABLE
                                                                      outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  std::shared_ptr<VulkanShaderModule> vulkanShaderModule;
  Result result;
  if (desc.input.type == ShaderInputType::Binary) {
    vulkanShaderModule =
        createShaderModule(desc.input.data, desc.input.length, desc.debugName, &result);
  } else {
    vulkanShaderModule =
        createShaderModule(desc.info.stage, desc.input.source, desc.debugName, &result);
  }

  if (!result.isOk()) {
    Result::setResult(outResult, std::move(result));
    return nullptr;
  }
  Result::setResult(outResult, std::move(result));
  auto shaderModule = std::make_shared<ShaderModule>(desc.info, std::move(vulkanShaderModule));

  if (hasResourceTracker()) {
    shaderModule->initResourceTracker(getResourceTracker(), desc.debugName);
  }

  return shaderModule;
}

std::shared_ptr<VulkanShaderModule> Device::createShaderModule(const void* IGL_NULLABLE data,
                                                               size_t length,
                                                               const std::string& debugName,
                                                               Result* IGL_NULLABLE
                                                                   outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

#if IGL_SHADER_DUMP && IGL_DEBUG
  uint64_t hash = 0;
  IGL_DEBUG_ASSERT(length % sizeof(uint32_t) == 0);
  auto words = reinterpret_cast<const uint32_t*>(data);
  for (int i = 0; i < (length / sizeof(uint32_t)); i++) {
    hash ^= std::hash<uint32_t>()(words[i]);
  }
  const std::string filename = IGL_FORMAT(
      "{}{}{}.spv", IGL_SHADER_DUMP_PATH, sanitizeFileName(debugName), std::to_string(hash));
  IGL_LOG_INFO("Dumping shader to: %s", filename.c_str());
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
  const VkShaderModuleCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = length,
      .pCode = static_cast<const uint32_t*>(data),
  };
  const VkResult result =
      ctx_->vf_.vkCreateShaderModule(ctx_->getVkDevice(), &ci, nullptr, &vkShaderModule);

  setResultFrom(outResult, result);

  if (result != VK_SUCCESS) {
    return nullptr;
  }

  if (!debugName.empty()) {
    // set debug name
    VK_ASSERT(ivkSetDebugObjectName(&ctx_->vf_,
                                    ctx_->getVkDevice(),
                                    VK_OBJECT_TYPE_SHADER_MODULE,
                                    (uint64_t)vkShaderModule,
                                    debugName.c_str()));
  }

  IGL_DEBUG_ASSERT(vkShaderModule != VK_NULL_HANDLE);
  return std::make_shared<VulkanShaderModule>(
      ctx_->vf_,
      ctx_->getVkDevice(),
      vkShaderModule,
      util::getReflectionData(reinterpret_cast<const uint32_t*>(data), length));
}

std::shared_ptr<VulkanShaderModule> Device::createShaderModule(ShaderStage stage,
                                                               const char* IGL_NULLABLE source,
                                                               const std::string& debugName,
                                                               Result* IGL_NULLABLE
                                                                   outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  const VkShaderStageFlagBits vkStage = shaderStageToVkShaderStage(stage);
  IGL_DEBUG_ASSERT(vkStage != VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);
  IGL_DEBUG_ASSERT(source);

  std::string sourcePatched;

  if (!source || !*source) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "Shader source is empty");
    return nullptr;
  }

  if (strstr(source, "#version ") == nullptr) {
    std::string extraExtensions = ctx_->config_.enableDescriptorIndexing
                                      ? "#extension GL_EXT_nonuniform_qualifier : require\n"
                                      : "";

    // GL_EXT_debug_printf extension
    if (ctx_->features_.has_VK_KHR_shader_non_semantic_info) {
      extraExtensions += "#extension GL_EXT_debug_printf : enable\n";
    }

    const std::string enhancedShaderDebuggingCode =
        EnhancedShaderDebuggingStore::recordLineShaderCode(
            ctx_->enhancedShaderDebuggingStore_ != nullptr, ctx_->features_);

    if (ctx_->features_.featuresShaderFloat16Int8.shaderFloat16 == VK_TRUE) {
      extraExtensions += "#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require\n";
    }

    if (ctx_->features_.has_VK_KHR_buffer_device_address) {
      extraExtensions += "#extension GL_EXT_buffer_reference : require\n";
      extraExtensions += "#extension GL_EXT_buffer_reference_uvec2 : require\n";
    }

    const std::string bindlessTexturesSource = ctx_->config_.enableDescriptorIndexing ?
                                                                                      R"(
      // everything - indexed by global texture/sampler id
      layout (set = 3, binding = 0) uniform texture2D kTextures2D[];
      layout (set = 3, binding = 1) uniform texture2DArray kTextures2DArray[];
      layout (set = 3, binding = 2) uniform texture3D kTextures3D[];
      layout (set = 3, binding = 3) uniform textureCube kTexturesCube[];
      layout (set = 3, binding = 4) uniform sampler kSamplers[];
      layout (set = 3, binding = 5) uniform samplerShadow kSamplersShadow[];
      // binding #6 is reserved for STORAGE_IMAGEs: check VulkanContext.cpp
      )"
                                                                                      : "";

    // there's no header provided in the shader source, let's insert our own header
    if (vkStage == VK_SHADER_STAGE_VERTEX_BIT || vkStage == VK_SHADER_STAGE_COMPUTE_BIT) {
      sourcePatched += R"(
      #version 460
      )" + extraExtensions +
                       enhancedShaderDebuggingCode;
    }
    if (vkStage == VK_SHADER_STAGE_FRAGMENT_BIT) {
      sourcePatched += R"(
      #version 460
      )" + extraExtensions +
                       bindlessTexturesSource + enhancedShaderDebuggingCode;
    }
    sourcePatched += source;
    source = sourcePatched.c_str();
  }

  glslang_resource_t glslangResource = {};
  glslangGetDefaultResource(&glslangResource);
  ivkUpdateGlslangResource(&glslangResource, &ctx_->getVkPhysicalDeviceProperties());

  std::vector<uint32_t> spirv;
  const Result result = glslang::compileShader(stage, source, spirv, &glslangResource);

  VkShaderModule vkShaderModule = VK_NULL_HANDLE;
  const VkShaderModuleCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = spirv.size() * sizeof(uint32_t),
      .pCode = spirv.data(),
  };
  VK_ASSERT(ctx_->vf_.vkCreateShaderModule(ctx_->getVkDevice(), &ci, nullptr, &vkShaderModule));

  Result::setResult(outResult, result);

  if (!result.isOk()) {
    return nullptr;
  }
  IGL_DEBUG_ASSERT(vkShaderModule != VK_NULL_HANDLE);

  if (!debugName.empty()) {
    // set debug name
    VK_ASSERT(ivkSetDebugObjectName(&ctx_->vf_,
                                    ctx_->getVkDevice(),
                                    VK_OBJECT_TYPE_SHADER_MODULE,
                                    (uint64_t)vkShaderModule,
                                    debugName.c_str()));
  }

  return std::make_shared<VulkanShaderModule>(
      ctx_->vf_,
      ctx_->getVkDevice(),
      vkShaderModule,
      util::getReflectionData(spirv.data(), spirv.size() * sizeof(uint32_t)));
}

std::shared_ptr<IFramebuffer> Device::createFramebufferInternal(const FramebufferDesc& desc,
                                                                Result* IGL_NULLABLE outResult) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  auto resource = std::make_shared<Framebuffer>(*this, desc);
  Result::setOk(outResult);

  if (hasResourceTracker()) {
    resource->initResourceTracker(getResourceTracker(), desc.debugName);
  }

  return resource;
}

const PlatformDevice& Device::getPlatformDeviceInternal() const noexcept {
  return platformDevice_;
}

size_t Device::getCurrentDrawCountInternal() const {
  return ctx_->drawCallCount_;
}

size_t Device::getShaderCompilationCountInternal() const {
  return ctx_->shaderCompilationCount_;
}

std::unique_ptr<IShaderLibrary> Device::createShaderLibraryInternal(const ShaderLibraryDesc& desc,
                                                                    Result* IGL_NULLABLE
                                                                        outResult) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  ctx_->shaderCompilationCount_++;

  if (IGL_DEBUG_VERIFY_NOT(desc.moduleInfo.empty())) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid);
    return nullptr;
  }
  Result result;
  std::shared_ptr<VulkanShaderModule> vulkanShaderModule;
  if (desc.input.type == ShaderInputType::Binary) {
    vulkanShaderModule =
        createShaderModule(desc.input.data, desc.input.length, desc.debugName, &result);
  } else {
    if (desc.moduleInfo.size() > 1) {
      IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
      Result::setResult(outResult, Result::Code::Unsupported);
      return nullptr;
    }
    vulkanShaderModule = createShaderModule(
        desc.moduleInfo.front().stage, desc.input.source, desc.debugName, &result);
  }

  if (!result.isOk()) {
    Result::setResult(outResult, std::move(result));
    return nullptr;
  }

  std::vector<std::shared_ptr<IShaderModule>> modules;
  modules.reserve(desc.moduleInfo.size());
  for (const auto& info : desc.moduleInfo) {
    modules.emplace_back(std::make_shared<ShaderModule>(info, vulkanShaderModule));
  }

  Result::setResult(outResult, std::move(result));
  auto shaderLibrary = std::make_unique<ShaderLibrary>(std::move(modules));

  if (hasResourceTracker()) {
    shaderLibrary->initResourceTracker(getResourceTracker(), desc.debugName);
  }

  return shaderLibrary;
}

bool Device::hasFeatureInternal(DeviceFeatures feature) const {
  IGL_PROFILER_FUNCTION();

  VkPhysicalDevice physicalDevice = ctx_->vkPhysicalDevice_;
  IGL_DEBUG_ASSERT(physicalDevice != VK_NULL_HANDLE);
  const VkPhysicalDeviceProperties& deviceProperties = ctx_->getVkPhysicalDeviceProperties();

  switch (feature) {
  case DeviceFeatures::MultiSample:
  case DeviceFeatures::MultiSampleResolve:
    return deviceProperties.limits.framebufferColorSampleCounts > VK_SAMPLE_COUNT_1_BIT;
  case DeviceFeatures::TextureFilterAnisotropic:
    return deviceProperties.limits.maxSamplerAnisotropy > 1;
  case DeviceFeatures::MapBufferRange:
    return true;
  case DeviceFeatures::MeshShaders:
    return false;
  case DeviceFeatures::MultipleRenderTargets:
    return deviceProperties.limits.maxColorAttachments > 1;
  case DeviceFeatures::StandardDerivative:
    return true;
  case DeviceFeatures::StandardDerivativeExt:
    return false;
  case DeviceFeatures::TextureFormatRG:
    return supportsFormat(ctx_->vf_, physicalDevice, VK_FORMAT_R8G8_UNORM);
  case DeviceFeatures::TextureFormatRGB:
    return supportsFormat(ctx_->vf_, physicalDevice, VK_FORMAT_R8G8B8_SRGB);
  case DeviceFeatures::ReadWriteFramebuffer:
    return true;
  case DeviceFeatures::TextureNotPot:
    return true;
  case DeviceFeatures::UniformBlocks:
    return true;
  case DeviceFeatures::TextureHalfFloat:
    return supportsFormat(ctx_->vf_, physicalDevice, VK_FORMAT_R16G16B16A16_SFLOAT) ||
           supportsFormat(ctx_->vf_, physicalDevice, VK_FORMAT_R16_SFLOAT);
  case DeviceFeatures::TextureFloat:
    return supportsFormat(ctx_->vf_, physicalDevice, VK_FORMAT_R32G32B32A32_SFLOAT) ||
           supportsFormat(ctx_->vf_, physicalDevice, VK_FORMAT_R32_SFLOAT);
  case DeviceFeatures::Texture2DArray:
  case DeviceFeatures::Texture3D:
    return true;
  case DeviceFeatures::StorageBuffers:
    return true;
  case DeviceFeatures::ShaderTextureLod:
    return true;
  case DeviceFeatures::ShaderTextureLodExt:
    return false;
  case DeviceFeatures::DepthShaderRead:
    return true;
  case DeviceFeatures::DepthCompare:
    return true;
  case DeviceFeatures::MinMaxBlend:
    return true;
  case DeviceFeatures::TextureExternalImage:
    return false;
  case DeviceFeatures::Compute:
    return true;
  case DeviceFeatures::CopyBuffer:
    return true;
  case DeviceFeatures::ExplicitBinding:
    return true;
  case DeviceFeatures::ExplicitBindingExt:
    return false;
  case DeviceFeatures::ExternalMemoryObjects:
    return true;
  case DeviceFeatures::TextureBindless:
    return ctx_->vkPhysicalDeviceDescriptorIndexingProperties_
               .shaderSampledImageArrayNonUniformIndexingNative == VK_TRUE;
  case DeviceFeatures::PushConstants:
    return true;
  case DeviceFeatures::BufferDeviceAddress:
    return true;
  case DeviceFeatures::Multiview:
    return ctx_->features().featuresMultiview.multiview == VK_TRUE;
  case DeviceFeatures::MultiViewMultisample:
    return ctx_->features().featuresMultiview.multiview == VK_TRUE &&
           deviceProperties.limits.framebufferColorSampleCounts > VK_SAMPLE_COUNT_1_BIT;
  case DeviceFeatures::BindUniform:
    return false;
  case DeviceFeatures::TexturePartialMipChain:
    return true;
  case DeviceFeatures::BufferRing:
    return false;
  case DeviceFeatures::BufferNoCopy:
    return false;
  case DeviceFeatures::ShaderLibrary:
    return true;
  case DeviceFeatures::BindBytes:
    return false;
  case DeviceFeatures::TextureArrayExt:
    return false;
  case DeviceFeatures::SRGB:
    return true;
  case DeviceFeatures::SRGBSwapchain:
    return true;
  // on Metal and Vulkan, the framebuffer pixel format dictates sRGB control.
  case DeviceFeatures::SRGBWriteControl:
    return false;
  case DeviceFeatures::SamplerMinMaxLod:
    return true;
  case DeviceFeatures::DrawFirstIndexFirstVertex:
    return true;
  case DeviceFeatures::DrawIndexedIndirect:
    return true;
  case DeviceFeatures::DrawInstanced:
    return true;
  case DeviceFeatures::Indices8Bit:
    return ctx_->features_.has_VK_EXT_index_type_uint8;
  case DeviceFeatures::ValidationLayersEnabled:
    return ctx_->areValidationLayersEnabled();
  case DeviceFeatures::TextureViews:
    return true;
  case DeviceFeatures::Timers:
    return false;
  }

  IGL_DEBUG_ABORT("DeviceFeatures value not handled: %d", (int)feature);

  return false;
}

bool Device::hasRequirementInternal(DeviceRequirement requirement) const {
  IGL_PROFILER_FUNCTION();

  switch (requirement) {
  case DeviceRequirement::ExplicitBindingExtReq:
  case DeviceRequirement::StandardDerivativeExtReq:
  case DeviceRequirement::TextureArrayExtReq:
  case DeviceRequirement::TextureFormatRGExtReq:
  case DeviceRequirement::ShaderTextureLodExtReq:
    return false;
  };

  assert(false);

  return false;
}

bool Device::getFeatureLimitsInternal(DeviceFeatureLimits featureLimits, size_t& result) const {
  IGL_PROFILER_FUNCTION();

  const VkPhysicalDeviceLimits& limits = ctx_->getVkPhysicalDeviceProperties().limits;

  switch (featureLimits) {
  case DeviceFeatureLimits::MaxTextureDimension1D2D:
    result = std::min(limits.maxImageDimension1D, limits.maxImageDimension2D);
    return true;
  case DeviceFeatureLimits::MaxCubeMapDimension:
    result = limits.maxImageDimensionCube;
    return true;
  case DeviceFeatureLimits::MaxStorageBufferBytes:
    result = limits.maxStorageBufferRange;
    return true;
  case DeviceFeatureLimits::MaxVertexUniformVectors:
  case DeviceFeatureLimits::MaxFragmentUniformVectors:
  case DeviceFeatureLimits::MaxUniformBufferBytes:
    result = limits.maxUniformBufferRange;
    return true;
  case DeviceFeatureLimits::MaxPushConstantBytes:
    result = limits.maxPushConstantsSize;
    return true;
  case DeviceFeatureLimits::MaxMultisampleCount: {
    const VkSampleCountFlags sampleCounts = limits.framebufferColorSampleCounts;
    if ((sampleCounts & VK_SAMPLE_COUNT_64_BIT) != 0) {
      result = 64;
    } else if ((sampleCounts & VK_SAMPLE_COUNT_32_BIT) != 0) {
      result = 32;
    } else if ((sampleCounts & VK_SAMPLE_COUNT_16_BIT) != 0) {
      result = 16;
    } else if ((sampleCounts & VK_SAMPLE_COUNT_8_BIT) != 0) {
      result = 8;
    } else if ((sampleCounts & VK_SAMPLE_COUNT_4_BIT) != 0) {
      result = 4;
    } else if ((sampleCounts & VK_SAMPLE_COUNT_2_BIT) != 0) {
      result = 2;
    } else {
      result = 1;
    }
    return true;
  }
  case DeviceFeatureLimits::PushConstantsAlignment:
    result = 4;
    return true;
  case DeviceFeatureLimits::ShaderStorageBufferOffsetAlignment:
    result = limits.minStorageBufferOffsetAlignment;
    return true;
  case DeviceFeatureLimits::BufferAlignment:
    result = limits.minUniformBufferOffsetAlignment;
    return true;
  case DeviceFeatureLimits::BufferNoCopyAlignment:
    result = 0;
    return true;
  case DeviceFeatureLimits::MaxBindBytesBytes:
    result = 0;
    return true;
  }

  IGL_DEBUG_ABORT("DeviceFeatureLimits value not handled: %d", (int)featureLimits);
  result = 0;
  return false;
}

ICapabilities::TextureFormatCapabilities Device::getTextureFormatCapabilitiesInternal(
    TextureFormat format) const {
  IGL_PROFILER_FUNCTION();

  const VkFormat vkFormat = igl::vulkan::textureFormatToVkFormat(format);

  if (vkFormat == VK_FORMAT_UNDEFINED) {
    return TextureFormatCapabilityBits::Unsupported;
  }

  if (vkFormat == VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG ||
      vkFormat == VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG ||
      vkFormat == VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG ||
      vkFormat == VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG ||
      vkFormat == VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG ||
      vkFormat == VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG ||
      vkFormat == VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG ||
      vkFormat == VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG) {
    // Deprecated without replacement
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_IMG_format_pvrtc.html
    return TextureFormatCapabilityBits::Unsupported;
  }

  VkFormatProperties properties;
  ctx_->vf_.vkGetPhysicalDeviceFormatProperties(ctx_->vkPhysicalDevice_, vkFormat, &properties);

  const VkFormatFeatureFlags features = properties.optimalTilingFeatures;

  TextureFormatCapabilities caps = TextureFormatCapabilityBits::Unsupported;

  if (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
    caps |= TextureFormatCapabilityBits::Sampled;
  }
  if (features & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) {
    caps |= TextureFormatCapabilityBits::Storage;
  }
  if (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) {
    caps |= TextureFormatCapabilityBits::SampledFiltered;
  }
  if (features & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) {
    caps |= TextureFormatCapabilityBits::Attachment;
  }
  if (features & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    caps |= TextureFormatCapabilityBits::Attachment;
  }

  // Special handling for when a format can be sampled AND used as an attachment
  if (contains(caps, TextureFormatCapabilityBits::Sampled) &&
      contains(caps, TextureFormatCapabilityBits::Attachment)) {
    caps |= TextureFormatCapabilityBits::SampledAttachment;
  }

  return caps;
}

ShaderVersion Device::getShaderVersionInternal() const {
  return {ShaderFamily::SpirV, 1, 5, 0};
}

BackendVersion Device::getBackendVersionInternal() const {
  const uint32_t apiVersion = ctx_->vkPhysicalDeviceProperties2_.properties.apiVersion;
  return {BackendFlavor::Vulkan,
          static_cast<uint8_t>(VK_API_VERSION_MAJOR(apiVersion)),
          static_cast<uint8_t>(VK_API_VERSION_MINOR(apiVersion))};
}

Holder<BindGroupTextureHandle> Device::createBindGroupInternal(
    const igl::BindGroupTextureDesc& desc,
    const IRenderPipelineState* IGL_NULLABLE compatiblePipeline,
    Result* IGL_NULLABLE outResult) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  IGL_DEBUG_ASSERT(ctx_);
  IGL_DEBUG_ASSERT(!desc.debugName.empty(), "Each bind group should have a debug name");
  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  return {this, ctx_->createBindGroup(desc, compatiblePipeline, outResult)};
}

Holder<BindGroupBufferHandle> Device::createBindGroupInternal(const igl::BindGroupBufferDesc& desc,
                                                              Result* IGL_NULLABLE outResult) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  IGL_DEBUG_ASSERT(ctx_);
  IGL_DEBUG_ASSERT(!desc.debugName.empty(), "Each bind group should have a debug name");
  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  return {this, ctx_->createBindGroup(desc, outResult)};
}

std::shared_ptr<ITimer> Device::createTimer(Result* IGL_NULLABLE outResult) const noexcept {
  if (outResult) {
    *outResult = Result(Result::Code::Unsupported, "Timer is not supported on Vulkan");
  }
  return nullptr;
}

void Device::destroyInternal(BindGroupTextureHandle handle) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);
  IGL_DEBUG_ASSERT(ctx_);
  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  ctx_->destroy(handle);
}

void Device::destroyInternal(BindGroupBufferHandle handle) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);
  IGL_DEBUG_ASSERT(ctx_);
  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  ctx_->destroy(handle);
}

void Device::destroyInternal(SamplerHandle handle) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);
  IGL_DEBUG_ASSERT(ctx_);
  IGL_ENSURE_VULKAN_CONTEXT_THREAD(ctx_);

  ctx_->destroy(handle);
}

void Device::setCurrentThreadInternal() {
  IGL_PROFILER_FUNCTION();
  IGL_DEBUG_ASSERT(ctx_);

  ctx_->setCurrentContextThread();
}

} // namespace igl::vulkan
