/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Device.h>

#include <cstring>
#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/ComputePipelineState.h>
#include <igl/vulkan/RenderPipelineState.h>
#include <igl/vulkan/SamplerState.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanShaderModule.h>
#include <igl/vulkan/VulkanSwapchain.h>

namespace {

bool supportsFormat(VkPhysicalDevice physicalDevice, VkFormat format) {
  VkFormatProperties properties;
  vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
  return properties.bufferFeatures != 0 || properties.linearTilingFeatures != 0 ||
         properties.optimalTilingFeatures != 0;
} // namespace

VkShaderStageFlagBits shaderStageToVkShaderStage(lvk::ShaderStage stage) {
  switch (stage) {
  case lvk::Stage_Vertex:
    return VK_SHADER_STAGE_VERTEX_BIT;
  case lvk::Stage_Fragment:
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  case lvk::Stage_Compute:
    return VK_SHADER_STAGE_COMPUTE_BIT;
  case lvk::kNumShaderStages:
    return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
  };
  return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
}

} // namespace

namespace lvk::vulkan {

Device::Device(std::unique_ptr<VulkanContext> ctx) : ctx_(std::move(ctx)) {}

std::shared_ptr<ICommandBuffer> Device::createCommandBuffer() {
  IGL_PROFILER_FUNCTION();

  return std::make_shared<CommandBuffer>(getVulkanContext());
}

void Device::submit(const lvk::ICommandBuffer& commandBuffer,
                    lvk::CommandQueueType queueType,
                    ITexture* present) {
  IGL_PROFILER_FUNCTION();

  const VulkanContext& ctx = getVulkanContext();

  vulkan::CommandBuffer* vkCmdBuffer =
      const_cast<vulkan::CommandBuffer*>(static_cast<const vulkan::CommandBuffer*>(&commandBuffer));

  const bool isGraphicsQueue = queueType == CommandQueueType::Graphics;

  if (present) {
    const auto& vkTex = static_cast<Texture&>(*present);
    const VulkanTexture& tex = vkTex.getVulkanTexture();
    const VulkanImage& img = tex.getVulkanImage();

    IGL_ASSERT(vkTex.isSwapchainTexture());

    // prepare image for presentation the image might be coming from a compute shader
    const VkPipelineStageFlagBits srcStage = (img.imageLayout_ == VK_IMAGE_LAYOUT_GENERAL)
                                                 ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                                 : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    img.transitionLayout(
        vkCmdBuffer->wrapper_.cmdBuf_,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        srcStage,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // wait for all subsequent operations
        VkImageSubresourceRange{
            VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }

  // Submit to the graphics queue.
  const bool shouldPresent = isGraphicsQueue && ctx.hasSwapchain() && present;
  if (shouldPresent) {
    ctx.immediate_->waitSemaphore(ctx.swapchain_->acquireSemaphore_);
  }

  vkCmdBuffer->lastSubmitHandle_ = ctx.immediate_->submit(vkCmdBuffer->wrapper_);

  if (shouldPresent) {
    ctx.present();
  }

  ctx.processDeferredTasks();
}

std::unique_ptr<IBuffer> Device::createBuffer(const BufferDesc& desc, Result* outResult) {
  auto buffer = std::make_unique<vulkan::Buffer>(*this);

  const auto result = buffer->create(desc);

  if (!IGL_VERIFY(result.isOk())) {
    return nullptr;
  }

  if (!desc.data) {
    return buffer;
  }

  const auto uploadResult = buffer->upload(desc.data, desc.size);
  IGL_VERIFY(uploadResult.isOk());
  Result::setResult(outResult, uploadResult);

  return buffer;
}

std::shared_ptr<ISamplerState> Device::createSamplerState(const SamplerStateDesc& desc,
                                                          Result* outResult) {
  auto samplerState = std::make_shared<vulkan::SamplerState>(*this);

  Result::setResult(outResult, samplerState->create(desc));

  return samplerState;
}

std::shared_ptr<ITexture> Device::createTexture(const TextureDesc& desc,
                                                const char* debugName,
                                                Result* outResult) {
  TextureDesc patchedDesc(desc);

  if (debugName && *debugName) {
    patchedDesc.debugName = debugName;
  }

  auto texture = std::make_shared<vulkan::Texture>(*ctx_.get(), patchedDesc.format);

  Result res = texture->create(patchedDesc);

  if (!res.isOk()) {
    return nullptr;
  }

  if (patchedDesc.initialData) {
    IGL_ASSERT(patchedDesc.type == TextureType::TwoD);
    const void* mipMaps[] = {patchedDesc.initialData};
    res = texture->upload(
        {
            .width = desc.width,
            .height = desc.height,
            .numMipLevels = 1,
        },
        mipMaps);
  }

  Result::setResult(outResult, res);

  return res.isOk() ? texture : nullptr;
}

lvk::Holder<lvk::ComputePipelineHandle> Device::createComputePipeline(
    const ComputePipelineDesc& desc,
    Result* outResult) {
  if (!IGL_VERIFY(desc.shaderStages.getModule(Stage_Compute).valid())) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Missing compute shader");
    return {};
  }

  return {this, ctx_->computePipelinesPool_.create(ComputePipelineState(this, desc))};
}

lvk::Holder<lvk::RenderPipelineHandle> Device::createRenderPipeline(const RenderPipelineDesc& desc,
                                                                    Result* outResult) {
  const bool hasColorAttachments = desc.getNumColorAttachments() > 0;
  const bool hasDepthAttachment = desc.depthAttachmentFormat != TextureFormat::Invalid;
  const bool hasAnyAttachments = hasColorAttachments || hasDepthAttachment;
  if (!IGL_VERIFY(hasAnyAttachments)) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Need at least one attachment");
    return {};
  }

  if (!IGL_VERIFY(desc.shaderStages.getModule(Stage_Vertex).valid())) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Missing vertex shader");
    return {};
  }

  if (!IGL_VERIFY(desc.shaderStages.getModule(Stage_Fragment).valid())) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Missing fragment shader");
    return {};
  }

  return {this, ctx_->renderPipelinesPool_.create(RenderPipelineState(this, desc))};
}

void Device::destroy(lvk::ComputePipelineHandle handle) {
  ctx_->computePipelinesPool_.destroy(handle);
}

void Device::destroy(lvk::RenderPipelineHandle handle){
  ctx_->renderPipelinesPool_.destroy(handle);
}

void Device::destroy(lvk::ShaderModuleHandle handle) {
  ctx_->shaderModulesPool_.destroy(handle);
}

lvk::Holder<lvk::ShaderModuleHandle> Device::createShaderModule(const ShaderModuleDesc& desc, Result* outResult) {
  Result result;
  VulkanShaderModule vulkanShaderModule =
      desc.dataSize ? std::move(
                          // binary
                          createShaderModule(
                              desc.data, desc.dataSize, desc.entryPoint, desc.debugName, &result))
                    : std::move(
                          // text
                          createShaderModule(
                              desc.stage, desc.data, desc.entryPoint, desc.debugName, &result));

  if (!result.isOk()) {
    Result::setResult(outResult, std::move(result));
    return {};
  }
  Result::setResult(outResult, std::move(result));

  return {this, ctx_->shaderModulesPool_.create(std::move(vulkanShaderModule))};
}

VulkanShaderModule Device::createShaderModule(const void* data,
                                              size_t length,
                                              const char* entryPoint,
                                              const char* debugName,
                                              Result* outResult) const {
  VkShaderModule vkShaderModule = VK_NULL_HANDLE;

  const VkShaderModuleCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = length,
      .pCode = (const uint32_t*)data,
  };
  const VkResult result = vkCreateShaderModule(ctx_->vkDevice_, &ci, nullptr, &vkShaderModule);

  setResultFrom(outResult, result);

  if (result != VK_SUCCESS) {
    return VulkanShaderModule();
  }

  VK_ASSERT(ivkSetDebugObjectName(
      ctx_->vkDevice_, VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)vkShaderModule, debugName));

  return VulkanShaderModule(ctx_->vkDevice_, vkShaderModule, entryPoint);
}

VulkanShaderModule Device::createShaderModule(ShaderStage stage,
                                              const char* source,
                                              const char* entryPoint,
                                              const char* debugName,
                                              Result* outResult) const {
  const VkShaderStageFlagBits vkStage = shaderStageToVkShaderStage(stage);
  IGL_ASSERT(vkStage != VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);
  IGL_ASSERT(source);

  std::string sourcePatched;

  if (!source || !*source) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Shader source is empty");
    return VulkanShaderModule();
  }

  if (strstr(source, "#version ") == nullptr) {
    // there's no header provided in the shader source, let's insert our own header
    if (vkStage == VK_SHADER_STAGE_VERTEX_BIT || vkStage == VK_SHADER_STAGE_COMPUTE_BIT) {
      sourcePatched += R"(
      #version 460
      #extension GL_EXT_debug_printf : enable
      #extension GL_EXT_nonuniform_qualifier : require
      #extension GL_EXT_buffer_reference : require
      #extension GL_EXT_buffer_reference_uvec2 : require
      #extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
      )";
    }
    if (vkStage == VK_SHADER_STAGE_FRAGMENT_BIT) {
      sourcePatched += R"(
      #version 460
      #extension GL_EXT_debug_printf : enable
      #extension GL_EXT_nonuniform_qualifier : require
      #extension GL_EXT_buffer_reference_uvec2 : require
      #extension GL_EXT_shader_explicit_arithmetic_types_float16 : require

      layout (set = 0, binding = 0) uniform texture2D kTextures2D[];
      layout (set = 0, binding = 0) uniform texture3D kTextures3D[];
      layout (set = 0, binding = 0) uniform textureCube kTexturesCube[];
      layout (set = 0, binding = 1) uniform sampler kSamplers[];
      layout (set = 0, binding = 1) uniform samplerShadow kSamplersShadow[];

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

  const glslang_resource_t glslangResource =
      lvk::getGlslangResource(ctx_->getVkPhysicalDeviceProperties().limits);

  VkShaderModule vkShaderModule = VK_NULL_HANDLE;
  const Result result = lvk::vulkan::compileShader(
      ctx_->vkDevice_, vkStage, source, &vkShaderModule, &glslangResource);

  Result::setResult(outResult, result);

  if (!result.isOk()) {
    return VulkanShaderModule();
  }

  VK_ASSERT(ivkSetDebugObjectName(
      ctx_->vkDevice_, VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)vkShaderModule, debugName));

  return VulkanShaderModule(ctx_->vkDevice_, vkShaderModule, entryPoint);
}

std::shared_ptr<ITexture> Device::getCurrentSwapchainTexture() {
  IGL_PROFILER_FUNCTION();

  if (!ctx_->hasSwapchain()) {
    return nullptr;
  };

  std::shared_ptr<Texture> tex = ctx_->swapchain_->getCurrentTexture();

  if (!IGL_VERIFY(tex != nullptr)) {
    IGL_ASSERT_MSG(false, "Swapchain has no valid texture");
    return nullptr;
  }

  IGL_ASSERT_MSG(tex->getVulkanTexture().getVulkanImage().imageFormat_ != VK_FORMAT_UNDEFINED,
                 "Invalid image format");

  return tex;
}

std::unique_ptr<VulkanContext> Device::createContext(const VulkanContextConfig& config,
                                                     void* window,
                                                     void* display) {
  return std::make_unique<VulkanContext>(config, window, display);
}

std::vector<HWDeviceDesc> Device::queryDevices(VulkanContext& ctx,
                                               HWDeviceType deviceType,
                                               Result* outResult) {
  std::vector<HWDeviceDesc> outDevices;

  Result::setResult(outResult, ctx.queryDevices(deviceType, outDevices));

  return outDevices;
}

std::unique_ptr<IDevice> Device::create(std::unique_ptr<VulkanContext> ctx,
                                        const HWDeviceDesc& desc,
                                        uint32_t width,
                                        uint32_t height,
                                        Result* outResult) {
  IGL_ASSERT(ctx.get());

  auto result = ctx->initContext(desc);

  Result::setResult(outResult, result);

  if (!result.isOk()) {
    return nullptr;
  }

  if (width > 0 && height > 0) {
    result = ctx->initSwapchain(width, height);

    Result::setResult(outResult, result);
  }

  return result.isOk() ? std::make_unique<lvk::vulkan::Device>(std::move(ctx)) : nullptr;
}

} // namespace lvk::vulkan
