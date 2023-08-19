/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Device.h>

#include <cstring>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/RenderPipelineState.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanSwapchain.h>
#include <igl/vulkan/VulkanTexture.h>
#include <lvk/vulkan/VulkanClasses.h>
#include <lvk/vulkan/VulkanUtils.h>

#include <glslang/Include/glslang_c_interface.h>

namespace {

bool supportsFormat(VkPhysicalDevice physicalDevice, VkFormat format) {
  VkFormatProperties properties;
  vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
  return properties.bufferFeatures != 0 || properties.linearTilingFeatures != 0 ||
         properties.optimalTilingFeatures != 0;
} // namespace

VkShaderStageFlagBits shaderStageToVkShaderStage(lvk::ShaderStage stage) {
  switch (stage) {
  case lvk::Stage_Vert:
    return VK_SHADER_STAGE_VERTEX_BIT;
  case lvk::Stage_Geom:
    return VK_SHADER_STAGE_GEOMETRY_BIT;
  case lvk::Stage_Frag:
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  case lvk::Stage_Comp:
    return VK_SHADER_STAGE_COMPUTE_BIT;
  };
  return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
}

VkMemoryPropertyFlags storageTypeToVkMemoryPropertyFlags(lvk::StorageType storage) {
  VkMemoryPropertyFlags memFlags{0};

  switch (storage) {
  case lvk::StorageType_Device:
    memFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    break;
  case lvk::StorageType_HostVisible:
    memFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    break;
  case lvk::StorageType_Memoryless:
    memFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    break;
  }
  return memFlags;
}

} // namespace

namespace lvk::vulkan {

Device::Device(std::unique_ptr<VulkanContext> ctx) : ctx_(std::move(ctx)) {}

ICommandBuffer& Device::acquireCommandBuffer() {
  LVK_PROFILER_FUNCTION();

  LVK_ASSERT_MSG(!currentCommandBuffer_.ctx_, "Cannot acquire more than 1 command buffer simultaneously");

  currentCommandBuffer_ = CommandBuffer(ctx_.get());

  return currentCommandBuffer_;
}

void Device::submit(const lvk::ICommandBuffer& commandBuffer, TextureHandle present) {
  LVK_PROFILER_FUNCTION();

  const VulkanContext& ctx = getVulkanContext();

  vulkan::CommandBuffer* vkCmdBuffer = const_cast<vulkan::CommandBuffer*>(static_cast<const vulkan::CommandBuffer*>(&commandBuffer));

  LVK_ASSERT(vkCmdBuffer);
  LVK_ASSERT(vkCmdBuffer->ctx_);
  LVK_ASSERT(vkCmdBuffer->wrapper_);

  if (present) {
    const lvk::vulkan::VulkanTexture& tex = *ctx.texturesPool_.get(present);

    LVK_ASSERT(tex.isSwapchainTexture());

    // prepare image for presentation the image might be coming from a compute shader
    const VkPipelineStageFlagBits srcStage = (tex.image_->vkImageLayout_ == VK_IMAGE_LAYOUT_GENERAL)
                                                 ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                                 : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    tex.image_->transitionLayout(
        vkCmdBuffer->wrapper_->cmdBuf_,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        srcStage,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // wait for all subsequent operations
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }

  const bool shouldPresent = ctx.hasSwapchain() && present;

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
  VkBufferUsageFlags usageFlags = (desc.storage == StorageType_Device) ? VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
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
    usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
  }

  if (desc.usage & BufferUsageBits_Storage) {
    usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
  }

  if (desc.usage & BufferUsageBits_Indirect) {
    usageFlags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
  }

  const VkMemoryPropertyFlags memFlags = storageTypeToVkMemoryPropertyFlags(desc.storage);

  Result result;
  BufferHandle handle = ctx_->createBuffer(desc.size, usageFlags, memFlags, &result, desc.debugName);

  if (!LVK_VERIFY(result.isOk())) {
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
  LVK_PROFILER_FUNCTION();

  Result result;

  SamplerHandle handle = ctx_->createSampler(
      samplerStateDescToVkSamplerCreateInfo(desc, ctx_->getVkPhysicalDeviceProperties().limits), &result, desc.debugName);

  if (!LVK_VERIFY(result.isOk())) {
    Result::setResult(outResult, Result(Result::Code::RuntimeError, "Cannot create Sampler"));
    return {};
  }

  Result::setResult(outResult, result);

  return {this, handle};
}

Holder<TextureHandle> Device::createTexture(const TextureDesc& requestedDesc, const char* debugName, Result* outResult) {
  TextureDesc desc(requestedDesc);

  if (debugName && *debugName) {
    desc.debugName = debugName;
  }

  const VkFormat vkFormat = lvk::isDepthOrStencilFormat(desc.format) ? ctx_->getClosestDepthStencilFormat(desc.format)
                                                                     : formatToVkFormat(desc.format);

  const lvk::TextureType type = desc.type;
  if (!LVK_VERIFY(type == TextureType_2D || type == TextureType_Cube || type == TextureType_3D)) {
    LVK_ASSERT_MSG(false, "Only 2D, 3D and Cube textures are supported");
    Result::setResult(outResult, Result::Code::RuntimeError);
    return {};
  }

  if (desc.numMipLevels == 0) {
    LVK_ASSERT_MSG(false, "The number of mip levels specified must be greater than 0");
    desc.numMipLevels = 1;
  }

  if (desc.numSamples > 1 && desc.numMipLevels != 1) {
    LVK_ASSERT_MSG(false, "The number of mip levels for multisampled images should be 1");
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "The number of mip-levels for multisampled images should be 1");
    return {};
  }

  if (desc.numSamples > 1 && type == TextureType_3D) {
    LVK_ASSERT_MSG(false, "Multisampled 3D images are not supported");
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Multisampled 3D images are not supported");
    return {};
  }

  if (!LVK_VERIFY(desc.numMipLevels <= lvk::calcNumMipLevels(desc.dimensions.width, desc.dimensions.height))) {
    Result::setResult(outResult,
                      Result::Code::ArgumentOutOfRange,
                      "The number of specified mip-levels is greater than the maximum possible "
                      "number of mip-levels.");
    return {};
  }

  if (desc.usage == 0) {
    LVK_ASSERT_MSG(false, "Texture usage flags are not set");
    desc.usage = lvk::TextureUsageBits_Sampled;
  }

  /* Use staging device to transfer data into the image when the storage is private to the device */
  VkImageUsageFlags usageFlags = (desc.storage == StorageType_Device) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0;

  if (desc.usage & lvk::TextureUsageBits_Sampled) {
    usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if (desc.usage & lvk::TextureUsageBits_Storage) {
    LVK_ASSERT_MSG(desc.numSamples <= 1, "Storage images cannot be multisampled");
    usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  if (desc.usage & lvk::TextureUsageBits_Attachment) {
    usageFlags |= lvk::isDepthOrStencilFormat(desc.format) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                           : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }

  // For now, always set this flag so we can read it back
  usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  LVK_ASSERT_MSG(usageFlags != 0, "Invalid usage flags");

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
    samples = lvk::getVulkanSampleCountFlags(desc.numSamples);
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
    LVK_ASSERT_MSG(false, "Code should NOT be reached");
    Result::setResult(outResult, Result::Code::RuntimeError, "Unsupported texture type");
    return {};
  }

  Result result;
  auto image = ctx_->createImage(imageType,
                                 VkExtent3D{desc.dimensions.width, desc.dimensions.height, desc.dimensions.depth},
                                 vkFormat,
                                 desc.numMipLevels,
                                 arrayLayerCount,
                                 VK_IMAGE_TILING_OPTIMAL,
                                 usageFlags,
                                 memFlags,
                                 createFlags,
                                 samples,
                                 &result,
                                 debugNameImage);
  if (!LVK_VERIFY(result.isOk())) {
    Result::setResult(outResult, result);
    return {};
  }
  if (!LVK_VERIFY(image.get())) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Cannot create VulkanImage");
    return {};
  }

  VkImageAspectFlags aspect = 0;
  if (image->isDepthFormat_ || image->isStencilFormat_) {
    if (image->isDepthFormat_) {
      aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
    } else if (image->isStencilFormat_) {
      aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  } else {
    aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  VkImageView view =
      image->createImageView(imageViewType, vkFormat, aspect, 0, VK_REMAINING_MIP_LEVELS, 0, arrayLayerCount, debugNameImageView);

  if (!LVK_VERIFY(view != VK_NULL_HANDLE)) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Cannot create VkImageView");
    return {};
  }

  TextureHandle handle = ctx_->texturesPool_.create(vulkan::VulkanTexture(std::move(image), view));

  LVK_ASSERT(ctx_->texturesPool_.numObjects() <= ctx_->config_.maxTextures);

  ctx_->awaitingCreation_ = true;

  if (desc.data) {
    LVK_ASSERT(desc.type == TextureType_2D);
    const void* mipMaps[] = {desc.data};
    Result res = upload(handle, {.dimensions = desc.dimensions, .numMipLevels = 1}, mipMaps);
    if (!res.isOk()) {
      Result::setResult(outResult, res);
      return {};
    }
  }

  Result::setResult(outResult, Result());

  return {this, handle};
}

lvk::Holder<lvk::ComputePipelineHandle> Device::createComputePipeline(const ComputePipelineDesc& desc, Result* outResult) {
  if (!LVK_VERIFY(desc.shaderModule.valid())) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Missing compute shader");
    return {};
  }

  const VkShaderModule* sm = ctx_->shaderModulesPool_.get(desc.shaderModule);

  LVK_ASSERT(sm);

  const VkComputePipelineCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .flags = 0,
      .stage = lvk::getPipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, *sm, desc.entryPoint),
      .layout = ctx_->vkPipelineLayout_,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };
  VkPipeline pipeline = VK_NULL_HANDLE;
  VK_ASSERT(vkCreateComputePipelines(ctx_->getVkDevice(), ctx_->pipelineCache_, 1, &ci, nullptr, &pipeline));
  VK_ASSERT(lvk::setDebugObjectName(ctx_->getVkDevice(), VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline, desc.debugName));

  // a shader module can be destroyed while pipelines created using its shaders are still in use
  // https://registry.khronos.org/vulkan/specs/1.3/html/chap9.html#vkDestroyShaderModule
  destroy(desc.shaderModule);

  return {this, ctx_->computePipelinesPool_.create(std::move(pipeline))};
}

lvk::Holder<lvk::RenderPipelineHandle> Device::createRenderPipeline(const RenderPipelineDesc& desc, Result* outResult) {
  const bool hasColorAttachments = desc.getNumColorAttachments() > 0;
  const bool hasDepthAttachment = desc.depthFormat != Format_Invalid;
  const bool hasAnyAttachments = hasColorAttachments || hasDepthAttachment;
  if (!LVK_VERIFY(hasAnyAttachments)) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Need at least one attachment");
    return {};
  }

  if (!LVK_VERIFY(desc.smVert.valid())) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Missing vertex shader");
    return {};
  }

  if (!LVK_VERIFY(desc.smFrag.valid())) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Missing fragment shader");
    return {};
  }

  return {this, ctx_->renderPipelinesPool_.create(RenderPipelineState(this, desc))};
}

void Device::destroy(lvk::ComputePipelineHandle handle) {
  VkPipeline* pipeline = ctx_->computePipelinesPool_.get(handle);

  LVK_ASSERT(pipeline);
  LVK_ASSERT(*pipeline != VK_NULL_HANDLE);

  ctx_->deferredTask(
      std::packaged_task<void()>([device = ctx_->getVkDevice(), pipeline = *pipeline]() { vkDestroyPipeline(device, pipeline, nullptr); }));

  ctx_->computePipelinesPool_.destroy(handle);
}

void Device::destroy(lvk::RenderPipelineHandle handle) {
  ctx_->renderPipelinesPool_.destroy(handle);
}

void Device::destroy(lvk::ShaderModuleHandle handle) {
  const VkShaderModule* sm = ctx_->shaderModulesPool_.get(handle);

  LVK_ASSERT(sm);

  if (*sm != VK_NULL_HANDLE) {
    vkDestroyShaderModule(ctx_->getVkDevice(), *sm, nullptr);
  }

  ctx_->shaderModulesPool_.destroy(handle);
}

void Device::destroy(SamplerHandle handle) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_DESTROY);

  VkSampler sampler = *ctx_->samplersPool_.get(handle);

  ctx_->samplersPool_.destroy(handle);

  ctx_->deferredTask(
      std::packaged_task<void()>([device = ctx_->vkDevice_, sampler = sampler]() { vkDestroySampler(device, sampler, nullptr); }));

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
      lvk::vulkan::VulkanTexture* tex = ctx_->texturesPool_.get(handle);
      if (!tex || tex->isSwapchainTexture())
        return;
      destroy(handle);
      handle = {};
    }
  };

  for (Framebuffer::AttachmentDesc& a : fb.color) {
    destroyFbTexture(a.texture);
    destroyFbTexture(a.resolveTexture);
  }
  destroyFbTexture(fb.depthStencil.texture);
  destroyFbTexture(fb.depthStencil.resolveTexture);
}

Result Device::upload(BufferHandle handle, const void* data, size_t size, size_t offset) {
  LVK_PROFILER_FUNCTION();

  if (!LVK_VERIFY(data)) {
    return lvk::Result();
  }

  lvk::VulkanBuffer* buf = ctx_->buffersPool_.get(handle);

  if (!LVK_VERIFY(buf)) {
    return lvk::Result();
  }

  if (!LVK_VERIFY(offset + size <= buf->bufferSize_)) {
    return lvk::Result(Result::Code::ArgumentOutOfRange, "Out of range");
  }

  ctx_->stagingDevice_->bufferSubData(*buf, offset, size, data);

  return lvk::Result();
}

uint8_t* Device::getMappedPtr(BufferHandle handle) const {
  lvk::VulkanBuffer* buf = ctx_->buffersPool_.get(handle);

  LVK_ASSERT(buf);

  return buf->isMapped() ? buf->getMappedPtr() : nullptr;
}

uint64_t Device::gpuAddress(BufferHandle handle, size_t offset) const {
  LVK_ASSERT_MSG((offset & 7) == 0, "Buffer offset must be 8 bytes aligned as per GLSL_EXT_buffer_reference spec.");

  lvk::VulkanBuffer* buf = ctx_->buffersPool_.get(handle);

  LVK_ASSERT(buf);

  return buf ? (uint64_t)buf->vkDeviceAddress_ + offset : 0u;
}

void Device::flushMappedMemory(BufferHandle handle, size_t offset, size_t size) const {
  lvk::VulkanBuffer* buf = ctx_->buffersPool_.get(handle);

  LVK_ASSERT(buf);

  buf->flushMappedMemory(offset, size);
}

static Result validateRange(const lvk::Dimensions& dimensions, uint32_t numLevels, const lvk::TextureRangeDesc& range) {
  if (!LVK_VERIFY(range.dimensions.width > 0 && range.dimensions.height > 0 || range.dimensions.depth > 0 || range.numLayers > 0 ||
                  range.numMipLevels > 0)) {
    return Result{Result::Code::ArgumentOutOfRange, "width, height, depth numLayers, and numMipLevels must be > 0"};
  }
  if (range.mipLevel > numLevels) {
    return Result{Result::Code::ArgumentOutOfRange, "range.mipLevel exceeds texture mip-levels"};
  }

  const auto texWidth = std::max(dimensions.width >> range.mipLevel, 1u);
  const auto texHeight = std::max(dimensions.height >> range.mipLevel, 1u);
  const auto texDepth = std::max(dimensions.depth >> range.mipLevel, 1u);

  if (range.dimensions.width > texWidth || range.dimensions.height > texHeight || range.dimensions.depth > texDepth) {
    return Result{Result::Code::ArgumentOutOfRange, "range dimensions exceed texture dimensions"};
  }
  if (range.x > texWidth - range.dimensions.width || range.y > texHeight - range.dimensions.height ||
      range.z > texDepth - range.dimensions.depth) {
    return Result{Result::Code::ArgumentOutOfRange, "range dimensions exceed texture dimensions"};
  }

  return Result{};
}

Result Device::upload(TextureHandle handle, const TextureRangeDesc& range, const void* data[]) const {
  if (!data) {
    return lvk::Result();
  }

  lvk::vulkan::VulkanTexture* texture = ctx_->texturesPool_.get(handle);

  const auto result = validateRange(texture->getDimensions(), texture->image_->numLevels_, range);

  if (!LVK_VERIFY(result.isOk())) {
    return result;
  }

  const uint32_t numLayers = std::max(range.numLayers, 1u);

  const VkImageType type = texture->image_->vkType_;
  VkFormat vkFormat = texture->image_->vkImageFormat_;

  if (type == VK_IMAGE_TYPE_3D) {
    const void* uploadData = data[0];
    ctx_->stagingDevice_->imageData3D(*texture->image_.get(),
                                      VkOffset3D{(int32_t)range.x, (int32_t)range.y, (int32_t)range.z},
                                      VkExtent3D{range.dimensions.width, range.dimensions.height, range.dimensions.depth},
                                      vkFormat,
                                      uploadData);
  } else {
    const VkRect2D imageRegion = {
        .offset = {.x = (int)range.x, .y = (int)range.y},
        .extent = {.width = range.dimensions.width, .height = range.dimensions.height},
    };
    ctx_->stagingDevice_->imageData2D(
        *texture->image_.get(), imageRegion, range.mipLevel, range.numMipLevels, range.layer, range.numLayers, vkFormat, data);
  }

  return Result();
}

Dimensions Device::getDimensions(TextureHandle handle) const {
  if (!handle) {
    return {};
  }

  return ctx_->texturesPool_.get(handle)->getDimensions();
}

void Device::generateMipmap(TextureHandle handle) const {
  if (handle.empty()) {
    return;
  }

  lvk::vulkan::VulkanTexture* tex = ctx_->texturesPool_.get(handle);

  if (tex->image_->numLevels_ > 1) {
    LVK_ASSERT(tex->image_->vkImageLayout_ != VK_IMAGE_LAYOUT_UNDEFINED);
    const auto& wrapper = ctx_->immediate_->acquire();
    tex->image_->generateMipmap(wrapper.cmdBuf_);
    ctx_->immediate_->submit(wrapper);
  }
}

Format Device::getFormat(TextureHandle handle) const {
  if (handle.empty()) {
    return Format_Invalid;
  }

  return vkFormatToFormat(ctx_->texturesPool_.get(handle)->image_->vkImageFormat_);
}

lvk::Holder<lvk::ShaderModuleHandle> Device::createShaderModule(const ShaderModuleDesc& desc, Result* outResult) {
  Result result;
  VkShaderModule sm = desc.dataSize ?
                                    // binary
                          createShaderModule(desc.data, desc.dataSize, desc.debugName, &result)
                                    :
                                    // text
                          createShaderModule(desc.stage, desc.data, desc.debugName, &result);

  if (!result.isOk()) {
    Result::setResult(outResult, result);
    return {};
  }
  Result::setResult(outResult, result);

  return {this, ctx_->shaderModulesPool_.create(std::move(sm))};
}

VkShaderModule Device::createShaderModule(const void* data, size_t length, const char* debugName, Result* outResult) const {
  VkShaderModule vkShaderModule = VK_NULL_HANDLE;

  const VkShaderModuleCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = length,
      .pCode = (const uint32_t*)data,
  };
  const VkResult result = vkCreateShaderModule(ctx_->vkDevice_, &ci, nullptr, &vkShaderModule);

  lvk::setResultFrom(outResult, result);

  if (result != VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }

  VK_ASSERT(lvk::setDebugObjectName(ctx_->vkDevice_, VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)vkShaderModule, debugName));

  LVK_ASSERT(vkShaderModule != VK_NULL_HANDLE);

  return vkShaderModule;
}

VkShaderModule Device::createShaderModule(ShaderStage stage, const char* source, const char* debugName, Result* outResult) const {
  const VkShaderStageFlagBits vkStage = shaderStageToVkShaderStage(stage);
  LVK_ASSERT(vkStage != VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);
  LVK_ASSERT(source);

  std::string sourcePatched;

  if (!source || !*source) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Shader source is empty");
    return VK_NULL_HANDLE;
  }

  if (strstr(source, "#version ") == nullptr) {
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

  const glslang_resource_t glslangResource = lvk::getGlslangResource(ctx_->getVkPhysicalDeviceProperties().limits);

  VkShaderModule vkShaderModule = VK_NULL_HANDLE;
  const Result result = lvk::compileShader(ctx_->vkDevice_, vkStage, source, &vkShaderModule, &glslangResource);

  Result::setResult(outResult, result);

  if (!result.isOk()) {
    return VK_NULL_HANDLE;
  }

  VK_ASSERT(lvk::setDebugObjectName(ctx_->vkDevice_, VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)vkShaderModule, debugName));

  LVK_ASSERT(vkShaderModule != VK_NULL_HANDLE);

  return vkShaderModule;
}

Format Device::getSwapchainFormat() const {
  if (!ctx_->hasSwapchain()) {
    return Format_Invalid;
  }

  return getFormat(ctx_->swapchain_->getCurrentTexture());
}

TextureHandle Device::getCurrentSwapchainTexture() {
  LVK_PROFILER_FUNCTION();

  if (!ctx_->hasSwapchain()) {
    return {};
  }

  TextureHandle tex = ctx_->swapchain_->getCurrentTexture();

  if (!LVK_VERIFY(tex.valid())) {
    LVK_ASSERT_MSG(false, "Swapchain has no valid texture");
    return {};
  }

  LVK_ASSERT_MSG(ctx_->texturesPool_.get(tex)->image_->vkImageFormat_ != VK_FORMAT_UNDEFINED, "Invalid image format");

  return tex;
}

void Device::recreateSwapchain(int newWidth, int newHeight) {
  ctx_->initSwapchain(newWidth, newHeight);
}

std::unique_ptr<VulkanContext> Device::createContext(const lvk::VulkanContextConfig& config, void* window, void* display) {
  return std::make_unique<VulkanContext>(config, window, display);
}

std::vector<HWDeviceDesc> Device::queryDevices(VulkanContext& ctx, HWDeviceType deviceType, Result* outResult) {
  std::vector<HWDeviceDesc> outDevices;

  Result::setResult(outResult, ctx.queryDevices(deviceType, outDevices));

  return outDevices;
}

std::unique_ptr<IDevice> Device::create(std::unique_ptr<VulkanContext> ctx,
                                        const HWDeviceDesc& desc,
                                        uint32_t width,
                                        uint32_t height,
                                        Result* outResult) {
  LVK_ASSERT(ctx.get());

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
