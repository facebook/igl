/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <array>
#include <cstring>
#include <set>
#include <vector>

#include <igl/IGLSafeC.h>

// For vk_mem_alloc.h, define this before including VulkanContext.h in exactly
// one CPP file
#define VMA_IMPLEMENTATION

// For volk.h, define this before including volk.h in exactly one CPP file.
// @fb-only
#if defined(IGL_CMAKE_BUILD)
#define VOLK_IMPLEMENTATION
#endif // IGL_CMAKE_BUILD

#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/EnhancedShaderDebuggingStore.h>
#include <igl/vulkan/SyncManager.h>
#include <igl/vulkan/VulkanBuffer.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDescriptorSetLayout.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanExtensions.h>
#include <igl/vulkan/VulkanPipelineBuilder.h>
#include <igl/vulkan/VulkanPipelineLayout.h>
#include <igl/vulkan/VulkanSampler.h>
#include <igl/vulkan/VulkanSemaphore.h>
#include <igl/vulkan/VulkanSwapchain.h>
#include <igl/vulkan/VulkanTexture.h>
#include <igl/vulkan/VulkanVma.h>

#if IGL_PLATFORM_MACOS
#include <dlfcn.h>
#endif

namespace {

/*
 * Descriptor sets:
 *  0 - combined image samplers
 *  1 - uniform buffers
 *  2 - storage buffers
 *  3 - bindless textures/samplers  <--  optional
 */
const uint32_t kBindPoint_CombinedImageSamplers = 0;
const uint32_t kBindPoint_BuffersUniform = 1;
const uint32_t kBindPoint_BuffersStorage = 2;
const uint32_t kBindPoint_Bindless = 3;

/*
 BINDLESS ONLY: these bindings should match GLSL declarations injected into shaders in
 Device::compileShaderModule(). Same with SparkSL.
 */
const uint32_t kBinding_Texture2D = 0;
const uint32_t kBinding_Texture2DArray = 1;
const uint32_t kBinding_Texture3D = 2;
const uint32_t kBinding_TextureCube = 3;
const uint32_t kBinding_Sampler = 4;
const uint32_t kBinding_SamplerShadow = 5;
const uint32_t kBinding_StorageImages = 6;

#if defined(VK_EXT_debug_utils) && IGL_PLATFORM_WIN
VKAPI_ATTR VkBool32 VKAPI_CALL
vulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity,
                    [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT msgType,
                    const VkDebugUtilsMessengerCallbackDataEXT* cbData,
                    void* userData) {
  if (msgSeverity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    return VK_FALSE;
  }

  const bool isError = (msgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0;

  igl::vulkan::VulkanContext* ctx = static_cast<igl::vulkan::VulkanContext*>(userData);

#if IGL_DEBUG || defined(IGL_FORCE_ENABLE_LOGS)
  std::array<char, 128> errorName = {};
  int object = 0;
  void* handle = nullptr;
  std::array<char, 128> typeName = {};
  void* messageID = nullptr;

  if (sscanf(cbData->pMessage,
             "Validation Error : [ %127s ] Object %i: handle = %p, type = %127s | MessageID = %p",
             errorName.data(),
             &object,
             &handle,
             typeName.data(),
             &messageID) >= 2) {
    const char* message = strrchr(cbData->pMessage, '|') + 1;
    IGL_LOG_INFO(
        "%sValidation layer:\n Validation Error: %s \n Object %i: handle = %p, type = %s\n "
        "MessageID = %p \n%s \n",
        isError ? "\nERROR:\n" : "",
        errorName.data(),
        object,
        handle,
        typeName.data(),
        messageID,
        message);
  } else {
    const bool isWarning = (msgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0;

    if (isError || isWarning || ctx->config_.enableExtraLogs) {
      IGL_LOG_INFO("%sValidation layer:\n%s\n", isError ? "\nERROR:\n" : "", cbData->pMessage);
    }
  }
#endif

  if (isError) {
    if (ctx->config_.terminateOnValidationError) {
      IGL_ASSERT(false);
      std::terminate();
    }
  }

  return VK_FALSE;
}
#endif // defined(VK_EXT_debug_utils) && !IGL_PLATFORM_ANDROID

std::vector<VkFormat> getCompatibleDepthStencilFormats(igl::TextureFormat format) {
  switch (format) {
  case igl::TextureFormat::Z_UNorm16:
    return {VK_FORMAT_D16_UNORM,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D32_SFLOAT};
  case igl::TextureFormat::Z_UNorm24:
    return {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM_S8_UINT};
  case igl::TextureFormat::Z_UNorm32:
    return {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
  case igl::TextureFormat::S8_UInt_Z24_UNorm:
    return {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT};
  case igl::TextureFormat::S8_UInt_Z32_UNorm:
    return {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT};
  case igl::TextureFormat::S_UInt8:
    return {VK_FORMAT_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    // default
  default:
    return {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT};
  }
}

VkQueueFlagBits getQueueTypeFlag(igl::CommandQueueType type) {
  switch (type) {
  case igl::CommandQueueType::Compute:
    return VK_QUEUE_COMPUTE_BIT;
  case igl::CommandQueueType::Graphics:
    return VK_QUEUE_GRAPHICS_BIT;
  case igl::CommandQueueType::MemoryTransfer:
    return VK_QUEUE_TRANSFER_BIT;
  }
  IGL_UNREACHABLE_RETURN(VK_QUEUE_GRAPHICS_BIT);
}

bool validateImageLimits(VkImageType imageType,
                         VkSampleCountFlagBits samples,
                         const VkExtent3D& extent,
                         const VkPhysicalDeviceLimits& limits,
                         igl::Result* outResult) {
  using igl::Result;

  if (samples != VK_SAMPLE_COUNT_1_BIT && !IGL_VERIFY(imageType == VK_IMAGE_TYPE_2D)) {
    Result::setResult(
        outResult,
        Result(Result::Code::InvalidOperation, "Multisampling is supported only for 2D images"));
    return false;
  }

  if (imageType == VK_IMAGE_TYPE_1D && !IGL_VERIFY(extent.width <= limits.maxImageDimension1D)) {
    Result::setResult(outResult,
                      Result(Result::Code::InvalidOperation, "1D texture size exceeded"));
    return false;
  } else if (imageType == VK_IMAGE_TYPE_2D &&
             !IGL_VERIFY(extent.width <= limits.maxImageDimension2D &&
                         extent.height <= limits.maxImageDimension2D)) {
    Result::setResult(outResult,
                      Result(Result::Code::InvalidOperation, "2D texture size exceeded"));
    return false;
  } else if (imageType == VK_IMAGE_TYPE_3D &&
             !IGL_VERIFY(extent.width <= limits.maxImageDimension3D &&
                         extent.height <= limits.maxImageDimension3D &&
                         extent.depth <= limits.maxImageDimension3D)) {
    Result::setResult(outResult,
                      Result(Result::Code::InvalidOperation, "3D texture size exceeded"));
    return false;
  }

  Result::setOk(outResult);

  return true;
}

} // namespace

namespace igl {
namespace vulkan {

struct VulkanContextImpl final {
  // Vulkan Memory Allocator
  VmaAllocator vma_ = VK_NULL_HANDLE;
};

VulkanContext::VulkanContext(const VulkanContextConfig& config,
                             void* window,
                             size_t numExtraInstanceExtensions,
                             const char** extraInstanceExtensions,
                             void* display) :
  config_(config) {
  IGL_PROFILER_THREAD("MainThread");

  pimpl_ = std::make_unique<VulkanContextImpl>();

  if (volkInitialize() != VK_SUCCESS) {
    IGL_LOG_ERROR("volkInitialize() failed\n");
    exit(255);
  };

  glslang_initialize_process();

  createInstance(numExtraInstanceExtensions, extraInstanceExtensions);

  if (window) {
    createSurface(window, display);
  }
}

VulkanContext::~VulkanContext() {
  IGL_PROFILER_FUNCTION();

  if (device_) {
    waitIdle();
  }

  enhancedShaderDebuggingStore_.reset(nullptr);

  dummyStorageBuffer_.reset();
  dummyUniformBuffer_.reset();
  textures_.clear();
  samplers_.clear();

  // This will free an internal buffer that was allocated by VMA
  stagingDevice_.reset(nullptr);

  VkDevice device = device_ ? device_->getVkDevice() : VK_NULL_HANDLE;
  if (device_) {
    for (auto r : renderPasses_) {
      vkDestroyRenderPass(device, r, nullptr);
    }
  }

  dslCombinedImageSamplers_.reset(nullptr);
  dslBuffersUniform_.reset(nullptr);
  dslBuffersStorage_.reset(nullptr);
  dslBindless_.reset(nullptr);

  pipelineLayoutGraphics_.reset(nullptr);
  pipelineLayoutCompute_.reset(nullptr);
  swapchain_.reset(nullptr); // Swapchain has to be destroyed prior to Surface

  waitDeferredTasks();

  immediate_.reset(nullptr);

  if (device_) {
    if (dpBindless_ != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(device, dpBindless_, nullptr);
    }
    vkDestroyDescriptorPool(device, dpCombinedImageSamplers_, nullptr);
    vkDestroyDescriptorPool(device, dpBuffersUniform_, nullptr);
    vkDestroyDescriptorPool(device, dpBuffersStorage_, nullptr);
    vkDestroyPipelineCache(device, pipelineCache_, nullptr);
  }

  vkDestroySurfaceKHR(vkInstance_, vkSurface_, nullptr);

  // Clean up VMA
  if (IGL_VULKAN_USE_VMA) {
    vmaDestroyAllocator(pimpl_->vma_);
  }

  device_.reset(nullptr); // Device has to be destroyed prior to Instance
#if defined(VK_EXT_debug_utils) && !IGL_PLATFORM_ANDROID
  vkDestroyDebugUtilsMessengerEXT(vkInstance_, vkDebugUtilsMessenger_, nullptr);
#endif // defined(VK_EXT_debug_utils) && !IGL_PLATFORM_ANDROID
  vkDestroyInstance(vkInstance_, nullptr);

  glslang_finalize_process();

#if IGL_DEBUG || defined(IGL_FORCE_ENABLE_LOGS)
  if (config_.enableExtraLogs) {
    IGL_LOG_INFO("Vulkan graphics pipelines created: %u\n",
                 VulkanPipelineBuilder::getNumPipelinesCreated());
    IGL_LOG_INFO("Vulkan compute pipelines created: %u\n",
                 VulkanComputePipelineBuilder::getNumPipelinesCreated());
  }
#endif // IGL_DEBUG || defined(IGL_FORCE_ENABLE_LOGS)
}

void VulkanContext::createInstance(const size_t numExtraExtensions, const char** extraExtensions) {
  // Enumerate all instance extensions
  extensions_.enumerate();
  extensions_.enableCommonExtensions(VulkanExtensions::ExtensionType::Instance,
                                     config_.enableValidation);
  for (size_t index = 0; index < numExtraExtensions; ++index) {
    extensions_.enable(extraExtensions[index], VulkanExtensions::ExtensionType::Instance);
  }

  auto instanceExtensions = extensions_.allEnabled(VulkanExtensions::ExtensionType::Instance);

  vkInstance_ = VK_NULL_HANDLE;
  VK_ASSERT(ivkCreateInstance(VK_API_VERSION_1_1,
                              config_.enableValidation,
                              config_.enableGPUAssistedValidation,
                              config_.enableSynchronizationValidation,
                              instanceExtensions.size(),
                              instanceExtensions.data(),
                              &vkInstance_));

  volkLoadInstance(vkInstance_);

#if defined(VK_EXT_debug_utils) && IGL_PLATFORM_WIN
  if (extensions_.enabled(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    VK_ASSERT(ivkCreateDebugUtilsMessenger(
        vkInstance_, &vulkanDebugCallback, this, &vkDebugUtilsMessenger_));
  }
#endif // if defined(VK_EXT_debug_utils) && IGL_PLATFORM_WIN

#if IGL_DEBUG || defined(IGL_FORCE_ENABLE_LOGS)
  if (config_.enableExtraLogs) {
    // log available instance extensions
    IGL_LOG_INFO("Vulkan instance extensions:\n");
    for (const auto& extension :
         extensions_.allAvailableExtensions(VulkanExtensions::ExtensionType::Instance)) {
      IGL_LOG_INFO("  %s\n", extension.c_str());
    }
  }
#endif
}

void VulkanContext::createSurface(void* window, void* display) {
  VK_ASSERT(ivkCreateSurface(vkInstance_, window, display, &vkSurface_));
}

igl::Result VulkanContext::queryDevices(const HWDeviceQueryDesc& desc,
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
      return HWDeviceType::IntegratedGpu;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      return HWDeviceType::DiscreteGpu;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      return HWDeviceType::ExternalGpu;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      return HWDeviceType::SoftwareGpu;
    default:
      return HWDeviceType::Unknown;
    }
  };

  const HWDeviceType desiredDeviceType = desc.hardwareType;

  for (uint32_t i = 0; i < deviceCount; ++i) {
    VkPhysicalDevice physicalDevice = vkDevices[i];
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

    const HWDeviceType deviceType = convertVulkanDeviceTypeToIGL(deviceProperties.deviceType);

    // filter non-suitable hardware devices
    if (desiredDeviceType != HWDeviceType::Unknown && deviceType != desiredDeviceType) {
      continue;
    }

    outDevices.emplace_back((uintptr_t)vkDevices[i],
                            deviceType,
                            deviceProperties.vendorID,
                            deviceProperties.deviceName,
                            std::to_string(deviceProperties.vendorID));
  }

  if (outDevices.empty()) {
    return Result(Result::Code::Unsupported, "No Vulkan devices matching your criteria");
  }

  return Result();
}

igl::Result VulkanContext::initContext(const HWDeviceDesc& desc,
                                       size_t numExtraDeviceExtensions,
                                       const char** extraDeviceExtensions) {
  if (desc.guid == 0UL) {
    IGL_LOG_ERROR("Invalid hardwareGuid(%lu)", desc.guid);
    return Result(Result::Code::Unsupported, "Vulkan is not supported");
  }

  vkPhysicalDevice_ = (VkPhysicalDevice)desc.guid;

  useStaging_ = !ivkIsHostVisibleSingleHeapMemory(vkPhysicalDevice_);

  vkGetPhysicalDeviceFeatures2(vkPhysicalDevice_, &vkPhysicalDeviceFeatures2_);
  vkGetPhysicalDeviceProperties2(vkPhysicalDevice_, &vkPhysicalDeviceProperties2_);

  const uint32_t apiVersion = vkPhysicalDeviceProperties2_.properties.apiVersion;

  if (config_.enableExtraLogs) {
    IGL_LOG_INFO("Vulkan physical device: %s\n",
                 vkPhysicalDeviceProperties2_.properties.deviceName);
    IGL_LOG_INFO("           API version: %i.%i.%i.%i\n",
                 VK_API_VERSION_MAJOR(apiVersion),
                 VK_API_VERSION_MINOR(apiVersion),
                 VK_API_VERSION_PATCH(apiVersion),
                 VK_API_VERSION_VARIANT(apiVersion));
    IGL_LOG_INFO("           Driver info: %s %s\n",
                 vkPhysicalDeviceDriverProperties_.driverName,
                 vkPhysicalDeviceDriverProperties_.driverInfo);
  }

  extensions_.enumerate(vkPhysicalDevice_);

#if IGL_DEBUG || defined(IGL_FORCE_ENABLE_LOGS)
  if (config_.enableExtraLogs) {
    IGL_LOG_INFO("Vulkan physical device extensions:\n");
    // log available physical device extensions
    for (const auto& extension :
         extensions_.allAvailableExtensions(VulkanExtensions::ExtensionType::Device)) {
      IGL_LOG_INFO("  %s\n", extension.c_str());
    }
  }
#endif

  extensions_.enableCommonExtensions(VulkanExtensions::ExtensionType::Device);
  // Enable extra device extensions
  for (size_t i = 0; i < numExtraDeviceExtensions; i++) {
    extensions_.enable(extraDeviceExtensions[i], VulkanExtensions::ExtensionType::Device);
  }
  if (config_.enableBufferDeviceAddress) {
    (void)IGL_VERIFY(extensions_.enable(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
                                        VulkanExtensions::ExtensionType::Device));
  }

  VulkanQueuePool queuePool(vkPhysicalDevice_);

  // Reserve IGL Vulkan queues
  auto graphicsQueueDescriptor = queuePool.findQueueDescriptor(VK_QUEUE_GRAPHICS_BIT);
  auto computeQueueDescriptor = queuePool.findQueueDescriptor(VK_QUEUE_COMPUTE_BIT);

  if (!graphicsQueueDescriptor.isValid()) {
    IGL_LOG_ERROR("VK_QUEUE_GRAPHICS_BIT is not supported");
    return Result(Result::Code::Unsupported, "VK_QUEUE_GRAPHICS_BIT is not supported");
  }

  if (!computeQueueDescriptor.isValid()) {
    IGL_LOG_ERROR("VK_QUEUE_COMPUTE_BIT is not supported");
    return Result(Result::Code::Unsupported, "VK_QUEUE_COMPUTE_BIT is not supported");
  }

  deviceQueues_.graphicsQueueFamilyIndex = graphicsQueueDescriptor.familyIndex;
  deviceQueues_.computeQueueFamilyIndex = computeQueueDescriptor.familyIndex;

  queuePool.reserveQueue(graphicsQueueDescriptor);
  queuePool.reserveQueue(computeQueueDescriptor);

  // Reserve queues requested by user
  // Reserve transfer types at the end, since those can fallback to compute and graphics queues.
  // This reduces the risk of failing reservation due to saturation of compute and graphics queues
  auto sortedUserQueues = config_.userQueues;
  sort(sortedUserQueues.begin(), sortedUserQueues.end(), [](const auto& /*q1*/, const auto& q2) {
    return q2 == CommandQueueType::MemoryTransfer;
  });

  for (const auto& userQueue : sortedUserQueues) {
    auto userQueueDescriptor = queuePool.findQueueDescriptor(getQueueTypeFlag(userQueue));
    if (userQueueDescriptor.isValid()) {
      userQueues_[userQueue] = userQueueDescriptor;
    } else {
      IGL_LOG_ERROR("User requested queue is not supported");
      return Result(Result::Code::Unsupported, "User requested queue is not supported");
    }
  }

  for (const auto& [_, descriptor] : userQueues_) {
    queuePool.reserveQueue(descriptor);
  }

  const auto qcis = queuePool.getQueueCreationInfos();

  VkDevice device;
  VK_ASSERT_RETURN(
      ivkCreateDevice(vkPhysicalDevice_,
                      qcis.size(),
                      qcis.data(),
                      extensions_.allEnabled(VulkanExtensions::ExtensionType::Device).size(),
                      extensions_.allEnabled(VulkanExtensions::ExtensionType::Device).data(),
                      vkPhysicalDeviceMultiviewFeatures_.multiview,
                      vkPhysicalDeviceShaderFloat16Int8Features_.shaderFloat16,
                      config_.enableBufferDeviceAddress,
                      config_.enableDescriptorIndexing,
                      &device));
  if (!config_.enableConcurrentVkDevicesSupport) {
    volkLoadDevice(device);
  }

  if (config_.enableBufferDeviceAddress && vkGetBufferDeviceAddressKHR == nullptr) {
    return Result(Result::Code::InvalidOperation, "Cannot initialize VK_KHR_buffer_device_address");
  }

  vkGetDeviceQueue(device, deviceQueues_.graphicsQueueFamilyIndex, 0, &deviceQueues_.graphicsQueue);
  vkGetDeviceQueue(device, deviceQueues_.computeQueueFamilyIndex, 0, &deviceQueues_.computeQueue);

  device_ = std::make_unique<igl::vulkan::VulkanDevice>(device, "Device: VulkanContext::device_");
  immediate_ = std::make_unique<igl::vulkan::VulkanImmediateCommands>(
      device, deviceQueues_.graphicsQueueFamilyIndex, "VulkanContext::immediate_");
  syncManager_ = std::make_unique<SyncManager>(*this, config_.maxResourceCount);

  // create Vulkan pipeline cache
  {
    const VkPipelineCacheCreateInfo ci = {
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        nullptr,
        VkPipelineCacheCreateFlags(0),
        config_.pipelineCacheDataSize,
        config_.pipelineCacheData,
    };
    vkCreatePipelineCache(device, &ci, nullptr, &pipelineCache_);
  }

  // Create Vulkan Memory Allocator
  if (IGL_VULKAN_USE_VMA) {
    VK_ASSERT_RETURN(ivkVmaCreateAllocator(vkPhysicalDevice_,
                                           device_->getVkDevice(),
                                           vkInstance_,
                                           apiVersion,
                                           config_.enableBufferDeviceAddress,
                                           &pimpl_->vma_));
  }

  // The staging device will use VMA to allocate a buffer, so this needs
  // to happen after VMA has been initialized.
  stagingDevice_ = std::make_unique<igl::vulkan::VulkanStagingDevice>(*this);

  // Unextended Vulkan 1.1 does not allow sparse (VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT)
  // bindings. Our descriptor set layout emulates OpenGL binding slots but we cannot put
  // VK_NULL_HANDLE into empty slots. We use dummy buffers to stick them into those empty slots.
  dummyUniformBuffer_ = createBuffer(256,
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                     nullptr,
                                     "Buffer: dummy uniform");
  dummyStorageBuffer_ = createBuffer(256,
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                     nullptr,
                                     "Buffer: dummy storage");

  // default texture
  IGL_ASSERT(textures_.size() == 1);
  {
    const VkFormat dummyTextureFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const VkMemoryPropertyFlags memFlags = useStaging_ ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                                       : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    Result result;
    auto image = createImage(VK_IMAGE_TYPE_2D,
                             VkExtent3D{1, 1, 1},
                             dummyTextureFormat,
                             1,
                             1,
                             VK_IMAGE_TILING_OPTIMAL,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                 VK_IMAGE_USAGE_STORAGE_BIT,
                             memFlags,
                             0,
                             VK_SAMPLE_COUNT_1_BIT,
                             &result,
                             "Image: dummy 1x1");
    if (!IGL_VERIFY(result.isOk())) {
      return result;
    }
    if (!IGL_VERIFY(image)) {
      return Result(Result::Code::InvalidOperation, "Cannot create VulkanImage");
    }
    auto imageView = image->createImageView(VK_IMAGE_VIEW_TYPE_2D,
                                            dummyTextureFormat,
                                            VK_IMAGE_ASPECT_COLOR_BIT,
                                            0,
                                            VK_REMAINING_MIP_LEVELS,
                                            0,
                                            1,
                                            "Image View: dummy 1x1");
    if (!IGL_VERIFY(imageView)) {
      return Result(Result::Code::InvalidOperation, "Cannot create VulkanImageView");
    }
    textures_[0] = std::make_shared<VulkanTexture>(*this, std::move(image), std::move(imageView));
    const uint32_t pixel = 0xFF000000;
    const VkRect2D imageRegion = ivkGetRect2D(0, 0, 1, 1);
    stagingDevice_->imageData2D(
        textures_[0]->getVulkanImage(),
        imageRegion,
        0,
        1,
        0,
        TextureFormatProperties::fromTextureFormat(vkFormatToTextureFormat(dummyTextureFormat)),
        dummyTextureFormat,
        &pixel);
  }

  // default sampler
  IGL_ASSERT(samplers_.size() == 1);
  samplers_[0] =
      std::make_shared<VulkanSampler>(*this,
                                      device,
                                      ivkGetSamplerCreateInfo(VK_FILTER_LINEAR,
                                                              VK_FILTER_LINEAR,
                                                              VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                                              VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                                              VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                                              VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                                              0.0f,
                                                              0.0f),
                                      "Sampler: default");

  if (config_.enableDescriptorIndexing) {
    if (!IGL_VERIFY(config_.maxSamplers <= vkPhysicalDeviceDescriptorIndexingProperties_
                                               .maxDescriptorSetUpdateAfterBindSamplers)) {
      IGL_LOG_ERROR(
          "Max Samplers exceeded %u (max %u)",
          config_.maxSamplers,
          vkPhysicalDeviceDescriptorIndexingProperties_.maxDescriptorSetUpdateAfterBindSamplers);
    }

    if (!IGL_VERIFY(config_.maxTextures <= vkPhysicalDeviceDescriptorIndexingProperties_
                                               .maxDescriptorSetUpdateAfterBindSampledImages)) {
      IGL_LOG_ERROR("Max Textures exceeded: %u (max %u)",
                    config_.maxTextures,
                    vkPhysicalDeviceDescriptorIndexingProperties_
                        .maxDescriptorSetUpdateAfterBindSampledImages);
    }
  }

  const VkPhysicalDeviceLimits& limits = getVkPhysicalDeviceProperties().limits;

  // TODO: make this more manageable (dynamic) once we migrate all apps to use descriptor sets
  constexpr uint32_t kNumSets = 1024;
  {
    // create default descriptor set layout for texture bindings
    constexpr uint32_t kNumBindings = IGL_TEXTURE_SAMPLERS_MAX;
    // NOTE: we really want these arrays to be uninitialized
    // @lint-ignore CLANGTIDY
    VkDescriptorSetLayoutBinding bindings[kNumBindings];
    // @lint-ignore CLANGTIDY
    VkDescriptorBindingFlags bindingFlags[kNumBindings];
    // @lint-ignore CLANGTIDY
    VkDescriptorPoolSize poolSizes[kNumBindings];
    for (uint32_t i = 0; i != kNumBindings; i++) {
      bindings[i] =
          ivkGetDescriptorSetLayoutBinding(i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
      bindingFlags[i] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
      poolSizes[i] =
          VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kNumSets * kNumBindings};
    }
    dslCombinedImageSamplers_ = std::make_unique<VulkanDescriptorSetLayout>(
        device,
        kNumBindings,
        bindings,
        bindingFlags,
        "Descriptor Set Layout: VulkanContext::dslCombinedImageSamplers_");
    // create default descriptor pool and allocate descriptor sets
    combinedImageSamplerDSets_.dsets.resize(kNumSets);
    VK_ASSERT_RETURN(ivkCreateDescriptorPool(
        device, kNumSets, kNumBindings, poolSizes, &dpCombinedImageSamplers_));
    for (uint32_t i = 0; i != kNumSets; i++) {
      VK_ASSERT_RETURN(
          ivkAllocateDescriptorSet(device,
                                   dpCombinedImageSamplers_,
                                   dslCombinedImageSamplers_->getVkDescriptorSetLayout(),
                                   &combinedImageSamplerDSets_.dsets[i].ds));
    }
  }
  {
    // create default descriptor set layout for uniform buffers
    constexpr uint32_t kNumBindings = IGL_UNIFORM_BLOCKS_BINDING_MAX;
    // NOTE: we really want these arrays to be uninitialized
    // @lint-ignore CLANGTIDY
    VkDescriptorSetLayoutBinding bindings[kNumBindings];
    // @lint-ignore CLANGTIDY
    VkDescriptorBindingFlags bindingFlags[kNumBindings];
    // @lint-ignore CLANGTIDY
    VkDescriptorPoolSize poolSizes[kNumBindings];
    for (uint32_t i = 0; i != kNumBindings; i++) {
      bindings[i] = ivkGetDescriptorSetLayoutBinding(i, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
      bindingFlags[i] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
      poolSizes[i] = VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                          kNumSets * IGL_UNIFORM_BLOCKS_BINDING_MAX};
    }
    dslBuffersUniform_ = std::make_unique<VulkanDescriptorSetLayout>(
        device,
        kNumBindings,
        bindings,
        bindingFlags,
        "Descriptor Set Layout: VulkanContext::dslBuffersUniform_");
    VK_ASSERT_RETURN(
        ivkCreateDescriptorPool(device, kNumSets, kNumBindings, poolSizes, &dpBuffersUniform_));
    bufferUniformDSets_.dsets.resize(kNumSets);
    for (size_t i = 0; i != kNumSets; i++) {
      VK_ASSERT_RETURN(ivkAllocateDescriptorSet(device,
                                                dpBuffersUniform_,
                                                dslBuffersUniform_->getVkDescriptorSetLayout(),
                                                &bufferUniformDSets_.dsets[i].ds));
    }
  }
  {
    // create default descriptor set layout for storage buffers
    constexpr uint32_t kNumBindings = IGL_UNIFORM_BLOCKS_BINDING_MAX;
    // NOTE: we really want these arrays to be uninitialized
    // @lint-ignore CLANGTIDY
    VkDescriptorSetLayoutBinding bindings[kNumBindings];
    // @lint-ignore CLANGTIDY
    VkDescriptorBindingFlags bindingFlags[kNumBindings];
    // @lint-ignore CLANGTIDY
    VkDescriptorPoolSize poolSizes[kNumBindings];
    for (uint32_t i = 0; i != kNumBindings; i++) {
      bindings[i] = ivkGetDescriptorSetLayoutBinding(i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
      bindingFlags[i] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
      poolSizes[i] = VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                          kNumSets * IGL_UNIFORM_BLOCKS_BINDING_MAX};
    }
    dslBuffersStorage_ = std::make_unique<VulkanDescriptorSetLayout>(
        device,
        kNumBindings,
        bindings,
        bindingFlags,
        "Descriptor Set Layout: VulkanContext::dslBuffersStorage_");
    VK_ASSERT_RETURN(
        ivkCreateDescriptorPool(device, kNumSets, kNumBindings, poolSizes, &dpBuffersStorage_));
    bufferStorageDSets_.dsets.resize(kNumSets);
    for (size_t i = 0; i != kNumSets; i++) {
      VK_ASSERT_RETURN(ivkAllocateDescriptorSet(device,
                                                dpBuffersStorage_,
                                                dslBuffersStorage_->getVkDescriptorSetLayout(),
                                                &bufferStorageDSets_.dsets[i].ds));
    }
  }

  // only do allocations if actually enabled
  if (config_.enableDescriptorIndexing) {
    // create default descriptor set layout which is going to be shared by graphics pipelines
    constexpr uint32_t kNumBindings = 7;
    const std::array<VkDescriptorSetLayoutBinding, kNumBindings> bindings = {
        ivkGetDescriptorSetLayoutBinding(
            kBinding_Texture2D, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, config_.maxTextures),
        ivkGetDescriptorSetLayoutBinding(
            kBinding_Texture2DArray, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, config_.maxTextures),
        ivkGetDescriptorSetLayoutBinding(
            kBinding_Texture3D, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, config_.maxTextures),
        ivkGetDescriptorSetLayoutBinding(
            kBinding_TextureCube, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, config_.maxTextures),
        ivkGetDescriptorSetLayoutBinding(
            kBinding_Sampler, VK_DESCRIPTOR_TYPE_SAMPLER, config_.maxSamplers),
        ivkGetDescriptorSetLayoutBinding(
            kBinding_SamplerShadow, VK_DESCRIPTOR_TYPE_SAMPLER, config_.maxSamplers),
        ivkGetDescriptorSetLayoutBinding(
            kBinding_StorageImages, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, config_.maxTextures),
    };
    const uint32_t flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                           VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
                           VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    const std::array<VkDescriptorBindingFlags, kNumBindings> bindingFlags = {
        flags, flags, flags, flags, flags, flags, flags};
    dslBindless_ = std::make_unique<VulkanDescriptorSetLayout>(
        device,
        kNumBindings,
        bindings.data(),
        bindingFlags.data(),
        "Descriptor Set Layout: VulkanContext::dslBindless_");
    // create default descriptor pool and allocate 1 descriptor set
    const std::array<VkDescriptorPoolSize, kNumBindings> poolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, config_.maxTextures},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, config_.maxTextures},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, config_.maxTextures},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, config_.maxTextures},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, config_.maxSamplers},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, config_.maxSamplers},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, config_.maxTextures},
    };
    VK_ASSERT_RETURN(ivkCreateDescriptorPool(
        device, 1, static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), &dpBindless_));
    VK_ASSERT_RETURN(ivkAllocateDescriptorSet(
        device, dpBindless_, dslBindless_->getVkDescriptorSetLayout(), &bindlessDSet_.ds));
  }

  // maxPushConstantsSize is guaranteed to be at least 128 bytes
  // https://www.khronos.org/registry/vulkan/specs/1.3/html/vkspec.html#features-limits
  // Table 32. Required Limits
  const uint32_t kPushConstantsSize = 128;
  if (!IGL_VERIFY(kPushConstantsSize <= limits.maxPushConstantsSize)) {
    IGL_LOG_ERROR("Push constants size exceeded %u (max %u bytes)",
                  kPushConstantsSize,
                  limits.maxPushConstantsSize);
  }

  // @lint-ignore CLANGTIDY
  const VkDescriptorSetLayout DSLs[] = {
      dslCombinedImageSamplers_->getVkDescriptorSetLayout(),
      dslBuffersUniform_->getVkDescriptorSetLayout(),
      dslBuffersStorage_->getVkDescriptorSetLayout(),
      config_.enableDescriptorIndexing ? dslBindless_->getVkDescriptorSetLayout() : VK_NULL_HANDLE,
  };

  // create pipeline layout
  pipelineLayoutGraphics_ = std::make_unique<VulkanPipelineLayout>(
      device,
      DSLs,
      static_cast<uint32_t>(config_.enableDescriptorIndexing ? IGL_ARRAY_NUM_ELEMENTS(DSLs)
                                                             : IGL_ARRAY_NUM_ELEMENTS(DSLs) - 1),
      ivkGetPushConstantRange(
          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, kPushConstantsSize),
      "Pipeline Layout: VulkanContext::pipelineLayoutGraphics_");

  pipelineLayoutCompute_ = std::make_unique<VulkanPipelineLayout>(
      device,
      DSLs,
      static_cast<uint32_t>(config_.enableDescriptorIndexing ? IGL_ARRAY_NUM_ELEMENTS(DSLs)
                                                             : IGL_ARRAY_NUM_ELEMENTS(DSLs) - 1),
      ivkGetPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, kPushConstantsSize),
      "Pipeline Layout: VulkanContext::pipelineLayoutCompute_");

  querySurfaceCapabilities();

  // enables/disables enhanced shader debugging
  if (config_.enhancedShaderDebugging) {
    enhancedShaderDebuggingStore_ = std::make_unique<EnhancedShaderDebuggingStore>();
  }

  return Result();
}

igl::Result VulkanContext::initSwapchain(uint32_t width, uint32_t height) {
  if (!device_ || !immediate_) {
    IGL_LOG_ERROR("Call initContext() first");
    return Result(Result::Code::Unsupported, "Call initContext() first");
  }

  if (swapchain_) {
    vkDeviceWaitIdle(device_->device_);
    swapchain_ = nullptr; // Destroy old swapchain first
  }

  if (!width || !height) {
    return Result();
  }

  swapchain_ = std::make_unique<igl::vulkan::VulkanSwapchain>(*this, width, height);

  return swapchain_ ? Result() : Result(Result::Code::RuntimeError, "Failed to create Swapchain");
}

VkExtent2D VulkanContext::getSwapchainExtent() const {
  return hasSwapchain() ? swapchain_->getExtent() : VkExtent2D{0, 0};
}

Result VulkanContext::waitIdle() const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_WAIT);

  for (auto queue : {deviceQueues_.graphicsQueue, deviceQueues_.computeQueue}) {
    VK_ASSERT_RETURN(vkQueueWaitIdle(queue));
  }

  return getResultFromVkResult(VK_SUCCESS);
}

Result VulkanContext::present() const {
  if (!hasSwapchain()) {
    return Result(Result::Code::InvalidOperation, "No swapchain available");
  }

  return swapchain_->present(immediate_->acquireLastSubmitSemaphore());
}

std::shared_ptr<VulkanBuffer> VulkanContext::createBuffer(VkDeviceSize bufferSize,
                                                          VkBufferUsageFlags usageFlags,
                                                          VkMemoryPropertyFlags memFlags,
                                                          igl::Result* outResult,
                                                          const char* debugName) const {
#define ENSURE_BUFFER_SIZE(flag, maxSize)                                                      \
  if (usageFlags & flag) {                                                                     \
    if (!IGL_VERIFY(bufferSize <= maxSize)) {                                                  \
      Result::setResult(outResult,                                                             \
                        Result(Result::Code::InvalidOperation, "Buffer size exceeded" #flag)); \
      return nullptr;                                                                          \
    }                                                                                          \
  }

  const VkPhysicalDeviceLimits& limits = getVkPhysicalDeviceProperties().limits;

  ENSURE_BUFFER_SIZE(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, limits.maxUniformBufferRange);
  // any buffer
  ENSURE_BUFFER_SIZE(VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM, limits.maxStorageBufferRange);
#undef ENSURE_BUFFER_SIZE

  Result::setOk(outResult);
  return std::make_shared<VulkanBuffer>(
      *this, device_->getVkDevice(), bufferSize, usageFlags, memFlags, debugName);
}

std::shared_ptr<VulkanImage> VulkanContext::createImage(VkImageType imageType,
                                                        VkExtent3D extent,
                                                        VkFormat format,
                                                        uint32_t mipLevels,
                                                        uint32_t arrayLayers,
                                                        VkImageTiling tiling,
                                                        VkImageUsageFlags usageFlags,
                                                        VkMemoryPropertyFlags memFlags,
                                                        VkImageCreateFlags flags,
                                                        VkSampleCountFlagBits samples,
                                                        igl::Result* outResult,
                                                        const char* debugName) const {
  if (!validateImageLimits(
          imageType, samples, extent, getVkPhysicalDeviceProperties().limits, outResult)) {
    return nullptr;
  }

  return std::make_shared<VulkanImage>(*this,
                                       device_->getVkDevice(),
                                       extent,
                                       imageType,
                                       format,
                                       mipLevels,
                                       arrayLayers,
                                       tiling,
                                       usageFlags,
                                       memFlags,
                                       flags,
                                       samples,
                                       debugName);
}

std::shared_ptr<VulkanImage> VulkanContext::createImageFromFileDescriptor(
    int32_t fileDescriptor,
    uint64_t memoryAllocationSize,
    VkImageType imageType,
    VkExtent3D extent,
    VkFormat format,
    uint32_t mipLevels,
    uint32_t arrayLayers,
    VkImageTiling tiling,
    VkImageUsageFlags usageFlags,
    VkImageCreateFlags flags,
    VkSampleCountFlagBits samples,
    igl::Result* outResult,
    const char* debugName) const {
  if (!validateImageLimits(
          imageType, samples, extent, getVkPhysicalDeviceProperties().limits, outResult)) {
    return nullptr;
  }

  return std::make_shared<VulkanImage>(*this,
                                       fileDescriptor,
                                       memoryAllocationSize,
                                       device_->getVkDevice(),
                                       extent,
                                       imageType,
                                       format,
                                       mipLevels,
                                       arrayLayers,
                                       tiling,
                                       usageFlags,
                                       flags,
                                       samples,
                                       debugName);
}

void VulkanContext::checkAndUpdateDescriptorSets() const {
  if (!awaitingCreation_) {
    // nothing to update here
    return;
  }

  // newly created resources can be used immediately - make sure they are put into descriptor sets
  IGL_PROFILER_FUNCTION();

  // here we remove deleted textures - everything which has only 1 reference is owned by this
  // context and can be released safely
  for (uint32_t i = 1; i < (uint32_t)textures_.size(); i++) {
    if (textures_[i] && textures_[i].use_count() == 1) {
      if (i == textures_.size() - 1) {
        textures_.pop_back();
      } else {
        textures_[i].reset();
        freeIndicesTextures_.push_back(i);
      }
    }
  }
  for (uint32_t i = 1; i < (uint32_t)samplers_.size(); i++) {
    if (samplers_[i] && samplers_[i].use_count() == 1) {
      if (i == samplers_.size() - 1) {
        samplers_.pop_back();
      } else {
        samplers_[i].reset();
        freeIndicesSamplers_.push_back(i);
      }
    }
  }

  // update Vulkan descriptor set here
  if (!config_.enableDescriptorIndexing) {
    return;
  }

  // 1. Sampled and storage images
  std::vector<VkDescriptorImageInfo> infoSampledImages;
  std::vector<VkDescriptorImageInfo> infoStorageImages;
  IGL_ASSERT(textures_.size() >= 1); // make sure the guard value is always there
  infoSampledImages.reserve(textures_.size());
  infoStorageImages.reserve(textures_.size());

  // use the dummy texture to avoid sparse array
  VkImageView dummyImageView = textures_[0]->imageView_->getVkImageView();

  for (const auto& texture : textures_) {
    // multisampled images cannot be directly accessed from shaders
    // @lint-ignore CLANGTIDY
    const bool isTextureAvailable =
        texture && ((texture->image_->samples_ & VK_SAMPLE_COUNT_1_BIT) == VK_SAMPLE_COUNT_1_BIT);
    const bool isSampledImage = isTextureAvailable && texture->image_->isSampledImage();
    const bool isStorageImage = isTextureAvailable && texture->image_->isStorageImage();
    infoSampledImages.push_back(
        {samplers_[0]->getVkSampler(),
         isSampledImage ? texture->imageView_->getVkImageView() : dummyImageView,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    IGL_ASSERT(infoSampledImages.back().imageView != VK_NULL_HANDLE);
    infoStorageImages.push_back(VkDescriptorImageInfo{
        VK_NULL_HANDLE,
        isStorageImage ? texture->imageView_->getVkImageView() : dummyImageView,
        VK_IMAGE_LAYOUT_GENERAL});
  }

  // 2. Samplers
  std::vector<VkDescriptorImageInfo> infoSamplers;
  IGL_ASSERT(samplers_.size() >= 1); // make sure the guard value is always there
  infoSamplers.reserve(samplers_.size());

  for (const auto& sampler : samplers_) {
    infoSamplers.push_back({(sampler ? sampler : samplers_[0])->getVkSampler(),
                            VK_NULL_HANDLE,
                            VK_IMAGE_LAYOUT_UNDEFINED});
  }

  std::vector<VkWriteDescriptorSet> write;

  auto& dsetToUpdate = bindlessDSet_;

  if (!infoSampledImages.empty()) {
    // use the same indexing for every texture type
    for (uint32_t i = kBinding_Texture2D; i != kBinding_TextureCube + 1; i++) {
      write.push_back(ivkGetWriteDescriptorSet_ImageInfo(dsetToUpdate.ds,
                                                         i,
                                                         VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                         (uint32_t)infoSampledImages.size(),
                                                         infoSampledImages.data()));
    }
  };

  if (!infoSamplers.empty()) {
    for (uint32_t i = kBinding_Sampler; i != kBinding_SamplerShadow + 1; i++) {
      write.push_back(ivkGetWriteDescriptorSet_ImageInfo(dsetToUpdate.ds,
                                                         i,
                                                         VK_DESCRIPTOR_TYPE_SAMPLER,
                                                         (uint32_t)infoSamplers.size(),
                                                         infoSamplers.data()));
    }
  }

  if (!infoStorageImages.empty()) {
    write.push_back(ivkGetWriteDescriptorSet_ImageInfo(dsetToUpdate.ds,
                                                       kBinding_StorageImages,
                                                       VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                       (uint32_t)infoStorageImages.size(),
                                                       infoStorageImages.data()));
  };

  // do not switch to the next descriptor set if there is nothing to update
  if (!write.empty()) {
#if IGL_VULKAN_PRINT_COMMANDS
    IGL_LOG_INFO("Updating descriptor set bindlessDSet_\n");
#endif // IGL_VULKAN_PRINT_COMMANDS
    immediate_->wait(std::exchange(dsetToUpdate.handle, immediate_->getLastSubmitHandle()));
    vkUpdateDescriptorSets(
        device_->getVkDevice(), static_cast<uint32_t>(write.size()), write.data(), 0, nullptr);
  }

  awaitingCreation_ = false;

  lastDeletionFrame_ = getFrameNumber();
}

std::shared_ptr<VulkanTexture> VulkanContext::createTexture(
    std::shared_ptr<VulkanImage> image,
    std::shared_ptr<VulkanImageView> imageView) const {
  auto texture = std::make_shared<VulkanTexture>(*this, std::move(image), std::move(imageView));
  if (!IGL_VERIFY(texture)) {
    return nullptr;
  }
  if (!freeIndicesTextures_.empty()) {
    // reuse an empty slot
    texture->textureId_ = freeIndicesTextures_.back();
    freeIndicesTextures_.pop_back();
    textures_[texture->textureId_] = texture;
  } else {
    texture->textureId_ = uint32_t(textures_.size());
    textures_.emplace_back(texture);
  }

  if (config_.enableDescriptorIndexing) {
    const bool canFitTextures = textures_.size() <= config_.maxTextures;
    if (!IGL_VERIFY(canFitTextures)) {
      return nullptr;
    }
  }

  awaitingCreation_ = true;

  return texture;
}

std::shared_ptr<VulkanSampler> VulkanContext::createSampler(const VkSamplerCreateInfo& ci,
                                                            igl::Result* outResult,
                                                            const char* debugName) const {
  auto sampler = std::make_shared<VulkanSampler>(*this, device_->getVkDevice(), ci, debugName);
  if (!IGL_VERIFY(sampler)) {
    Result::setResult(outResult, Result::Code::InvalidOperation);
    return nullptr;
  }
  if (!freeIndicesSamplers_.empty()) {
    // reuse an empty slot
    sampler->samplerId_ = freeIndicesSamplers_.back();
    freeIndicesSamplers_.pop_back();
    samplers_[sampler->samplerId_] = sampler;
  } else {
    sampler->samplerId_ = uint32_t(samplers_.size());
    samplers_.emplace_back(sampler);
  }

  IGL_ASSERT(samplers_.size() <= config_.maxSamplers);

  awaitingCreation_ = true;

  return sampler;
}

void VulkanContext::querySurfaceCapabilities() {
  // This is not an exhaustive list. It's only formats that we are using.
  std::vector<VkFormat> depthFormats = {VK_FORMAT_D32_SFLOAT_S8_UINT,
                                        VK_FORMAT_D24_UNORM_S8_UINT,
                                        VK_FORMAT_D16_UNORM_S8_UINT,
                                        VK_FORMAT_D32_SFLOAT,
                                        VK_FORMAT_D16_UNORM,
                                        VK_FORMAT_S8_UINT};
  for (const auto& depthFormat : depthFormats) {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(vkPhysicalDevice_, depthFormat, &formatProps);

    if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ||
        formatProps.bufferFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ||
        formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      deviceDepthFormats_.push_back(depthFormat);
    }
  }

  if (vkSurface_ != VK_NULL_HANDLE) {
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_, vkSurface_, &deviceSurfaceCaps_);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_, vkSurface_, &formatCount, nullptr);

    if (formatCount) {
      deviceSurfaceFormats_.resize(formatCount);
      vkGetPhysicalDeviceSurfaceFormatsKHR(
          vkPhysicalDevice_, vkSurface_, &formatCount, deviceSurfaceFormats_.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        vkPhysicalDevice_, vkSurface_, &presentModeCount, nullptr);

    if (presentModeCount) {
      devicePresentModes_.resize(presentModeCount);
      vkGetPhysicalDeviceSurfacePresentModesKHR(
          vkPhysicalDevice_, vkSurface_, &presentModeCount, devicePresentModes_.data());
    }
  }
}

VkFormat VulkanContext::getClosestDepthStencilFormat(igl::TextureFormat desiredFormat) const {
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

VulkanContext::RenderPassHandle VulkanContext::getRenderPass(uint8_t index) const {
  return RenderPassHandle{renderPasses_[index], index};
}

VulkanContext::RenderPassHandle VulkanContext::findRenderPass(
    const VulkanRenderPassBuilder& builder) const {
  IGL_PROFILER_FUNCTION();

  auto it = renderPassesHash_.find(builder);

  if (it != renderPassesHash_.end()) {
    return RenderPassHandle{renderPasses_[it->second], it->second};
  }

  VkRenderPass pass = VK_NULL_HANDLE;
  builder.build(device_->getVkDevice(), &pass);

  const size_t index = renderPasses_.size();

  IGL_ASSERT(index <= 255);

  renderPassesHash_[builder] = uint8_t(index);
  // @fb-only
  // @lint-ignore CLANGTIDY
  renderPasses_.push_back(pass);

  return RenderPassHandle{pass, uint8_t(index)};
}

std::vector<uint8_t> VulkanContext::getPipelineCacheData() const {
  VkDevice device = device_->getVkDevice();

  size_t size = 0;
  vkGetPipelineCacheData(device, pipelineCache_, &size, nullptr);

  std::vector<uint8_t> data(size);

  if (size) {
    vkGetPipelineCacheData(device, pipelineCache_, &size, data.data());
  }

  return data;
}

uint64_t VulkanContext::getFrameNumber() const {
  return swapchain_ ? swapchain_->getFrameNumber() : 0u;
}

void VulkanContext::bindDefaultDescriptorSets(VkCommandBuffer cmdBuf,
                                              VkPipelineBindPoint bindPoint) const {
  if (!config_.enableDescriptorIndexing) {
    return;
  }

  const bool isGraphics = bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS;

#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("%p vkCmdBindDescriptorSets(%u) - bindless\n", cmdBuf, bindPoint);
#endif // IGL_VULKAN_PRINT_COMMANDS
  vkCmdBindDescriptorSets(
      cmdBuf,
      bindPoint,
      (isGraphics ? pipelineLayoutGraphics_ : pipelineLayoutCompute_)->getVkPipelineLayout(),
      kBindPoint_Bindless,
      1,
      &bindlessDSet_.ds,
      0,
      nullptr);
}

void VulkanContext::updateBindingsTextures(VkCommandBuffer cmdBuf,
                                           VkPipelineBindPoint bindPoint,
                                           const BindingsTextures& data) const {
  VkDescriptorSet dset = combinedImageSamplerDSets_.acquireNext(*immediate_);

  std::array<VkDescriptorImageInfo, IGL_TEXTURE_SAMPLERS_MAX> infoSampledImages{};
  uint32_t numImages = 0;

  // use the dummy texture to avoid sparse array
  VkImageView dummyImageView = textures_[0]->imageView_->getVkImageView();
  VkSampler dummySampler = samplers_[0]->getVkSampler();

  const bool isGraphics = bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS;

  for (size_t i = 0; i != IGL_TEXTURE_SAMPLERS_MAX; i++) {
    igl::vulkan::VulkanTexture* texture = data.textures[i];
    if (texture && isGraphics) {
      IGL_ASSERT_MSG(data.samplers[i], "A sampler should be bound to every bound texture slot");
      (void)IGL_VERIFY(data.samplers[i]);
    }
    VkSampler sampler = data.samplers[i] ? data.samplers[i]->getVkSampler() : dummySampler;
    // multisampled images cannot be directly accessed from shaders
    const bool isTextureAvailable =
        texture && ((texture->image_->samples_ & VK_SAMPLE_COUNT_1_BIT) == VK_SAMPLE_COUNT_1_BIT);
    const bool isSampledImage = isTextureAvailable && texture->image_->isSampledImage();
    infoSampledImages[numImages++] = {isSampledImage ? sampler : dummySampler,
                                      isSampledImage ? texture->imageView_->getVkImageView()
                                                     : dummyImageView,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  }

  VkWriteDescriptorSet write = ivkGetWriteDescriptorSet_ImageInfo(
      dset, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numImages, infoSampledImages.data());

  vkUpdateDescriptorSets(device_->getVkDevice(), 1, &write, 0, nullptr);

#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("%p vkCmdBindDescriptorSets(%u) - textures\n", cmdBuf, bindPoint);
#endif // IGL_VULKAN_PRINT_COMMANDS
  vkCmdBindDescriptorSets(
      cmdBuf,
      bindPoint,
      (isGraphics ? pipelineLayoutGraphics_ : pipelineLayoutCompute_)->getVkPipelineLayout(),
      kBindPoint_CombinedImageSamplers,
      1,
      &dset,
      0,
      nullptr);
}

void VulkanContext::updateBindingsUniformBuffers(VkCommandBuffer cmdBuf,
                                                 VkPipelineBindPoint bindPoint,
                                                 BindingsBuffers& data) const {
  VkDescriptorSet dsetBufUniform = bufferUniformDSets_.acquireNext(*immediate_);

  for (uint32_t i = 0; i != IGL_UNIFORM_BLOCKS_BINDING_MAX; i++) {
    VkDescriptorBufferInfo& bi = data.buffers[i];
    if (bi.buffer == VK_NULL_HANDLE) {
      bi = {dummyUniformBuffer_->getVkBuffer(), 0, VK_WHOLE_SIZE};
    }
  }

  VkWriteDescriptorSet write =
      ivkGetWriteDescriptorSet_BufferInfo(dsetBufUniform,
                                          0,
                                          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                          IGL_UNIFORM_BLOCKS_BINDING_MAX,
                                          data.buffers);

  vkUpdateDescriptorSets(device_->getVkDevice(), 1, &write, 0, nullptr);

  const bool isGraphics = bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS;

#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("%p vkCmdBindDescriptorSets(%u) - uniform buffers\n", cmdBuf, bindPoint);
#endif // IGL_VULKAN_PRINT_COMMANDS
  vkCmdBindDescriptorSets(
      cmdBuf,
      bindPoint,
      (isGraphics ? pipelineLayoutGraphics_ : pipelineLayoutCompute_)->getVkPipelineLayout(),
      kBindPoint_BuffersUniform,
      1,
      &dsetBufUniform,
      0,
      nullptr);
}

void VulkanContext::updateBindingsStorageBuffers(VkCommandBuffer cmdBuf,
                                                 VkPipelineBindPoint bindPoint,
                                                 BindingsBuffers& data) const {
  VkDescriptorSet dsetBufStorage = bufferStorageDSets_.acquireNext(*immediate_);

  for (uint32_t i = 0; i != IGL_UNIFORM_BLOCKS_BINDING_MAX; i++) {
    VkDescriptorBufferInfo& bi = data.buffers[i];
    if (bi.buffer == VK_NULL_HANDLE) {
      bi = {dummyStorageBuffer_->getVkBuffer(), 0, VK_WHOLE_SIZE};
    }
  }

  VkWriteDescriptorSet write =
      ivkGetWriteDescriptorSet_BufferInfo(dsetBufStorage,
                                          0,
                                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                          IGL_UNIFORM_BLOCKS_BINDING_MAX,
                                          data.buffers);

  vkUpdateDescriptorSets(device_->getVkDevice(), 1, &write, 0, nullptr);

  const bool isGraphics = bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS;

#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("%p vkCmdBindDescriptorSets(%u) - storage buffers\n", cmdBuf, bindPoint);
#endif // IGL_VULKAN_PRINT_COMMANDS
  vkCmdBindDescriptorSets(
      cmdBuf,
      bindPoint,
      (isGraphics ? pipelineLayoutGraphics_ : pipelineLayoutCompute_)->getVkPipelineLayout(),
      kBindPoint_BuffersStorage,
      1,
      &dsetBufStorage,
      0,
      nullptr);
}

void VulkanContext::markSubmit(const VulkanImmediateCommands::SubmitHandle& handle) const {
  if (config_.enableDescriptorIndexing) {
    bindlessDSet_.handle = handle;
  }
  combinedImageSamplerDSets_.updateHandles(handle);
  bufferUniformDSets_.updateHandles(handle);
  bufferStorageDSets_.updateHandles(handle);
}

void VulkanContext::deferredTask(std::packaged_task<void()>&& task, SubmitHandle handle) const {
  if (handle.empty()) {
    handle = immediate_->getLastSubmitHandle();
  }
  deferredTasks_.emplace_back(std::move(task), handle);
}

bool VulkanContext::areValidationLayersEnabled() const {
  return config_.enableValidation;
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
} // namespace igl
