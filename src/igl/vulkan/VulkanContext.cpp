/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstring>
#include <set>
#include <vector>

#define VMA_IMPLEMENTATION
#define VOLK_IMPLEMENTATION

#include <igl/vulkan/VulkanContext.h>
#include <lvk/vulkan/VulkanClasses.h>
#include <lvk/vulkan/VulkanUtils.h>

#include <glslang/Include/glslang_c_interface.h>

static_assert(lvk::HWDeviceDesc::LVK_MAX_PHYSICAL_DEVICE_NAME_SIZE == VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);

namespace {

const char* kDefaultValidationLayers[] = {"VK_LAYER_KHRONOS_validation"};

// These bindings should match GLSL declarations injected into shaders in VulkanContext::createShaderModule().
enum Bindings {
  kBinding_Textures = 0,
  kBinding_Samplers = 1,
  kBinding_StorageImages = 2,
  kBinding_NumBindins = 3,
};

VKAPI_ATTR VkBool32 VKAPI_CALL
vulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity,
                    [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT msgType,
                    const VkDebugUtilsMessengerCallbackDataEXT* cbData,
                    void* userData) {
  if (msgSeverity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    return VK_FALSE;
  }

  const bool isError = (msgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0;

  char errorName[128] = {};
  int object = 0;
  void* handle = nullptr;
  char typeName[128] = {};
  void* messageID = nullptr;

  if (sscanf(cbData->pMessage,
             "Validation Error : [ %127s ] Object %i: handle = %p, type = %127s | MessageID = %p",
             errorName,
             &object,
             &handle,
             typeName,
             &messageID) >= 2) {
    const char* message = strrchr(cbData->pMessage, '|') + 1;
    LLOGL(
        "%sValidation layer:\n Validation Error: %s \n Object %i: handle = %p, type = %s\n "
        "MessageID = %p \n%s \n",
        isError ? "\nERROR:\n" : "",
        errorName,
        object,
        handle,
        typeName,
        messageID,
        message);
  } else {
    LLOGL("%sValidation layer:\n%s\n", isError ? "\nERROR:\n" : "", cbData->pMessage);
  }

  if (isError) {
    lvk::vulkan::VulkanContext* ctx = static_cast<lvk::vulkan::VulkanContext*>(userData);
    if (ctx->config_.terminateOnValidationError) {
      LVK_ASSERT(false);
      std::terminate();
    }
  }

  return VK_FALSE;
}

bool supportsFormat(VkPhysicalDevice physicalDevice, VkFormat format) {
  VkFormatProperties properties;
  vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
  return properties.bufferFeatures != 0 || properties.linearTilingFeatures != 0 ||
         properties.optimalTilingFeatures != 0;
}

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

std::vector<VkFormat> getCompatibleDepthStencilFormats(lvk::Format format) {
  switch (format) {
  case lvk::Format_Z_UN16:
    return {VK_FORMAT_D16_UNORM,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D32_SFLOAT};
  case lvk::Format_Z_UN24:
    return {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM_S8_UINT};
  case lvk::Format_Z_F32:
    return {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
  case lvk::Format_Z_UN24_S_UI8:
    return {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT};
  default:
    return {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT};
  }
  return {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT};
}

bool validateImageLimits(VkImageType imageType,
                         VkSampleCountFlagBits samples,
                         const VkExtent3D& extent,
                         const VkPhysicalDeviceLimits& limits,
                         lvk::Result* outResult) {
  using lvk::Result;

  if (samples != VK_SAMPLE_COUNT_1_BIT && !LVK_VERIFY(imageType == VK_IMAGE_TYPE_2D)) {
    Result::setResult(outResult, Result(Result::Code::ArgumentOutOfRange, "Multisampling is supported only for 2D images"));
    return false;
  }

  if (imageType == VK_IMAGE_TYPE_2D &&
      !LVK_VERIFY(extent.width <= limits.maxImageDimension2D && extent.height <= limits.maxImageDimension2D)) {
    Result::setResult(outResult, Result(Result::Code::ArgumentOutOfRange, "2D texture size exceeded"));
    return false;
  }
  if (imageType == VK_IMAGE_TYPE_3D &&
      !LVK_VERIFY(extent.width <= limits.maxImageDimension3D && extent.height <= limits.maxImageDimension3D &&
                  extent.depth <= limits.maxImageDimension3D)) {
    Result::setResult(outResult, Result(Result::Code::ArgumentOutOfRange, "3D texture size exceeded"));
    return false;
  }

  return true;
}

bool isHostVisibleSingleHeapMemory(VkPhysicalDevice physDev) {
  VkPhysicalDeviceMemoryProperties memProperties;

  vkGetPhysicalDeviceMemoryProperties(physDev, &memProperties);

  if (memProperties.memoryHeapCount != 1) {
    return false;
  }

  const uint32_t flag = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((memProperties.memoryTypes[i].propertyFlags & flag) == flag) {
      return true;
    }
  }

  return false;
}

void getInstanceExtensionProps(std::vector<VkExtensionProperties>& props,
                               const char* validationLayer = nullptr) {
  uint32_t numExtensions = 0;
  vkEnumerateInstanceExtensionProperties(validationLayer, &numExtensions, nullptr);
  std::vector<VkExtensionProperties> p(numExtensions);
  vkEnumerateInstanceExtensionProperties(validationLayer, &numExtensions, p.data());
  props.insert(props.end(), p.begin(), p.end());
}

void getDeviceExtensionProps(VkPhysicalDevice dev,
                             std::vector<VkExtensionProperties>& props,
                             const char* validationLayer = nullptr) {
  uint32_t numExtensions = 0;
  vkEnumerateDeviceExtensionProperties(dev, validationLayer, &numExtensions, nullptr);
  std::vector<VkExtensionProperties> p(numExtensions);
  vkEnumerateDeviceExtensionProperties(dev, validationLayer, &numExtensions, p.data());
  props.insert(props.end(), p.begin(), p.end());
}

bool hasExtension(const char* ext, const std::vector<VkExtensionProperties>& props) {
  for (const VkExtensionProperties& p : props) {
    if (strcmp(ext, p.extensionName) == 0)
      return true;
  }
  return false;
}

} // namespace

namespace lvk {
namespace vulkan {

struct VulkanContextImpl final {
  // Vulkan Memory Allocator
  VmaAllocator vma_ = VK_NULL_HANDLE;

  lvk::CommandBuffer currentCommandBuffer_;
};

VulkanContext::VulkanContext(const lvk::ContextConfig& config,
                             void* window,
                             void* display) :
  config_(config) {
  LVK_PROFILER_THREAD("MainThread");

  pimpl_ = std::make_unique<VulkanContextImpl>();

  if (volkInitialize() != VK_SUCCESS) {
    LLOGW("volkInitialize() failed\n");
    exit(255);
  };

  glslang_initialize_process();

  createInstance();

  if (window) {
    createSurface(window, display);
  }
}

VulkanContext::~VulkanContext() {
  LVK_PROFILER_FUNCTION();

  VK_ASSERT(vkDeviceWaitIdle(vkDevice_));

  stagingDevice_.reset(nullptr);
  swapchain_.reset(nullptr); // swapchain has to be destroyed prior to Surface

  if (shaderModulesPool_.numObjects()) {
    LLOGW("Leaked %u shader modules\n", shaderModulesPool_.numObjects());
  }
  if (renderPipelinesPool_.numObjects()) {
    LLOGW("Leaked %u render pipelines\n", renderPipelinesPool_.numObjects());
  }
  if (computePipelinesPool_.numObjects()) {
    LLOGW("Leaked %u compute pipelines\n", computePipelinesPool_.numObjects());
  }
  if (samplersPool_.numObjects() > 1) {
    // the dummy value is owned by the context
    LLOGW("Leaked %u samplers\n", samplersPool_.numObjects() - 1);
  }
  if (texturesPool_.numObjects() > 1) {
    // the dummy value is owned by the context
    LLOGW("Leaked %u textures\n", texturesPool_.numObjects() - 1);
  }
  if (buffersPool_.numObjects()) {
    LLOGW("Leaked %u buffers\n", buffersPool_.numObjects());
  }

  // manually destroy the dummy sampler
  vkDestroySampler(vkDevice_, samplersPool_.objects_.front().obj_, nullptr);
  samplersPool_.clear();
  computePipelinesPool_.clear();
  renderPipelinesPool_.clear();
  shaderModulesPool_.clear();
  texturesPool_.clear();

  waitDeferredTasks();

  immediate_.reset(nullptr);

  vkDestroyDescriptorSetLayout(vkDevice_, vkDSLBindless_, nullptr);
  vkDestroyPipelineLayout(vkDevice_, vkPipelineLayout_, nullptr);
  vkDestroyDescriptorPool(vkDevice_, vkDPBindless_, nullptr);
  vkDestroySurfaceKHR(vkInstance_, vkSurface_, nullptr);
  vkDestroyPipelineCache(vkDevice_, pipelineCache_, nullptr);

  // Clean up VMA
  if (LVK_VULKAN_USE_VMA) {
    vmaDestroyAllocator(pimpl_->vma_);
  }

  // Device has to be destroyed prior to Instance
  vkDestroyDevice(vkDevice_, nullptr);

  vkDestroyDebugUtilsMessengerEXT(vkInstance_, vkDebugUtilsMessenger_, nullptr);
  vkDestroyInstance(vkInstance_, nullptr);

  glslang_finalize_process();

  LLOGL("Vulkan graphics pipelines created: %u\n",
               VulkanPipelineBuilder::getNumPipelinesCreated());
}

ICommandBuffer& VulkanContext::acquireCommandBuffer() {
  LVK_PROFILER_FUNCTION();

  LVK_ASSERT_MSG(!pimpl_->currentCommandBuffer_.ctx_, "Cannot acquire more than 1 command buffer simultaneously");

  pimpl_->currentCommandBuffer_ = CommandBuffer(this);

  return pimpl_->currentCommandBuffer_;
}

void VulkanContext::submit(lvk::ICommandBuffer& commandBuffer, TextureHandle present) {
  LVK_PROFILER_FUNCTION();

  CommandBuffer* vkCmdBuffer = static_cast<CommandBuffer*>(&commandBuffer);

  LVK_ASSERT(vkCmdBuffer);
  LVK_ASSERT(vkCmdBuffer->ctx_);
  LVK_ASSERT(vkCmdBuffer->wrapper_);

  if (present) {
    const lvk::VulkanTexture& tex = *texturesPool_.get(present);

    LVK_ASSERT(tex.image_->isSwapchainImage_);

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

  const bool shouldPresent = hasSwapchain() && present;

  if (shouldPresent) {
    immediate_->waitSemaphore(swapchain_->acquireSemaphore_);
  }

  vkCmdBuffer->lastSubmitHandle_ = immediate_->submit(*vkCmdBuffer->wrapper_);

  if (shouldPresent) {
    swapchain_->present(immediate_->acquireLastSubmitSemaphore());
  }

  processDeferredTasks();

  // reset
  pimpl_->currentCommandBuffer_ = {};
}

Holder<BufferHandle> VulkanContext::createBuffer(const BufferDesc& requestedDesc, Result* outResult) {
  BufferDesc desc = requestedDesc;

  if (!useStaging_ && (desc.storage == StorageType_Device)) {
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
  BufferHandle handle = createBuffer(desc.size, usageFlags, memFlags, &result, desc.debugName);

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

Holder<SamplerHandle> VulkanContext::createSampler(const SamplerStateDesc& desc, Result* outResult) {
  LVK_PROFILER_FUNCTION();

  Result result;

  SamplerHandle handle = createSampler(
      samplerStateDescToVkSamplerCreateInfo(desc, getVkPhysicalDeviceProperties().limits), &result, desc.debugName);

  if (!LVK_VERIFY(result.isOk())) {
    Result::setResult(outResult, Result(Result::Code::RuntimeError, "Cannot create Sampler"));
    return {};
  }

  Result::setResult(outResult, result);

  return {this, handle};
}

Holder<TextureHandle> VulkanContext::createTexture(const TextureDesc& requestedDesc, const char* debugName, Result* outResult) {
  TextureDesc desc(requestedDesc);

  if (debugName && *debugName) {
    desc.debugName = debugName;
  }

  const VkFormat vkFormat = lvk::isDepthOrStencilFormat(desc.format) ? getClosestDepthStencilFormat(desc.format)
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
  auto image = createImage(imageType,
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

  TextureHandle handle = texturesPool_.create(lvk::VulkanTexture(std::move(image), view));

  LVK_ASSERT(texturesPool_.numObjects() <= config_.maxTextures);

  awaitingCreation_ = true;

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

lvk::Holder<lvk::ComputePipelineHandle> VulkanContext::createComputePipeline(const ComputePipelineDesc& desc, Result* outResult) {
  if (!LVK_VERIFY(desc.shaderModule.valid())) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Missing compute shader");
    return {};
  }

  const VkShaderModule* sm = shaderModulesPool_.get(desc.shaderModule);

  LVK_ASSERT(sm);

  const VkComputePipelineCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .flags = 0,
      .stage = lvk::getPipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, *sm, desc.entryPoint),
      .layout = vkPipelineLayout_,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };
  VkPipeline pipeline = VK_NULL_HANDLE;
  VK_ASSERT(vkCreateComputePipelines(getVkDevice(), pipelineCache_, 1, &ci, nullptr, &pipeline));
  VK_ASSERT(lvk::setDebugObjectName(getVkDevice(), VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline, desc.debugName));

  // a shader module can be destroyed while pipelines created using its shaders are still in use
  // https://registry.khronos.org/vulkan/specs/1.3/html/chap9.html#vkDestroyShaderModule
  destroy(desc.shaderModule);

  return {this, computePipelinesPool_.create(std::move(pipeline))};
}

lvk::Holder<lvk::RenderPipelineHandle> VulkanContext::createRenderPipeline(const RenderPipelineDesc& desc, Result* outResult) {
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

  return {this, renderPipelinesPool_.create(RenderPipelineState(this, desc))};
}

void VulkanContext::destroy(lvk::ComputePipelineHandle handle) {
  VkPipeline* pipeline = computePipelinesPool_.get(handle);

  LVK_ASSERT(pipeline);
  LVK_ASSERT(*pipeline != VK_NULL_HANDLE);

  deferredTask(
      std::packaged_task<void()>([device = getVkDevice(), pipeline = *pipeline]() { vkDestroyPipeline(device, pipeline, nullptr); }));

  computePipelinesPool_.destroy(handle);
}

void VulkanContext::destroy(lvk::RenderPipelineHandle handle) {
  renderPipelinesPool_.destroy(handle);
}

void VulkanContext::destroy(lvk::ShaderModuleHandle handle) {
  const VkShaderModule* sm = shaderModulesPool_.get(handle);

  LVK_ASSERT(sm);

  if (*sm != VK_NULL_HANDLE) {
    // a shader module can be destroyed while pipelines created using its shaders are still in use
    // https://registry.khronos.org/vulkan/specs/1.3/html/chap9.html#vkDestroyShaderModule
    vkDestroyShaderModule(getVkDevice(), *sm, nullptr);
  }

  shaderModulesPool_.destroy(handle);
}

void VulkanContext::destroy(SamplerHandle handle) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_DESTROY);

  VkSampler sampler = *samplersPool_.get(handle);

  samplersPool_.destroy(handle);

  deferredTask(
      std::packaged_task<void()>([device = vkDevice_, sampler = sampler]() { vkDestroySampler(device, sampler, nullptr); }));
}

void VulkanContext::destroy(BufferHandle handle) {
	// deferred deletion handled in VulkanBuffer
  buffersPool_.destroy(handle);
}

void VulkanContext::destroy(lvk::TextureHandle handle) {
  // deferred deletion handled in VulkanTexture
  texturesPool_.destroy(handle);
}

void VulkanContext::destroy(Framebuffer& fb) {
  auto destroyFbTexture = [this](TextureHandle& handle) {
    {
      if (handle.empty())
        return;
      lvk::VulkanTexture* tex = texturesPool_.get(handle);
      if (!tex || tex->image_->isSwapchainImage_)
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

Result VulkanContext::upload(BufferHandle handle, const void* data, size_t size, size_t offset) {
  LVK_PROFILER_FUNCTION();

  if (!LVK_VERIFY(data)) {
    return lvk::Result();
  }

  lvk::VulkanBuffer* buf = buffersPool_.get(handle);

  if (!LVK_VERIFY(buf)) {
    return lvk::Result();
  }

  if (!LVK_VERIFY(offset + size <= buf->bufferSize_)) {
    return lvk::Result(Result::Code::ArgumentOutOfRange, "Out of range");
  }

  stagingDevice_->bufferSubData(*buf, offset, size, data);

  return lvk::Result();
}

uint8_t* VulkanContext::getMappedPtr(BufferHandle handle) const {
  const lvk::VulkanBuffer* buf = buffersPool_.get(handle);

  LVK_ASSERT(buf);

  return buf->isMapped() ? buf->getMappedPtr() : nullptr;
}

uint64_t VulkanContext::gpuAddress(BufferHandle handle, size_t offset) const {
  LVK_ASSERT_MSG((offset & 7) == 0, "Buffer offset must be 8 bytes aligned as per GLSL_EXT_buffer_reference spec.");

  const lvk::VulkanBuffer* buf = buffersPool_.get(handle);

  LVK_ASSERT(buf);

  return buf ? (uint64_t)buf->vkDeviceAddress_ + offset : 0u;
}

void VulkanContext::flushMappedMemory(BufferHandle handle, size_t offset, size_t size) const {
  const lvk::VulkanBuffer* buf = buffersPool_.get(handle);

  LVK_ASSERT(buf);

  buf->flushMappedMemory(offset, size);
}

static Result validateRange(const VkExtent3D& ext, uint32_t numLevels, const lvk::TextureRangeDesc& range) {
  if (!LVK_VERIFY(range.dimensions.width > 0 && range.dimensions.height > 0 || range.dimensions.depth > 0 || range.numLayers > 0 ||
                  range.numMipLevels > 0)) {
    return Result{Result::Code::ArgumentOutOfRange, "width, height, depth numLayers, and numMipLevels must be > 0"};
  }
  if (range.mipLevel > numLevels) {
    return Result{Result::Code::ArgumentOutOfRange, "range.mipLevel exceeds texture mip-levels"};
  }

  const auto texWidth = std::max(ext.width >> range.mipLevel, 1u);
  const auto texHeight = std::max(ext.height >> range.mipLevel, 1u);
  const auto texDepth = std::max(ext.depth >> range.mipLevel, 1u);

  if (range.dimensions.width > texWidth || range.dimensions.height > texHeight || range.dimensions.depth > texDepth) {
    return Result{Result::Code::ArgumentOutOfRange, "range dimensions exceed texture dimensions"};
  }
  if (range.x > texWidth - range.dimensions.width || range.y > texHeight - range.dimensions.height ||
      range.z > texDepth - range.dimensions.depth) {
    return Result{Result::Code::ArgumentOutOfRange, "range dimensions exceed texture dimensions"};
  }

  return Result{};
}

Result VulkanContext::upload(TextureHandle handle, const TextureRangeDesc& range, const void* data[]) {
  if (!data) {
    return Result();
  }

  lvk::VulkanTexture* texture = texturesPool_.get(handle);

  const Result result = validateRange(texture->getExtent(), texture->image_->numLevels_, range);

  if (!LVK_VERIFY(result.isOk())) {
    return result;
  }

  const uint32_t numLayers = std::max(range.numLayers, 1u);

  const VkImageType type = texture->image_->vkType_;
  VkFormat vkFormat = texture->image_->vkImageFormat_;

  if (type == VK_IMAGE_TYPE_3D) {
    const void* uploadData = data[0];
    stagingDevice_->imageData3D(*texture->image_.get(),
                                      VkOffset3D{(int32_t)range.x, (int32_t)range.y, (int32_t)range.z},
                                      VkExtent3D{range.dimensions.width, range.dimensions.height, range.dimensions.depth},
                                      vkFormat,
                                      uploadData);
  } else {
    const VkRect2D imageRegion = {
        .offset = {.x = (int)range.x, .y = (int)range.y},
        .extent = {.width = range.dimensions.width, .height = range.dimensions.height},
    };
    stagingDevice_->imageData2D(
        *texture->image_.get(), imageRegion, range.mipLevel, range.numMipLevels, range.layer, range.numLayers, vkFormat, data);
  }

  return Result();
}

Dimensions VulkanContext::getDimensions(TextureHandle handle) const {
  if (!handle) {
    return {};
  }

  const lvk::VulkanTexture* tex = texturesPool_.get(handle);

  LVK_ASSERT(tex);

  if (!tex || !tex->image_) {
    return {};
  }

  return {
      .width = tex->image_->vkExtent_.width,
      .height = tex->image_->vkExtent_.height,
      .depth = tex->image_->vkExtent_.depth,
  };
}

void VulkanContext::generateMipmap(TextureHandle handle) const {
  if (handle.empty()) {
    return;
  }

  const lvk::VulkanTexture* tex = texturesPool_.get(handle);

  if (tex->image_->numLevels_ > 1) {
    LVK_ASSERT(tex->image_->vkImageLayout_ != VK_IMAGE_LAYOUT_UNDEFINED);
    const auto& wrapper = immediate_->acquire();
    tex->image_->generateMipmap(wrapper.cmdBuf_);
    immediate_->submit(wrapper);
  }
}

Format VulkanContext::getFormat(TextureHandle handle) const {
  if (handle.empty()) {
    return Format_Invalid;
  }

  return vkFormatToFormat(texturesPool_.get(handle)->image_->vkImageFormat_);
}

lvk::Holder<lvk::ShaderModuleHandle> VulkanContext::createShaderModule(const ShaderModuleDesc& desc, Result* outResult) {
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

  return {this, shaderModulesPool_.create(std::move(sm))};
}

VkShaderModule VulkanContext::createShaderModule(const void* data, size_t length, const char* debugName, Result* outResult) const {
  VkShaderModule vkShaderModule = VK_NULL_HANDLE;

  const VkShaderModuleCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = length,
      .pCode = (const uint32_t*)data,
  };
  const VkResult result = vkCreateShaderModule(vkDevice_, &ci, nullptr, &vkShaderModule);

  lvk::setResultFrom(outResult, result);

  if (result != VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }

  VK_ASSERT(lvk::setDebugObjectName(vkDevice_, VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)vkShaderModule, debugName));

  LVK_ASSERT(vkShaderModule != VK_NULL_HANDLE);

  return vkShaderModule;
}

VkShaderModule VulkanContext::createShaderModule(ShaderStage stage, const char* source, const char* debugName, Result* outResult) const {
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

  const glslang_resource_t glslangResource = lvk::getGlslangResource(getVkPhysicalDeviceProperties().limits);

  VkShaderModule vkShaderModule = VK_NULL_HANDLE;
  const Result result = lvk::compileShader(vkDevice_, vkStage, source, &vkShaderModule, &glslangResource);

  Result::setResult(outResult, result);

  if (!result.isOk()) {
    return VK_NULL_HANDLE;
  }

  VK_ASSERT(lvk::setDebugObjectName(vkDevice_, VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)vkShaderModule, debugName));

  LVK_ASSERT(vkShaderModule != VK_NULL_HANDLE);

  return vkShaderModule;
}

Format VulkanContext::getSwapchainFormat() const {
  if (!hasSwapchain()) {
    return Format_Invalid;
  }

  return getFormat(swapchain_->getCurrentTexture());
}

TextureHandle VulkanContext::getCurrentSwapchainTexture() {
  LVK_PROFILER_FUNCTION();

  if (!hasSwapchain()) {
    return {};
  }

  TextureHandle tex = swapchain_->getCurrentTexture();

  if (!LVK_VERIFY(tex.valid())) {
    LVK_ASSERT_MSG(false, "Swapchain has no valid texture");
    return {};
  }

  LVK_ASSERT_MSG(texturesPool_.get(tex)->image_->vkImageFormat_ != VK_FORMAT_UNDEFINED, "Invalid image format");

  return tex;
}

void VulkanContext::recreateSwapchain(int newWidth, int newHeight) {
  initSwapchain(newWidth, newHeight);
}

void VulkanContext::createInstance() {
  vkInstance_ = VK_NULL_HANDLE;

  const char* instanceExtensionNames[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#if defined(_WIN32)
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(__linux__)
    VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif
    VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME // enabled only for validation
  };

  const uint32_t numInstanceExtensions = config_.enableValidation ? (uint32_t)LVK_ARRAY_NUM_ELEMENTS(instanceExtensionNames)
                                                                  : (uint32_t)LVK_ARRAY_NUM_ELEMENTS(instanceExtensionNames) - 1;

  const VkValidationFeatureEnableEXT validationFeaturesEnabled[] = {
      VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
  };

  const VkValidationFeaturesEXT features = {
      .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
      .pNext = nullptr,
      .enabledValidationFeatureCount =
          config_.enableValidation ? (uint32_t)LVK_ARRAY_NUM_ELEMENTS(validationFeaturesEnabled)
                                   : 0u,
      .pEnabledValidationFeatures = config_.enableValidation ? validationFeaturesEnabled : nullptr,
  };

  const VkApplicationInfo appInfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = nullptr,
      .pApplicationName = "IGL/Vulkan",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "IGL/Vulkan",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_3,
  };

  const VkInstanceCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = config_.enableValidation ? &features : nullptr,
      .flags = 0,
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = config_.enableValidation ? (uint32_t)LVK_ARRAY_NUM_ELEMENTS(kDefaultValidationLayers) : 0,
      .ppEnabledLayerNames = config_.enableValidation ? kDefaultValidationLayers : nullptr,
      .enabledExtensionCount = numInstanceExtensions,
      .ppEnabledExtensionNames = instanceExtensionNames,
  };
  VK_ASSERT(vkCreateInstance(&ci, nullptr, &vkInstance_));

  volkLoadInstance(vkInstance_);

  // debug messenger
  {
    const VkDebugUtilsMessengerCreateInfoEXT ci = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = &vulkanDebugCallback,
        .pUserData = this,
    };
    VK_ASSERT(vkCreateDebugUtilsMessengerEXT(vkInstance_, &ci, nullptr, &vkDebugUtilsMessenger_));
  }

  uint32_t count = 0;
  VK_ASSERT(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));

  std::vector<VkExtensionProperties> allInstanceExtensions(count);

  VK_ASSERT(vkEnumerateInstanceExtensionProperties(nullptr, &count, allInstanceExtensions.data()));

  // log available instance extensions
  LLOGL("\nVulkan instance extensions:\n");

  for (const auto& extension : allInstanceExtensions) {
    LLOGL("  %s\n", extension.extensionName);
  }
}

void VulkanContext::createSurface(void* window, void* display) {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
  const VkWin32SurfaceCreateInfoKHR ci = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .hinstance = GetModuleHandle(nullptr),
      .hwnd = (HWND)window,
  };
  VK_ASSERT(vkCreateWin32SurfaceKHR(vkInstance_, &ci, nullptr, &vkSurface_));
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
  const VkXlibSurfaceCreateInfoKHR ci = {
      .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
      .flags = 0,
      .dpy = (Display*)display,
      .window = (Window)window,
  };
  VK_ASSERT(vkCreateXlibSurfaceKHR(vkInstance_, &ci, nullptr, &vkSurface_));
#else
#error Implement for other platforms
#endif
}

lvk::Result VulkanContext::queryDevices(HWDeviceType deviceType,
                                        std::vector<HWDeviceDesc>& outDevices) {
  outDevices.clear();

  // Physical devices
  uint32_t deviceCount = 0;
  VK_ASSERT_RETURN(vkEnumeratePhysicalDevices(vkInstance_, &deviceCount, nullptr));
  std::vector<VkPhysicalDevice> vkDevices(deviceCount);
  VK_ASSERT_RETURN(vkEnumeratePhysicalDevices(vkInstance_, &deviceCount, vkDevices.data()));

  auto convertVulkanDeviceTypeToIGL = [](VkPhysicalDeviceType vkDeviceType) -> HWDeviceType {
    switch (vkDeviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      return HWDeviceType_Integrated;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      return HWDeviceType_Discrete;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      return HWDeviceType_External;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      return HWDeviceType_Software;
    default:
      return HWDeviceType_Software;
    }
  };

  const HWDeviceType desiredDeviceType = deviceType;

  for (uint32_t i = 0; i < deviceCount; ++i) {
    VkPhysicalDevice physicalDevice = vkDevices[i];
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

    const HWDeviceType deviceType = convertVulkanDeviceTypeToIGL(deviceProperties.deviceType);

    // filter non-suitable hardware devices
    if (desiredDeviceType != HWDeviceType_Software && deviceType != desiredDeviceType) {
      continue;
    }

    outDevices.push_back({.guid = (uintptr_t)vkDevices[i], .type = deviceType});
    strncpy(
        outDevices.back().name, deviceProperties.deviceName, strlen(deviceProperties.deviceName));
  }

  if (outDevices.empty()) {
    return Result(Result::Code::RuntimeError, "No Vulkan devices matching your criteria");
  }

  return Result();
}

lvk::Result VulkanContext::initContext(const HWDeviceDesc& desc) {
  if (desc.guid == 0UL) {
    LLOGW("Invalid hardwareGuid(%lu)", desc.guid);
    return Result(Result::Code::RuntimeError, "Vulkan is not supported");
  }

  vkPhysicalDevice_ = (VkPhysicalDevice)desc.guid;

  useStaging_ = !isHostVisibleSingleHeapMemory(vkPhysicalDevice_);

  vkGetPhysicalDeviceFeatures2(vkPhysicalDevice_, &vkFeatures10_);
  vkGetPhysicalDeviceProperties2(vkPhysicalDevice_, &vkPhysicalDeviceProperties2_);

  const uint32_t apiVersion = vkPhysicalDeviceProperties2_.properties.apiVersion;

  LLOGL("Vulkan physical device: %s\n", vkPhysicalDeviceProperties2_.properties.deviceName);
  LLOGL("           API version: %i.%i.%i.%i\n",
        VK_API_VERSION_MAJOR(apiVersion),
        VK_API_VERSION_MINOR(apiVersion),
        VK_API_VERSION_PATCH(apiVersion),
        VK_API_VERSION_VARIANT(apiVersion));
  LLOGL("           Driver info: %s %s\n", vkPhysicalDeviceDriverProperties_.driverName, vkPhysicalDeviceDriverProperties_.driverInfo);

  uint32_t count = 0;
  VK_ASSERT(vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_, nullptr, &count, nullptr));

  std::vector<VkExtensionProperties> allPhysicalDeviceExtensions(count);

  VK_ASSERT(vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_, nullptr, &count, allPhysicalDeviceExtensions.data()));

  LLOGL("Vulkan physical device extensions:\n");

  // log available physical device extensions
  for (const auto& ext : allPhysicalDeviceExtensions) {
    LLOGL("  %s\n", ext.extensionName);
  }

  deviceQueues_.graphicsQueueFamilyIndex = lvk::findQueueFamilyIndex(vkPhysicalDevice_, VK_QUEUE_GRAPHICS_BIT);
  deviceQueues_.computeQueueFamilyIndex = lvk::findQueueFamilyIndex(vkPhysicalDevice_, VK_QUEUE_COMPUTE_BIT);

  if (deviceQueues_.graphicsQueueFamilyIndex == DeviceQueues::INVALID) {
    LLOGW("VK_QUEUE_GRAPHICS_BIT is not supported");
    return Result(Result::Code::RuntimeError, "VK_QUEUE_GRAPHICS_BIT is not supported");
  }

  if (deviceQueues_.computeQueueFamilyIndex == DeviceQueues::INVALID) {
    LLOGW("VK_QUEUE_COMPUTE_BIT is not supported");
    return Result(Result::Code::RuntimeError, "VK_QUEUE_COMPUTE_BIT is not supported");
  }

  const float queuePriority = 1.0f;

  const VkDeviceQueueCreateInfo ciQueue[2] = {
      {
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = deviceQueues_.graphicsQueueFamilyIndex,
          .queueCount = 1,
          .pQueuePriorities = &queuePriority,
      },
      {
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = deviceQueues_.computeQueueFamilyIndex,
          .queueCount = 1,
          .pQueuePriorities = &queuePriority,
      },
  };
  const uint32_t numQueues = ciQueue[0].queueFamilyIndex == ciQueue[1].queueFamilyIndex ? 1 : 2;

  const char* deviceExtensionNames[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#if defined(LVK_WITH_TRACY)
    VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
#endif
  };

  VkPhysicalDeviceFeatures deviceFeatures10 = {
      .geometryShader = VK_TRUE,
      .multiDrawIndirect = VK_TRUE,
      .drawIndirectFirstInstance = VK_TRUE,
      .depthBiasClamp = VK_TRUE,
      .fillModeNonSolid = VK_TRUE,
      .textureCompressionBC = VK_TRUE,
  };
  VkPhysicalDeviceVulkan11Features deviceFeatures11 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
      .storageBuffer16BitAccess = VK_TRUE,
      .shaderDrawParameters = VK_TRUE,
  };
  VkPhysicalDeviceVulkan12Features deviceFeatures12 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = &deviceFeatures11,
      .descriptorIndexing = VK_TRUE,
      .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
      .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
      .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
      .descriptorBindingUpdateUnusedWhilePending = VK_TRUE,
      .descriptorBindingPartiallyBound = VK_TRUE,
      .descriptorBindingVariableDescriptorCount = VK_TRUE,
      .runtimeDescriptorArray = VK_TRUE,
      .uniformBufferStandardLayout = VK_TRUE,
      .timelineSemaphore = VK_TRUE,
      .bufferDeviceAddress = VK_TRUE,
  };
  VkPhysicalDeviceVulkan13Features deviceFeatures13 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .pNext = &deviceFeatures12,
      .subgroupSizeControl = VK_TRUE,
      .synchronization2 = VK_TRUE,
      .dynamicRendering = VK_TRUE,
      .maintenance4 = VK_TRUE,
  };
  const VkDeviceCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &deviceFeatures13,
      .queueCreateInfoCount = numQueues,
      .pQueueCreateInfos = ciQueue,
      .enabledLayerCount = (uint32_t)LVK_ARRAY_NUM_ELEMENTS(kDefaultValidationLayers),
      .ppEnabledLayerNames = kDefaultValidationLayers,
      .enabledExtensionCount = (uint32_t)LVK_ARRAY_NUM_ELEMENTS(deviceExtensionNames),
      .ppEnabledExtensionNames = deviceExtensionNames,
      .pEnabledFeatures = &deviceFeatures10,
  };

  // check extensions
  {
    std::vector<VkExtensionProperties> props;
    getDeviceExtensionProps(vkPhysicalDevice_, props);
    for (const char* layer : kDefaultValidationLayers) {
      getDeviceExtensionProps(vkPhysicalDevice_, props, layer);
    }
    std::string missingExtensions;
    for (const char* ext : deviceExtensionNames) {
      if (!hasExtension(ext, props))
        missingExtensions += "\n   " + std::string(ext);
    }
    if (!missingExtensions.empty()) {
      MINILOG_LOG_PROC(minilog::FatalError, "Missing Vulkan device extensions: %s\n", missingExtensions.c_str());
      assert(false);
      return Result(Result::Code::RuntimeError);
    }
  }

  // check features
  {
    std::string missingFeatures;
#define CHECK_VULKAN_FEATURE(reqFeatures, availFeatures, feature, version)     \
  if ((reqFeatures.feature == VK_TRUE) && (availFeatures.feature == VK_FALSE)) \
    missingFeatures.append("\n   " version " ." #feature);
#define CHECK_FEATURE_1_0(feature) CHECK_VULKAN_FEATURE(deviceFeatures10, vkFeatures10_.features, feature, "1.0 ");
    CHECK_FEATURE_1_0(robustBufferAccess);
    CHECK_FEATURE_1_0(fullDrawIndexUint32);
    CHECK_FEATURE_1_0(imageCubeArray);
    CHECK_FEATURE_1_0(independentBlend);
    CHECK_FEATURE_1_0(geometryShader);
    CHECK_FEATURE_1_0(tessellationShader);
    CHECK_FEATURE_1_0(sampleRateShading);
    CHECK_FEATURE_1_0(dualSrcBlend);
    CHECK_FEATURE_1_0(logicOp);
    CHECK_FEATURE_1_0(multiDrawIndirect);
    CHECK_FEATURE_1_0(drawIndirectFirstInstance);
    CHECK_FEATURE_1_0(depthClamp);
    CHECK_FEATURE_1_0(depthBiasClamp);
    CHECK_FEATURE_1_0(fillModeNonSolid);
    CHECK_FEATURE_1_0(depthBounds);
    CHECK_FEATURE_1_0(wideLines);
    CHECK_FEATURE_1_0(largePoints);
    CHECK_FEATURE_1_0(alphaToOne);
    CHECK_FEATURE_1_0(multiViewport);
    CHECK_FEATURE_1_0(samplerAnisotropy);
    CHECK_FEATURE_1_0(textureCompressionETC2);
    CHECK_FEATURE_1_0(textureCompressionASTC_LDR);
    CHECK_FEATURE_1_0(textureCompressionBC);
    CHECK_FEATURE_1_0(occlusionQueryPrecise);
    CHECK_FEATURE_1_0(pipelineStatisticsQuery);
    CHECK_FEATURE_1_0(vertexPipelineStoresAndAtomics);
    CHECK_FEATURE_1_0(fragmentStoresAndAtomics);
    CHECK_FEATURE_1_0(shaderTessellationAndGeometryPointSize);
    CHECK_FEATURE_1_0(shaderImageGatherExtended);
    CHECK_FEATURE_1_0(shaderStorageImageExtendedFormats);
    CHECK_FEATURE_1_0(shaderStorageImageMultisample);
    CHECK_FEATURE_1_0(shaderStorageImageReadWithoutFormat);
    CHECK_FEATURE_1_0(shaderStorageImageWriteWithoutFormat);
    CHECK_FEATURE_1_0(shaderUniformBufferArrayDynamicIndexing);
    CHECK_FEATURE_1_0(shaderSampledImageArrayDynamicIndexing);
    CHECK_FEATURE_1_0(shaderStorageBufferArrayDynamicIndexing);
    CHECK_FEATURE_1_0(shaderStorageImageArrayDynamicIndexing);
    CHECK_FEATURE_1_0(shaderClipDistance);
    CHECK_FEATURE_1_0(shaderCullDistance);
    CHECK_FEATURE_1_0(shaderFloat64);
    CHECK_FEATURE_1_0(shaderInt64);
    CHECK_FEATURE_1_0(shaderInt16);
    CHECK_FEATURE_1_0(shaderResourceResidency);
    CHECK_FEATURE_1_0(shaderResourceMinLod);
    CHECK_FEATURE_1_0(sparseBinding);
    CHECK_FEATURE_1_0(sparseResidencyBuffer);
    CHECK_FEATURE_1_0(sparseResidencyImage2D);
    CHECK_FEATURE_1_0(sparseResidencyImage3D);
    CHECK_FEATURE_1_0(sparseResidency2Samples);
    CHECK_FEATURE_1_0(sparseResidency4Samples);
    CHECK_FEATURE_1_0(sparseResidency8Samples);
    CHECK_FEATURE_1_0(sparseResidency16Samples);
    CHECK_FEATURE_1_0(sparseResidencyAliased);
    CHECK_FEATURE_1_0(variableMultisampleRate);
    CHECK_FEATURE_1_0(inheritedQueries);
#undef CHECK_FEATURE_1_0
#define CHECK_FEATURE_1_1(feature) CHECK_VULKAN_FEATURE(deviceFeatures11, vkFeatures11_, feature, "1.1 ");
    CHECK_FEATURE_1_1(storageBuffer16BitAccess);
    CHECK_FEATURE_1_1(uniformAndStorageBuffer16BitAccess);
    CHECK_FEATURE_1_1(storagePushConstant16);
    CHECK_FEATURE_1_1(storageInputOutput16);
    CHECK_FEATURE_1_1(multiview);
    CHECK_FEATURE_1_1(multiviewGeometryShader);
    CHECK_FEATURE_1_1(multiviewTessellationShader);
    CHECK_FEATURE_1_1(variablePointersStorageBuffer);
    CHECK_FEATURE_1_1(variablePointers);
    CHECK_FEATURE_1_1(protectedMemory);
    CHECK_FEATURE_1_1(samplerYcbcrConversion);
    CHECK_FEATURE_1_1(shaderDrawParameters);
#undef CHECK_FEATURE_1_1
#define CHECK_FEATURE_1_2(feature) CHECK_VULKAN_FEATURE(deviceFeatures12, vkFeatures12_, feature, "1.2 ");
    CHECK_FEATURE_1_2(samplerMirrorClampToEdge);
    CHECK_FEATURE_1_2(drawIndirectCount);
    CHECK_FEATURE_1_2(storageBuffer8BitAccess);
    CHECK_FEATURE_1_2(uniformAndStorageBuffer8BitAccess);
    CHECK_FEATURE_1_2(storagePushConstant8);
    CHECK_FEATURE_1_2(shaderBufferInt64Atomics);
    CHECK_FEATURE_1_2(shaderSharedInt64Atomics);
    CHECK_FEATURE_1_2(shaderFloat16);
    CHECK_FEATURE_1_2(shaderInt8);
    CHECK_FEATURE_1_2(descriptorIndexing);
    CHECK_FEATURE_1_2(shaderInputAttachmentArrayDynamicIndexing);
    CHECK_FEATURE_1_2(shaderUniformTexelBufferArrayDynamicIndexing);
    CHECK_FEATURE_1_2(shaderStorageTexelBufferArrayDynamicIndexing);
    CHECK_FEATURE_1_2(shaderUniformBufferArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderSampledImageArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderStorageBufferArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderStorageImageArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderInputAttachmentArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderUniformTexelBufferArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderStorageTexelBufferArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(descriptorBindingUniformBufferUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingSampledImageUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingStorageImageUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingStorageBufferUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingUniformTexelBufferUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingStorageTexelBufferUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingUpdateUnusedWhilePending);
    CHECK_FEATURE_1_2(descriptorBindingPartiallyBound);
    CHECK_FEATURE_1_2(descriptorBindingVariableDescriptorCount);
    CHECK_FEATURE_1_2(runtimeDescriptorArray);
    CHECK_FEATURE_1_2(samplerFilterMinmax);
    CHECK_FEATURE_1_2(scalarBlockLayout);
    CHECK_FEATURE_1_2(imagelessFramebuffer);
    CHECK_FEATURE_1_2(uniformBufferStandardLayout);
    CHECK_FEATURE_1_2(shaderSubgroupExtendedTypes);
    CHECK_FEATURE_1_2(separateDepthStencilLayouts);
    CHECK_FEATURE_1_2(hostQueryReset);
    CHECK_FEATURE_1_2(timelineSemaphore);
    CHECK_FEATURE_1_2(bufferDeviceAddress);
    CHECK_FEATURE_1_2(bufferDeviceAddressCaptureReplay);
    CHECK_FEATURE_1_2(bufferDeviceAddressMultiDevice);
    CHECK_FEATURE_1_2(vulkanMemoryModel);
    CHECK_FEATURE_1_2(vulkanMemoryModelDeviceScope);
    CHECK_FEATURE_1_2(vulkanMemoryModelAvailabilityVisibilityChains);
    CHECK_FEATURE_1_2(shaderOutputViewportIndex);
    CHECK_FEATURE_1_2(shaderOutputLayer);
    CHECK_FEATURE_1_2(subgroupBroadcastDynamicId);
#undef CHECK_FEATURE_1_2
#define CHECK_FEATURE_1_3(feature) CHECK_VULKAN_FEATURE(deviceFeatures13, vkFeatures13_, feature, "1.3 ");
    CHECK_FEATURE_1_3(robustImageAccess);
    CHECK_FEATURE_1_3(inlineUniformBlock);
    CHECK_FEATURE_1_3(descriptorBindingInlineUniformBlockUpdateAfterBind);
    CHECK_FEATURE_1_3(pipelineCreationCacheControl);
    CHECK_FEATURE_1_3(privateData);
    CHECK_FEATURE_1_3(shaderDemoteToHelperInvocation);
    CHECK_FEATURE_1_3(shaderTerminateInvocation);
    CHECK_FEATURE_1_3(subgroupSizeControl);
    CHECK_FEATURE_1_3(computeFullSubgroups);
    CHECK_FEATURE_1_3(synchronization2);
    CHECK_FEATURE_1_3(textureCompressionASTC_HDR);
    CHECK_FEATURE_1_3(shaderZeroInitializeWorkgroupMemory);
    CHECK_FEATURE_1_3(dynamicRendering);
    CHECK_FEATURE_1_3(shaderIntegerDotProduct);
    CHECK_FEATURE_1_3(maintenance4);
#undef CHECK_FEATURE_1_3
    if (!missingFeatures.empty()) {
      MINILOG_LOG_PROC(minilog::FatalError, "Missing Vulkan features: %s\n", missingFeatures.c_str());
      assert(false);
      return Result(Result::Code::RuntimeError);
    }
  }

  VK_ASSERT_RETURN(vkCreateDevice(vkPhysicalDevice_, &ci, nullptr, &vkDevice_));

  volkLoadDevice(vkDevice_);

  vkGetDeviceQueue(vkDevice_, deviceQueues_.graphicsQueueFamilyIndex, 0, &deviceQueues_.graphicsQueue);
  vkGetDeviceQueue(vkDevice_, deviceQueues_.computeQueueFamilyIndex, 0, &deviceQueues_.computeQueue);

  VK_ASSERT(lvk::setDebugObjectName(vkDevice_, VK_OBJECT_TYPE_DEVICE, (uint64_t)vkDevice_, "Device: VulkanContext::vkDevice_"));

  immediate_ =
      std::make_unique<lvk::VulkanImmediateCommands>(vkDevice_, deviceQueues_.graphicsQueueFamilyIndex, "VulkanContext::immediate_");

  // create Vulkan pipeline cache
  {
    const VkPipelineCacheCreateInfo ci = {
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        nullptr,
        VkPipelineCacheCreateFlags(0),
        config_.pipelineCacheDataSize,
        config_.pipelineCacheData,
    };
    vkCreatePipelineCache(vkDevice_, &ci, nullptr, &pipelineCache_);
  }

  if (LVK_VULKAN_USE_VMA) {
    pimpl_->vma_ = lvk::createVmaAllocator(vkPhysicalDevice_, vkDevice_, vkInstance_, apiVersion);
    LVK_ASSERT(pimpl_->vma_ != VK_NULL_HANDLE);
  }

  stagingDevice_ = std::make_unique<lvk::vulkan::VulkanStagingDevice>(*this);

  // default texture
  {
    const VkFormat dummyTextureFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const VkMemoryPropertyFlags memFlags = useStaging_ ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    Result result;
    auto image = createImage(VK_IMAGE_TYPE_2D,
                             VkExtent3D{1, 1, 1},
                             dummyTextureFormat,
                             1,
                             1,
                             VK_IMAGE_TILING_OPTIMAL,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                             memFlags,
                             0,
                             VK_SAMPLE_COUNT_1_BIT,
                             &result,
                             "Image: dummy 1x1");
    if (!LVK_VERIFY(result.isOk())) {
      return result;
    }
    if (!LVK_VERIFY(image.get())) {
      return Result(Result::Code::RuntimeError, "Cannot create VulkanImage");
    }
    VkImageView imageView = image->createImageView(
        VK_IMAGE_VIEW_TYPE_2D, dummyTextureFormat, VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1, "Image View: dummy 1x1");
    if (!LVK_VERIFY(imageView != VK_NULL_HANDLE)) {
      return Result(Result::Code::RuntimeError, "Cannot create VkImageView");
    }
    VulkanTexture dummyTexture(std::move(image), imageView);
    const uint32_t pixel = 0xFF000000;
    const void* data[] = {&pixel};
    const VkRect2D imageRegion = {.offset = {.x = 0, .y = 0}, .extent = {.width = 1, .height = 1}};
    stagingDevice_->imageData2D(*dummyTexture.image_.get(), imageRegion, 0, 1, 0, 1, dummyTextureFormat, data);
    texturesPool_.create(std::move(dummyTexture));
    LVK_ASSERT(texturesPool_.numObjects() == 1);
  }

  // default sampler
  LVK_ASSERT(samplersPool_.numObjects() == 0);
  createSampler(
      {
          .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .magFilter = VK_FILTER_LINEAR,
          .minFilter = VK_FILTER_LINEAR,
          .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
          .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
          .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
          .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
          .mipLodBias = 0.0f,
          .anisotropyEnable = VK_FALSE,
          .maxAnisotropy = 0.0f,
          .compareEnable = VK_FALSE,
          .compareOp = VK_COMPARE_OP_ALWAYS,
          .minLod = 0.0f,
          .maxLod = 0.0f,
          .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
          .unnormalizedCoordinates = VK_FALSE,
      },
      nullptr,
      "Sampler: default");

  if (!LVK_VERIFY(config_.maxSamplers <= vkPhysicalDeviceVulkan12Properties_.maxDescriptorSetUpdateAfterBindSamplers)) {
    LLOGW("Max Samplers exceeded %u (max %u)",
          config_.maxSamplers,
          vkPhysicalDeviceVulkan12Properties_.maxDescriptorSetUpdateAfterBindSamplers);
  }

  if (!LVK_VERIFY(config_.maxTextures <= vkPhysicalDeviceVulkan12Properties_.maxDescriptorSetUpdateAfterBindSampledImages)) {
    LLOGW("Max Textures exceeded: %u (max %u)",
          config_.maxTextures,
          vkPhysicalDeviceVulkan12Properties_.maxDescriptorSetUpdateAfterBindSampledImages);
  }

  const VkPhysicalDeviceLimits& limits = getVkPhysicalDeviceProperties().limits;

  {
    // create default descriptor set layout which is going to be shared by graphics pipelines
    constexpr uint32_t numBindings = 3;
    const VkDescriptorSetLayoutBinding bindings[numBindings] = {
        lvk::getDSLBinding(kBinding_Textures, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, config_.maxTextures),
        lvk::getDSLBinding(kBinding_Samplers, VK_DESCRIPTOR_TYPE_SAMPLER, config_.maxSamplers),
        lvk::getDSLBinding(kBinding_StorageImages, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, config_.maxTextures),
    };
    const uint32_t flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
                           VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    const VkDescriptorBindingFlags bindingFlags[numBindings] = {flags, flags, flags};
    const VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlagsCI = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
        .bindingCount = numBindings,
        .pBindingFlags = bindingFlags,
    };
    const VkDescriptorSetLayoutCreateInfo dslci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &setLayoutBindingFlagsCI,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
        .bindingCount = numBindings,
        .pBindings = bindings,
    };
    VK_ASSERT(vkCreateDescriptorSetLayout(vkDevice_, &dslci, nullptr, &vkDSLBindless_));
    VK_ASSERT(lvk::setDebugObjectName(
        vkDevice_, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)vkDSLBindless_, "Descriptor Set Layout: VulkanContext::vkDSLBindless_"));

    // create default descriptor pool and allocate 1 descriptor set
    const uint32_t numSets = 1;
    LVK_ASSERT(numSets > 0);
    const VkDescriptorPoolSize poolSizes[numBindings]{
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, numSets * config_.maxTextures},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, numSets * config_.maxSamplers},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, numSets * config_.maxTextures},
    };
    bindlessDSets_.resize(numSets);
    const VkDescriptorPoolCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = numSets,
        .poolSizeCount = numBindings,
        .pPoolSizes = poolSizes,
    };
    VK_ASSERT_RETURN(vkCreateDescriptorPool(vkDevice_, &ci, nullptr, &vkDPBindless_));
    for (size_t i = 0; i != numSets; i++) {
      const VkDescriptorSetAllocateInfo ai = {
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
          .descriptorPool = vkDPBindless_,
          .descriptorSetCount = 1,
          .pSetLayouts = &vkDSLBindless_,
      };
      VK_ASSERT_RETURN(vkAllocateDescriptorSets(vkDevice_, &ai, &bindlessDSets_[i].ds));
    }
  }

  // maxPushConstantsSize is guaranteed to be at least 128 bytes
  // https://www.khronos.org/registry/vulkan/specs/1.3/html/vkspec.html#features-limits
  // Table 32. Required Limits
  const uint32_t kPushConstantsSize = 128;
  if (!LVK_VERIFY(kPushConstantsSize <= limits.maxPushConstantsSize)) {
    LLOGW("Push constants size exceeded %u (max %u bytes)", kPushConstantsSize, limits.maxPushConstantsSize);
  }

  // create pipeline layout
  {
    const VkPushConstantRange range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = kPushConstantsSize,
    };
    const VkPipelineLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &vkDSLBindless_,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &range,
    };
    VK_ASSERT(vkCreatePipelineLayout(vkDevice_, &ci, nullptr, &vkPipelineLayout_));
    VK_ASSERT(lvk::setDebugObjectName(
        vkDevice_, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)vkPipelineLayout_, "Pipeline Layout: VulkanContext::pipelineLayout_"));
  }

  querySurfaceCapabilities();

  return Result();
}

lvk::Result VulkanContext::initSwapchain(uint32_t width, uint32_t height) {
  if (!vkDevice_ || !immediate_) {
    LLOGW("Call initContext() first");
    return Result(Result::Code::RuntimeError, "Call initContext() first");
  }

  if (swapchain_) {
    // destroy the old swapchain first
    VK_ASSERT(vkDeviceWaitIdle(vkDevice_));
    swapchain_ = nullptr;
  }

  if (!width || !height) {
    return Result();
  }

  swapchain_ = std::make_unique<lvk::VulkanSwapchain>(*this, width, height);

  return swapchain_ ? Result() : Result(Result::Code::RuntimeError, "Failed to create swapchain");
}

BufferHandle VulkanContext::createBuffer(VkDeviceSize bufferSize,
                                         VkBufferUsageFlags usageFlags,
                                         VkMemoryPropertyFlags memFlags,
                                         lvk::Result* outResult,
                                         const char* debugName) {
#define ENSURE_BUFFER_SIZE(flag, maxSize)                                                  \
  if (usageFlags & flag) {                                                                 \
    if (!LVK_VERIFY(bufferSize <= maxSize)) {                                              \
      Result::setResult(outResult,                                                         \
                        Result(Result::Code::RuntimeError, "Buffer size exceeded" #flag)); \
      return {};                                                                           \
    }                                                                                      \
  }

  const VkPhysicalDeviceLimits& limits = getVkPhysicalDeviceProperties().limits;

  ENSURE_BUFFER_SIZE(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, limits.maxUniformBufferRange);
  // any buffer
  ENSURE_BUFFER_SIZE(VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM, limits.maxStorageBufferRange);
#undef ENSURE_BUFFER_SIZE

  return buffersPool_.create(
      VulkanBuffer(this, vkDevice_, bufferSize, usageFlags, memFlags, debugName));
}

std::shared_ptr<VulkanImage> VulkanContext::createImage(VkImageType imageType,
                                                        VkExtent3D extent,
                                                        VkFormat format,
                                                        uint32_t numLevels,
                                                        uint32_t numLayers,
                                                        VkImageTiling tiling,
                                                        VkImageUsageFlags usageFlags,
                                                        VkMemoryPropertyFlags memFlags,
                                                        VkImageCreateFlags flags,
                                                        VkSampleCountFlagBits samples,
                                                        lvk::Result* outResult,
                                                        const char* debugName) {
  if (!validateImageLimits(imageType, samples, extent, getVkPhysicalDeviceProperties().limits, outResult)) {
    return nullptr;
  }

  return std::make_shared<VulkanImage>(
      *this, vkDevice_, extent, imageType, format, numLevels, numLayers, tiling, usageFlags, memFlags, flags, samples, debugName);
}

void VulkanContext::bindDefaultDescriptorSets(VkCommandBuffer cmdBuf,
                                              VkPipelineBindPoint bindPoint) const {
  LVK_PROFILER_FUNCTION();

  vkCmdBindDescriptorSets(cmdBuf,
                          bindPoint,
                          vkPipelineLayout_,
                          0,
                          1,
                          &bindlessDSets_[currentDSetIndex_].ds,
                          0,
                          nullptr);
}

void VulkanContext::checkAndUpdateDescriptorSets() const {
  if (!awaitingCreation_) {
    // nothing to update here
    return;
  }

  // newly created resources can be used immediately - make sure they are put into descriptor sets
  LVK_PROFILER_FUNCTION();

  // update Vulkan descriptor set here

  // make sure the guard values are always there
  LVK_ASSERT(texturesPool_.numObjects() >= 1);
  LVK_ASSERT(samplersPool_.numObjects() >= 1);

  // 1. Sampled and storage images
  std::vector<VkDescriptorImageInfo> infoSampledImages;
  std::vector<VkDescriptorImageInfo> infoStorageImages;

  infoSampledImages.reserve(texturesPool_.numObjects());
  infoStorageImages.reserve(texturesPool_.numObjects());

  // use the dummy texture to avoid sparse array
  VkImageView dummyImageView = texturesPool_.objects_[0].obj_.imageView_;

  for (const auto& obj : texturesPool_.objects_) {
    const VulkanTexture& texture = obj.obj_;
    // multisampled images cannot be directly accessed from shaders
    const bool isTextureAvailable = (texture.image_->vkSamples_ & VK_SAMPLE_COUNT_1_BIT) == VK_SAMPLE_COUNT_1_BIT;
    const bool isSampledImage = isTextureAvailable && texture.image_->isSampledImage();
    const bool isStorageImage = isTextureAvailable && texture.image_->isStorageImage();
    infoSampledImages.push_back(
        {samplersPool_.objects_[0].obj_, isSampledImage ? texture.imageView_ : dummyImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    LVK_ASSERT(infoSampledImages.back().imageView != VK_NULL_HANDLE);
    infoStorageImages.push_back(
        VkDescriptorImageInfo{VK_NULL_HANDLE, isStorageImage ? texture.imageView_ : dummyImageView, VK_IMAGE_LAYOUT_GENERAL});
  }

  // 2. Samplers
  std::vector<VkDescriptorImageInfo> infoSamplers;
  infoSamplers.reserve(samplersPool_.objects_.size());

  for (const auto& sampler : samplersPool_.objects_) {
    infoSamplers.push_back({sampler.obj_ ? sampler.obj_ : samplersPool_.objects_[0].obj_, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED});
  }

  VkWriteDescriptorSet write[kBinding_NumBindins] = {};
  uint32_t numBindings = 0;

  // we want to update the next available descriptor set
  const uint32_t nextDSetIndex = (currentDSetIndex_ + 1) % bindlessDSets_.size();
  auto& dsetToUpdate = bindlessDSets_[nextDSetIndex];

  if (!infoSampledImages.empty()) {
    write[numBindings++] = VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = dsetToUpdate.ds,
        .dstBinding = kBinding_Textures,
        .dstArrayElement = 0,
        .descriptorCount = (uint32_t)infoSampledImages.size(),
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = infoSampledImages.data(),
    };
  }

  if (!infoSamplers.empty()) {
    write[numBindings++] = VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = dsetToUpdate.ds,
        .dstBinding = kBinding_Samplers,
        .dstArrayElement = 0,
        .descriptorCount = (uint32_t)infoSamplers.size(),
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = infoSamplers.data(),
    };
  }

  if (!infoStorageImages.empty()) {
    write[numBindings++] = VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = dsetToUpdate.ds,
        .dstBinding = kBinding_StorageImages,
        .dstArrayElement = 0,
        .descriptorCount = (uint32_t)infoStorageImages.size(),
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = infoStorageImages.data(),
    };
  }

  // do not switch to the next descriptor set if there is nothing to update
  if (numBindings) {
#if LVK_VULKAN_PRINT_COMMANDS
    LLOGL("Updating descriptor set %u\n", nextDSetIndex);
#endif // LVK_VULKAN_PRINT_COMMANDS
    currentDSetIndex_ = nextDSetIndex;
    immediate_->wait(std::exchange(dsetToUpdate.handle, immediate_->getLastSubmitHandle()));
    vkUpdateDescriptorSets(vkDevice_, numBindings, write, 0, nullptr);
  }

  awaitingCreation_ = false;
}

SamplerHandle VulkanContext::createSampler(const VkSamplerCreateInfo& ci, lvk::Result* outResult, const char* debugName) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_CREATE);

  VkSampler sampler = VK_NULL_HANDLE;

  VK_ASSERT(vkCreateSampler(vkDevice_, &ci, nullptr, &sampler));
  VK_ASSERT(lvk::setDebugObjectName(vkDevice_, VK_OBJECT_TYPE_SAMPLER, (uint64_t)sampler, debugName));

  SamplerHandle handle = samplersPool_.create(VkSampler(sampler));

  LVK_ASSERT(samplersPool_.numObjects() <= config_.maxSamplers);

  awaitingCreation_ = true;

  return handle;
}

void VulkanContext::querySurfaceCapabilities() {
  // enumerate only the formats we are using
  const VkFormat depthFormats[] = {
      VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM};
  for (const auto& depthFormat : depthFormats) {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(vkPhysicalDevice_, depthFormat, &formatProps);

    if (formatProps.optimalTilingFeatures) {
      deviceDepthFormats_.push_back(depthFormat);
    }
  }

  if (vkSurface_ == VK_NULL_HANDLE) {
    return;
  }

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_, vkSurface_, &deviceSurfaceCaps_);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_, vkSurface_, &formatCount, nullptr);

  if (formatCount) {
    deviceSurfaceFormats_.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_, vkSurface_, &formatCount, deviceSurfaceFormats_.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_, vkSurface_, &presentModeCount, nullptr);

  if (presentModeCount) {
    devicePresentModes_.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_, vkSurface_, &presentModeCount, devicePresentModes_.data());
  }
}

VkFormat VulkanContext::getClosestDepthStencilFormat(lvk::Format desiredFormat) const {
  // get a list of compatible depth formats for a given desired format
  // The list will contain depth format that are ordered from most to least closest
  const std::vector<VkFormat> compatibleDepthStencilFormatList =
      getCompatibleDepthStencilFormats(desiredFormat);

  // Generate a set of device supported formats
  std::set<VkFormat> availableFormats;
  for (auto format : deviceDepthFormats_) {
    availableFormats.insert(format);
  }

  // check if any of the format in compatible list is supported
  for (auto depthStencilFormat : compatibleDepthStencilFormatList) {
    if (availableFormats.count(depthStencilFormat) != 0) {
      return depthStencilFormat;
    }
  }

  // no matching found, choose the first supported format
  return !deviceDepthFormats_.empty() ? deviceDepthFormats_[0] : VK_FORMAT_D24_UNORM_S8_UINT;
}

std::vector<uint8_t> VulkanContext::getPipelineCacheData() const {
  size_t size = 0;
  vkGetPipelineCacheData(vkDevice_, pipelineCache_, &size, nullptr);

  std::vector<uint8_t> data(size);

  if (size) {
    vkGetPipelineCacheData(vkDevice_, pipelineCache_, &size, data.data());
  }

  return data;
}

void VulkanContext::deferredTask(std::packaged_task<void()>&& task, SubmitHandle handle) const {
  if (handle.empty()) {
    handle = immediate_->getLastSubmitHandle();
  }
  deferredTasks_.emplace_back(std::move(task), handle);
}

void* VulkanContext::getVmaAllocator() const {
  return pimpl_->vma_;
}

void VulkanContext::processDeferredTasks() const {
  while (!deferredTasks_.empty() && immediate_->isReady(deferredTasks_.front().handle_, true)) {
    deferredTasks_.front().task_();
    deferredTasks_.pop_front();
  }
}

void VulkanContext::waitDeferredTasks() {
  for (auto& task : deferredTasks_) {
    immediate_->wait(task.handle_);
    task.task_();
  }
  deferredTasks_.clear();
}

} // namespace vulkan
} // namespace lvk
