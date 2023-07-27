/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Device.h>

#include <cstring>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/ComputePipelineState.h>
#include <igl/vulkan/RenderPipelineState.h>
#include <igl/vulkan/VulkanBuffer.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanShaderModule.h>
#include <igl/vulkan/VulkanSwapchain.h>
#include <igl/vulkan/VulkanTexture.h>

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
  case lvk::Stage_Geometry:
    return VK_SHADER_STAGE_GEOMETRY_BIT;
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

ICommandBuffer& Device::acquireCommandBuffer() {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT_MSG(!currentCommandBuffer_.ctx_,
                 "Cannot acquire more than 1 command buffer simultaneously");

  currentCommandBuffer_ = CommandBuffer(ctx_.get());

  return currentCommandBuffer_;
}

void Device::submit(const lvk::ICommandBuffer& commandBuffer,
                    lvk::QueueType queueType,
                    TextureHandle present) {
  IGL_PROFILER_FUNCTION();

  const VulkanContext& ctx = getVulkanContext();

  vulkan::CommandBuffer* vkCmdBuffer =
      const_cast<vulkan::CommandBuffer*>(static_cast<const vulkan::CommandBuffer*>(&commandBuffer));

  IGL_ASSERT(vkCmdBuffer);
  IGL_ASSERT(vkCmdBuffer->ctx_);
  IGL_ASSERT(vkCmdBuffer->wrapper_);

  const bool isGraphicsQueue = queueType == QueueType_Graphics;

  if (present) {
    const lvk::vulkan::VulkanTexture& tex = *ctx.texturesPool_.get(present)->get();

    IGL_ASSERT(tex.isSwapchainTexture());

    // prepare image for presentation the image might be coming from a compute shader
    const VkPipelineStageFlagBits srcStage = (tex.image_->imageLayout_ == VK_IMAGE_LAYOUT_GENERAL)
                                                 ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                                 : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    tex.image_->transitionLayout(
        vkCmdBuffer->wrapper_->cmdBuf_,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        srcStage,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // wait for all subsequent operations
        VkImageSubresourceRange{
            VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }

  const bool shouldPresent = isGraphicsQueue && ctx.hasSwapchain() && present;
  if (shouldPresent) {
    ctx.immediate_->waitSemaphore(ctx.swapchain_->acquireSemaphore_);
  }

  vkCmdBuffer->lastSubmitHandle_ = ctx.immediate_->submit(*vkCmdBuffer->wrapper_);

  if (shouldPresent) {
    ctx.present();
  }

  ctx.processDeferredTasks();

  // reset
  currentCommandBuffer_ = {};
}

Holder<BufferHandle> Device::createBuffer(const BufferDesc& requestedDesc, Result* outResult) {
  BufferDesc desc = requestedDesc;

  if (!ctx_->useStaging_ && (desc.storage == StorageType_Device)) {
    desc.storage = StorageType_HostVisible;
  }

  // Use staging device to transfer data into the buffer when the storage is private to the device
  VkBufferUsageFlags usageFlags =
      (desc.storage == StorageType_Device)
          ? VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
          : 0;

  if (desc.usage == 0) {
    Result::setResult(outResult, Result(Result::Code::ArgumentOutOfRange, "Invalid buffer usage"));
    return {};
  }

  if (desc.usage & BufferUsageBits_Index) {
    usageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }
  if (desc.usage & BufferUsageBits_Vertex) {
    usageFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }
  if (desc.usage & BufferUsageBits_Uniform) {
    usageFlags |=
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
  }

  if (desc.usage & BufferUsageBits_Storage) {
    usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
  }

  if (desc.usage & BufferUsageBits_Indirect) {
    usageFlags |=
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
  }

  const VkMemoryPropertyFlags memFlags = storageTypeToVkMemoryPropertyFlags(desc.storage);

  Result result;
  BufferHandle handle =
      ctx_->createBuffer(desc.size, usageFlags, memFlags, &result, desc.debugName);

  if (!IGL_VERIFY(result.isOk())) {
    Result::setResult(outResult, result);
    return {};
  }

  if (desc.data) {
    upload(handle, desc.data, desc.size, 0);
  }

  Result::setResult(outResult, Result());

  return {this, handle};
}

Holder<SamplerHandle> Device::createSampler(const SamplerStateDesc& desc, Result* outResult) {
  IGL_PROFILER_FUNCTION();

  Result result;

  SamplerHandle handle = ctx_->createSampler(
      samplerStateDescToVkSamplerCreateInfo(desc, ctx_->getVkPhysicalDeviceProperties().limits),
      &result,
      desc.debugName);

  if (!IGL_VERIFY(result.isOk())) {
    Result::setResult(outResult, Result(Result::Code::RuntimeError, "Cannot create Sampler"));
    return {};
  }

  Holder<SamplerHandle> holder = {this, handle};

  Result::setResult(outResult, result);

  return holder;
}

Holder<TextureHandle> Device::createTexture(const TextureDesc& requestedDesc,
                                            const char* debugName,
                                            Result* outResult) {
  TextureDesc desc(requestedDesc);

  if (debugName && *debugName) {
    desc.debugName = debugName;
  }

  const VkFormat vkFormat = lvk::isDepthOrStencilFormat(desc.format)
                                ? ctx_->getClosestDepthStencilFormat(desc.format)
                                : textureFormatToVkFormat(desc.format);

  const lvk::TextureType type = desc.type;
  if (!IGL_VERIFY(type == TextureType_2D || type == TextureType_Cube || type == TextureType_3D)) {
    IGL_ASSERT_MSG(false, "Only 2D, 3D and Cube textures are supported");
    Result::setResult(outResult, Result::Code::RuntimeError);
    return {};
  }

  if (desc.numMipLevels == 0) {
    IGL_ASSERT_MSG(false, "The number of mip levels specified must be greater than 0");
    desc.numMipLevels = 1;
  }

  if (desc.numSamples > 1 && desc.numMipLevels != 1) {
    IGL_ASSERT_MSG(false, "The number of mip levels for multisampled images should be 1");
    Result::setResult(outResult,
                      Result::Code::ArgumentOutOfRange,
                      "The number of mip-levels for multisampled images should be 1");
    return {};
  }

  if (desc.numSamples > 1 && type == TextureType_3D) {
    IGL_ASSERT_MSG(false, "Multisampled 3D images are not supported");
    Result::setResult(
        outResult, Result::Code::ArgumentOutOfRange, "Multisampled 3D images are not supported");
    return {};
  }

  if (!IGL_VERIFY(desc.numMipLevels <= lvk::calcNumMipLevels(desc.width, desc.height))) {
    Result::setResult(outResult,
                      Result::Code::ArgumentOutOfRange,
                      "The number of specified mip-levels is greater than the maximum possible "
                      "number of mip-levels.");
    return {};
  }

  if (desc.usage == 0) {
    IGL_ASSERT_MSG(false, "Texture usage flags are not set");
    desc.usage = lvk::TextureUsageBits_Sampled;
  }

  /* Use staging device to transfer data into the image when the storage is private to the device */
  VkImageUsageFlags usageFlags =
      (desc.storage == StorageType_Device) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0;

  if (desc.usage & lvk::TextureUsageBits_Sampled) {
    usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if (desc.usage & lvk::TextureUsageBits_Storage) {
    IGL_ASSERT_MSG(desc.numSamples <= 1, "Storage images cannot be multisampled");
    usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  if (desc.usage & lvk::TextureUsageBits_Attachment) {
    usageFlags |= lvk::isDepthOrStencilFormat(desc.format)
                      ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                      : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }

  // For now, always set this flag so we can read it back
  usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  IGL_ASSERT_MSG(usageFlags != 0, "Invalid usage flags");

  const VkMemoryPropertyFlags memFlags = storageTypeToVkMemoryPropertyFlags(desc.storage);

  const bool hasDebugName = desc.debugName && *desc.debugName;

  char debugNameImage[256] = {0};
  char debugNameImageView[256] = {0};

  if (hasDebugName) {
    snprintf(debugNameImage, sizeof(debugNameImage) - 1, "Image: %s", desc.debugName);
    snprintf(debugNameImageView, sizeof(debugNameImageView) - 1, "Image View: %s", desc.debugName);
  }

  VkImageCreateFlags createFlags = 0;
  uint32_t arrayLayerCount = static_cast<uint32_t>(desc.numLayers);
  VkImageViewType imageViewType;
  VkImageType imageType;
  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
  switch (desc.type) {
  case TextureType_2D:
    imageViewType = VK_IMAGE_VIEW_TYPE_2D;
    imageType = VK_IMAGE_TYPE_2D;
    samples = getVulkanSampleCountFlags(desc.numSamples);
    break;
  case TextureType_3D:
    imageViewType = VK_IMAGE_VIEW_TYPE_3D;
    imageType = VK_IMAGE_TYPE_3D;
    break;
  case TextureType_Cube:
    imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
    arrayLayerCount *= 6;
    imageType = VK_IMAGE_TYPE_2D;
    createFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    break;
  default:
    IGL_ASSERT_MSG(false, "Code should NOT be reached");
    Result::setResult(outResult, Result::Code::RuntimeError, "Unsupported texture type");
    return {};
  }

  Result result;
  auto image = ctx_->createImage(
      imageType,
      VkExtent3D{(uint32_t)desc.width, (uint32_t)desc.height, (uint32_t)desc.depth},
      vkFormat,
      (uint32_t)desc.numMipLevels,
      arrayLayerCount,
      VK_IMAGE_TILING_OPTIMAL,
      usageFlags,
      memFlags,
      createFlags,
      samples,
      &result,
      debugNameImage);
  if (!IGL_VERIFY(result.isOk())) {
    Result::setResult(outResult, result);
    return {};
  }
  if (!IGL_VERIFY(image.get())) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Cannot create VulkanImage");
    return {};
  }

  // TODO: use multiple image views to allow sampling from the STENCIL buffer
  const VkImageAspectFlags aspect = (usageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                                        ? VK_IMAGE_ASPECT_DEPTH_BIT
                                        : VK_IMAGE_ASPECT_COLOR_BIT;

  std::shared_ptr<VulkanImageView> imageView = image->createImageView(imageViewType,
                                                                      vkFormat,
                                                                      aspect,
                                                                      0,
                                                                      VK_REMAINING_MIP_LEVELS,
                                                                      0,
                                                                      arrayLayerCount,
                                                                      debugNameImageView);

  if (!IGL_VERIFY(imageView.get())) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Cannot create VulkanImageView");
    return {};
  }

  std::shared_ptr<vulkan::VulkanTexture> texture =
      std::make_shared<VulkanTexture>(std::move(image), std::move(imageView));

  TextureHandle handle = ctx_->texturesPool_.create(std::move(texture));

  IGL_ASSERT(ctx_->texturesPool_.numObjects() <= ctx_->config_.maxTextures);

  ctx_->awaitingCreation_ = true;

  if (desc.initialData) {
    IGL_ASSERT(desc.type == TextureType_2D);
    const void* mipMaps[] = {desc.initialData};
    Result res = upload(handle,
                        {
                            .width = desc.width,
                            .height = desc.height,
                            .numMipLevels = 1,
                        },
                        mipMaps);
    if (!res.isOk()) {
      Result::setResult(outResult, res);
      return {};
    }
  }

  Result::setResult(outResult, Result());

  return {this, handle};
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

void Device::destroy(SamplerHandle handle) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  VkSampler sampler = *ctx_->samplersPool_.get(handle);

  ctx_->samplersPool_.destroy(handle);

  ctx_->deferredTask(std::packaged_task<void()>([device = ctx_->vkDevice_, sampler = sampler]() {
    vkDestroySampler(device, sampler, nullptr);
  }));

  // inform the context it should prune the samplers
  ctx_->awaitingDeletion_ = true;
}

void Device::destroy(BufferHandle handle) {
  ctx_->buffersPool_.destroy(handle);
}

void Device::destroy(lvk::TextureHandle handle) {
  ctx_->texturesPool_.destroy(handle);

  // inform the context it should prune the textures
  ctx_->awaitingDeletion_ = true;
}


void Device::destroy(Framebuffer& fb) {
  auto destroyFbTexture = [this](TextureHandle& handle) {
    {
      if (handle.empty())
        return;
      lvk::vulkan::VulkanTexture* tex = ctx_->texturesPool_.get(handle)->get();
      if (!tex || tex->isSwapchainTexture())
        return;
      destroy(handle);
      handle = {};
    }
  };

  for (Framebuffer::AttachmentDesc& a : fb.colorAttachments) {
    destroyFbTexture(a.texture);
    destroyFbTexture(a.resolveTexture);
  }
  destroyFbTexture(fb.depthStencilAttachment.texture);
  destroyFbTexture(fb.depthStencilAttachment.resolveTexture);
}

Result Device::upload(BufferHandle handle, const void* data, size_t size, size_t offset) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_VERIFY(data)) {
    return lvk::Result();
  }

  lvk::vulkan::VulkanBuffer* buf = ctx_->buffersPool_.get(handle);

  if (!IGL_VERIFY(buf)) {
    return lvk::Result();
  }

  if (!IGL_VERIFY(offset + size <= buf->getSize())) {
    return lvk::Result(Result::Code::ArgumentOutOfRange, "Out of range");
  }

  ctx_->stagingDevice_->bufferSubData(*buf, offset, size, data);

  return lvk::Result();
}

uint8_t* Device::getMappedPtr(BufferHandle handle) const {
  lvk::vulkan::VulkanBuffer* buf = ctx_->buffersPool_.get(handle);

  IGL_ASSERT(buf);

  return buf->isMapped() ? buf->getMappedPtr() : nullptr;
}

uint64_t Device::gpuAddress(BufferHandle handle, size_t offset) const {
  IGL_ASSERT_MSG((offset & 7) == 0,
                 "Buffer offset must be 8 bytes aligned as per GLSL_EXT_buffer_reference spec.");

  lvk::vulkan::VulkanBuffer* buf = ctx_->buffersPool_.get(handle);

  IGL_ASSERT(buf);

  return buf ? (uint64_t)buf->getVkDeviceAddress() + offset : 0u;
}

static Result validateRange(const lvk::Dimensions& dimensions,
                            uint32_t numLevels,
                            const lvk::TextureRangeDesc& range) {
  if (!IGL_VERIFY(range.width > 0 && range.height > 0 || range.depth > 0 || range.numLayers > 0 ||
                  range.numMipLevels > 0)) {
    return Result{Result::Code::ArgumentOutOfRange,
                  "width, height, depth numLayers, and numMipLevels must be > 0"};
  }
  if (range.mipLevel > numLevels) {
    return Result{Result::Code::ArgumentOutOfRange, "range.mipLevel exceeds texture mip-levels"};
  }

  const auto texWidth = std::max(dimensions.width >> range.mipLevel, 1u);
  const auto texHeight = std::max(dimensions.height >> range.mipLevel, 1u);
  const auto texDepth = std::max(dimensions.depth >> range.mipLevel, 1u);

  if (range.width > texWidth || range.height > texHeight || range.depth > texDepth) {
    return Result{Result::Code::ArgumentOutOfRange, "range dimensions exceed texture dimensions"};
  }
  if (range.x > texWidth - range.width || range.y > texHeight - range.height ||
      range.z > texDepth - range.depth) {
    return Result{Result::Code::ArgumentOutOfRange, "range dimensions exceed texture dimensions"};
  }

  return Result{};
}

Result Device::upload(TextureHandle handle,
                      const TextureRangeDesc& range,
                      const void* data[]) const {
  if (!data) {
    return lvk::Result();
  }

  lvk::vulkan::VulkanTexture* texture = ctx_->texturesPool_.get(handle)->get();

  const auto result = validateRange(texture->getDimensions(), texture->image_->levels_, range);

  if (!IGL_VERIFY(result.isOk())) {
    return result;
  }

  const uint32_t numLayers = std::max(range.numLayers, 1u);

  const VkImageType type = texture->image_->type_;
  VkFormat vkFormat = texture->image_->imageFormat_;

  if (type == VK_IMAGE_TYPE_3D) {
    const void* uploadData = data[0];
    ctx_->stagingDevice_->imageData3D(
        *texture->image_.get(),
        VkOffset3D{(int32_t)range.x, (int32_t)range.y, (int32_t)range.z},
        VkExtent3D{(uint32_t)range.width, (uint32_t)range.height, (uint32_t)range.depth},
        vkFormat,
        uploadData);
  } else {
    const VkRect2D imageRegion = ivkGetRect2D(
        (uint32_t)range.x, (uint32_t)range.y, (uint32_t)range.width, (uint32_t)range.height);
    ctx_->stagingDevice_->imageData2D(*texture->image_.get(),
                                      imageRegion,
                                      range.mipLevel,
                                      range.numMipLevels,
                                      range.layer,
                                      range.numLayers,
                                      vkFormat,
                                      data);
  }

  return Result();
}

Dimensions Device::getDimensions(TextureHandle handle) const {
  if (!handle) {
    return {};
  }

  return (*ctx_->texturesPool_.get(handle))->getDimensions();
}

void Device::generateMipmap(TextureHandle handle) const {
  if (handle.empty()) {
    return;
  }

  lvk::vulkan::VulkanTexture* tex = ctx_->texturesPool_.get(handle)->get();

  if (tex->image_->levels_ > 1) {
    IGL_ASSERT(tex->image_->imageLayout_ != VK_IMAGE_LAYOUT_UNDEFINED);
    const auto& wrapper = ctx_->immediate_->acquire();
    tex->image_->generateMipmap(wrapper.cmdBuf_);
    ctx_->immediate_->submit(wrapper);
  }
}

TextureFormat Device::getFormat(TextureHandle handle) const {
  if (handle.empty()) {
    return TextureFormat::Invalid;
  }

  return vkFormatToTextureFormat((*ctx_->texturesPool_.get(handle))->image_->imageFormat_);
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
      #extension GL_EXT_buffer_reference : require
      #extension GL_EXT_buffer_reference_uvec2 : require
      #extension GL_EXT_debug_printf : enable
      #extension GL_EXT_nonuniform_qualifier : require
      #extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
      )";
    }
    if (vkStage == VK_SHADER_STAGE_FRAGMENT_BIT) {
      sourcePatched += R"(
      #version 460
      #extension GL_EXT_buffer_reference_uvec2 : require
      #extension GL_EXT_debug_printf : enable
      #extension GL_EXT_nonuniform_qualifier : require
      #extension GL_EXT_samplerless_texture_functions : require
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
      ivec2 textureBindlessSize2D(uint textureid) {
        return textureSize(kTextures2D[textureid], 0);
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

TextureFormat Device::getSwapchainFormat() const {
  if (!ctx_->hasSwapchain()) {
    return TextureFormat::Invalid;
  }

  return getFormat(ctx_->swapchain_->getCurrentTexture());
}

TextureHandle Device::getCurrentSwapchainTexture() {
  IGL_PROFILER_FUNCTION();

  if (!ctx_->hasSwapchain()) {
    return {};
  }

  TextureHandle tex = ctx_->swapchain_->getCurrentTexture();

  if (!IGL_VERIFY(tex.valid())) {
    IGL_ASSERT_MSG(false, "Swapchain has no valid texture");
    return {};
  }

  // a chain of possibilities...
  IGL_ASSERT_MSG((*ctx_->texturesPool_.get(tex))->image_->imageFormat_ != VK_FORMAT_UNDEFINED,
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
