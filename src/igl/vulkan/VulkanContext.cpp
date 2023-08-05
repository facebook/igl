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

#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanBuffer.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanPipelineBuilder.h>
#include <igl/vulkan/VulkanSwapchain.h>
#include <igl/vulkan/VulkanTexture.h>
#include <lvk/vulkan/VulkanUtils.h>

#include <glslang/Include/glslang_c_interface.h>

static_assert(lvk::HWDeviceDesc::IGL_MAX_PHYSICAL_DEVICE_NAME_SIZE <= VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);

namespace {

const char* kDefaultValidationLayers[] = {"VK_LAYER_KHRONOS_validation"};

/*
 These bindings should match GLSL declarations injected into shaders in
 Device::compileShaderModule(). Same with SparkSL.
 */
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
      IGL_ASSERT(false);
      std::terminate();
    }
  }

  return VK_FALSE;
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

  if (samples != VK_SAMPLE_COUNT_1_BIT && !IGL_VERIFY(imageType == VK_IMAGE_TYPE_2D)) {
    Result::setResult(
        outResult,
        Result(Result::Code::ArgumentOutOfRange, "Multisampling is supported only for 2D images"));
    return false;
  }

  if (imageType == VK_IMAGE_TYPE_2D && !IGL_VERIFY(extent.width <= limits.maxImageDimension2D &&
                                                   extent.height <= limits.maxImageDimension2D)) {
    Result::setResult(outResult,
                      Result(Result::Code::ArgumentOutOfRange, "2D texture size exceeded"));
    return false;
  }
  if (imageType == VK_IMAGE_TYPE_3D && !IGL_VERIFY(extent.width <= limits.maxImageDimension3D &&
                                                   extent.height <= limits.maxImageDimension3D &&
                                                   extent.depth <= limits.maxImageDimension3D)) {
    Result::setResult(outResult,
                      Result(Result::Code::ArgumentOutOfRange, "3D texture size exceeded"));
    return false;
  }

  return true;
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
};

VulkanContext::VulkanContext(const VulkanContextConfig& config,
                             void* window,
                             void* display) :
  config_(config) {
  IGL_PROFILER_THREAD("MainThread");

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
  IGL_PROFILER_FUNCTION();

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
  if (IGL_VULKAN_USE_VMA) {
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

  useStaging_ = !ivkIsHostVisibleSingleHeapMemory(vkPhysicalDevice_);

  vkGetPhysicalDeviceFeatures2(vkPhysicalDevice_, &vkFeatures10_);
  vkGetPhysicalDeviceProperties2(vkPhysicalDevice_, &vkPhysicalDeviceProperties2_);

  const uint32_t apiVersion = vkPhysicalDeviceProperties2_.properties.apiVersion;

  LLOGL("Vulkan physical device: %s\n", vkPhysicalDeviceProperties2_.properties.deviceName);
  LLOGL("           API version: %i.%i.%i.%i\n",
        VK_API_VERSION_MAJOR(apiVersion),
        VK_API_VERSION_MINOR(apiVersion),
        VK_API_VERSION_PATCH(apiVersion),
        VK_API_VERSION_VARIANT(apiVersion));
  LLOGL("           Driver info: %s %s\n",
        vkPhysicalDeviceDriverProperties_.driverName,
        vkPhysicalDeviceDriverProperties_.driverInfo);

  uint32_t count = 0;
  VK_ASSERT(vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_, nullptr, &count, nullptr));

  std::vector<VkExtensionProperties> allPhysicalDeviceExtensions(count);

  VK_ASSERT(vkEnumerateDeviceExtensionProperties(
      vkPhysicalDevice_, nullptr, &count, allPhysicalDeviceExtensions.data()));

  LLOGL("Vulkan physical device extensions:\n");

  // log available physical device extensions
  for (const auto& ext : allPhysicalDeviceExtensions) {
    LLOGL("  %s\n", ext.extensionName);
  }

  deviceQueues_.graphicsQueueFamilyIndex =
      lvk::findQueueFamilyIndex(vkPhysicalDevice_, VK_QUEUE_GRAPHICS_BIT);
  deviceQueues_.computeQueueFamilyIndex =
      lvk::findQueueFamilyIndex(vkPhysicalDevice_, VK_QUEUE_COMPUTE_BIT);

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
      MINILOG_LOG_PROC(
          minilog::FatalError, "Missing Vulkan device extensions: %s\n", missingExtensions.c_str());
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
#define CHECK_FEATURE_1_0(feature) \
  CHECK_VULKAN_FEATURE(deviceFeatures10, vkFeatures10_.features, feature, "1.0 ");
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
#define CHECK_FEATURE_1_1(feature) \
  CHECK_VULKAN_FEATURE(deviceFeatures11, vkFeatures11_, feature, "1.1 ");
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
#define CHECK_FEATURE_1_2(feature) \
  CHECK_VULKAN_FEATURE(deviceFeatures12, vkFeatures12_, feature, "1.2 ");
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
#define CHECK_FEATURE_1_3(feature) \
  CHECK_VULKAN_FEATURE(deviceFeatures13, vkFeatures13_, feature, "1.3 ");
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
      MINILOG_LOG_PROC(
          minilog::FatalError, "Missing Vulkan features: %s\n", missingFeatures.c_str());
      assert(false);
      return Result(Result::Code::RuntimeError);
    }
  }

  VK_ASSERT_RETURN(vkCreateDevice(vkPhysicalDevice_, &ci, nullptr, &vkDevice_));

  volkLoadDevice(vkDevice_);

  vkGetDeviceQueue(
      vkDevice_, deviceQueues_.graphicsQueueFamilyIndex, 0, &deviceQueues_.graphicsQueue);
  vkGetDeviceQueue(
      vkDevice_, deviceQueues_.computeQueueFamilyIndex, 0, &deviceQueues_.computeQueue);

  VK_ASSERT(ivkSetDebugObjectName(
      vkDevice_, VK_OBJECT_TYPE_DEVICE, (uint64_t)vkDevice_, "Device: VulkanContext::vkDevice_"));

  immediate_ = std::make_unique<lvk::vulkan::VulkanImmediateCommands>(
      vkDevice_, deviceQueues_.graphicsQueueFamilyIndex, "VulkanContext::immediate_");

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

  if (IGL_VULKAN_USE_VMA) {
    pimpl_->vma_ = lvk::createVmaAllocator(vkPhysicalDevice_, vkDevice_, vkInstance_, apiVersion);
    IGL_ASSERT(pimpl_->vma_ != VK_NULL_HANDLE);
  }

  stagingDevice_ = std::make_unique<lvk::vulkan::VulkanStagingDevice>(*this);

  // default texture
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
    if (!IGL_VERIFY(image.get())) {
      return Result(Result::Code::RuntimeError, "Cannot create VulkanImage");
    }
    auto imageView = image->createImageView(VK_IMAGE_VIEW_TYPE_2D,
                                            dummyTextureFormat,
                                            VK_IMAGE_ASPECT_COLOR_BIT,
                                            0,
                                            VK_REMAINING_MIP_LEVELS,
                                            0,
                                            1,
                                            "Image View: dummy 1x1");
    if (!IGL_VERIFY(imageView.get())) {
      return Result(Result::Code::RuntimeError, "Cannot create VulkanImageView");
    }
    VulkanTexture dummyTexture(std::move(image), std::move(imageView));
    const uint32_t pixel = 0xFF000000;
    const void* data[] = {&pixel};
    const VkRect2D imageRegion = ivkGetRect2D(0, 0, 1, 1);
    stagingDevice_->imageData2D(
        *dummyTexture.image_.get(), imageRegion, 0, 1, 0, 1, dummyTextureFormat, data);
    texturesPool_.create(std::move(dummyTexture));
    IGL_ASSERT(texturesPool_.numObjects() == 1);
  }

  // default sampler
  IGL_ASSERT(samplersPool_.numObjects() == 0);
  createSampler(ivkGetSamplerCreateInfo(VK_FILTER_LINEAR,
                                        VK_FILTER_LINEAR,
                                        VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                        VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                        VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                        VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                        0.0f,
                                        0.0f),
                nullptr,
                "Sampler: default");

  if (!IGL_VERIFY(config_.maxSamplers <=
                  vkPhysicalDeviceVulkan12Properties_.maxDescriptorSetUpdateAfterBindSamplers)) {
    LLOGW("Max Samplers exceeded %u (max %u)",
          config_.maxSamplers,
          vkPhysicalDeviceVulkan12Properties_.maxDescriptorSetUpdateAfterBindSamplers);
  }

  if (!IGL_VERIFY(
          config_.maxTextures <=
          vkPhysicalDeviceVulkan12Properties_.maxDescriptorSetUpdateAfterBindSampledImages)) {
    LLOGW("Max Textures exceeded: %u (max %u)",
          config_.maxTextures,
          vkPhysicalDeviceVulkan12Properties_.maxDescriptorSetUpdateAfterBindSampledImages);
  }

  const VkPhysicalDeviceLimits& limits = getVkPhysicalDeviceProperties().limits;

  {
    // create default descriptor set layout which is going to be shared by graphics pipelines
    constexpr uint32_t numBindings = 3;
    const VkDescriptorSetLayoutBinding bindings[numBindings] = {
        ivkGetDescriptorSetLayoutBinding(
            kBinding_Textures, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, config_.maxTextures),
        ivkGetDescriptorSetLayoutBinding(
            kBinding_Samplers, VK_DESCRIPTOR_TYPE_SAMPLER, config_.maxSamplers),
        ivkGetDescriptorSetLayoutBinding(
            kBinding_StorageImages, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, config_.maxTextures),
    };
    const uint32_t flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                           VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
                           VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    const VkDescriptorBindingFlags bindingFlags[numBindings] = {flags, flags, flags};
    VK_ASSERT(ivkCreateDescriptorSetLayout(
        vkDevice_, numBindings, bindings, bindingFlags, &vkDSLBindless_));
    VK_ASSERT(ivkSetDebugObjectName(vkDevice_,
                                    VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                                    (uint64_t)vkDSLBindless_,
                                    "Descriptor Set Layout: VulkanContext::dslBindless_"));

    // create default descriptor pool and allocate 1 descriptor set
    const uint32_t numSets = 1;
    IGL_ASSERT(numSets > 0);
    const VkDescriptorPoolSize poolSizes[numBindings]{
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, numSets * config_.maxTextures},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, numSets * config_.maxSamplers},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, numSets * config_.maxTextures},
    };
    bindlessDSets_.resize(numSets);
    VK_ASSERT_RETURN(
        ivkCreateDescriptorPool(vkDevice_, numSets, numBindings, poolSizes, &vkDPBindless_));
    for (size_t i = 0; i != numSets; i++) {
      VK_ASSERT_RETURN(ivkAllocateDescriptorSet(
          vkDevice_, vkDPBindless_, vkDSLBindless_, &bindlessDSets_[i].ds));
    }
  }

  // maxPushConstantsSize is guaranteed to be at least 128 bytes
  // https://www.khronos.org/registry/vulkan/specs/1.3/html/vkspec.html#features-limits
  // Table 32. Required Limits
  const uint32_t kPushConstantsSize = 128;
  if (!IGL_VERIFY(kPushConstantsSize <= limits.maxPushConstantsSize)) {
    LLOGW("Push constants size exceeded %u (max %u bytes)",
          kPushConstantsSize,
          limits.maxPushConstantsSize);
  }

  // create pipeline layout
  {
    const VkPushConstantRange range = {
        .stageFlags =
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = kPushConstantsSize,
    };
    const VkPipelineLayoutCreateInfo ci =
        ivkGetPipelineLayoutCreateInfo(1, &vkDSLBindless_, &range);

    VK_ASSERT(vkCreatePipelineLayout(vkDevice_, &ci, nullptr, &vkPipelineLayout_));
    VK_ASSERT(ivkSetDebugObjectName(vkDevice_,
                                    VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                                    (uint64_t)vkPipelineLayout_,
                                    "Pipeline Layout: VulkanContext::pipelineLayout_"));
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

  swapchain_ = std::make_unique<lvk::vulkan::VulkanSwapchain>(*this, width, height);

  return swapchain_ ? Result() : Result(Result::Code::RuntimeError, "Failed to create swapchain");
}

Result VulkanContext::present() const {
  if (!hasSwapchain()) {
    return Result(Result::Code::ArgumentOutOfRange, "No swapchain available");
  }

  return swapchain_->present(immediate_->acquireLastSubmitSemaphore());
}

BufferHandle VulkanContext::createBuffer(VkDeviceSize bufferSize,
                                         VkBufferUsageFlags usageFlags,
                                         VkMemoryPropertyFlags memFlags,
                                         lvk::Result* outResult,
                                         const char* debugName) {
#define ENSURE_BUFFER_SIZE(flag, maxSize)                                                  \
  if (usageFlags & flag) {                                                                 \
    if (!IGL_VERIFY(bufferSize <= maxSize)) {                                              \
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
                                                        uint32_t mipLevels,
                                                        uint32_t arrayLayers,
                                                        VkImageTiling tiling,
                                                        VkImageUsageFlags usageFlags,
                                                        VkMemoryPropertyFlags memFlags,
                                                        VkImageCreateFlags flags,
                                                        VkSampleCountFlagBits samples,
                                                        lvk::Result* outResult,
                                                        const char* debugName) const {
  if (!validateImageLimits(
          imageType, samples, extent, getVkPhysicalDeviceProperties().limits, outResult)) {
    return nullptr;
  }

  return std::make_shared<VulkanImage>(*this,
                                       vkDevice_,
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

void VulkanContext::bindDefaultDescriptorSets(VkCommandBuffer cmdBuf,
                                              VkPipelineBindPoint bindPoint) const {
  IGL_PROFILER_FUNCTION();

#if IGL_DEBUG_DESCRIPTOR_SETS
  IGL_LOG_INFO("Binding descriptor set %u\n", currentDSetIndex_);
#endif // IGL_DEBUG_DESCRIPTOR_SETS

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
  if (awaitingDeletion_) {
    // Our descriptor set was created with VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT which
    // indicates that descriptors in this binding that are not dynamically used need not contain
    // valid descriptors at the time the descriptors are consumed. A descriptor is dynamically used
    // if any shader invocation executes an instruction that performs any memory access using the
    // descriptor. If a descriptor is not dynamically used, any resource referenced by the
    // descriptor is not considered to be referenced during command execution.
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkDescriptorBindingFlagBits.html
    awaitingDeletion_ = false;
  }

  if (!awaitingCreation_) {
    // nothing to update here
    return;
  }

  // newly created resources can be used immediately - make sure they are put into descriptor sets
  IGL_PROFILER_FUNCTION();

  // update Vulkan descriptor set here

  // make sure the guard values are always there
  IGL_ASSERT(texturesPool_.numObjects() >= 1); 
  IGL_ASSERT(samplersPool_.numObjects() >= 1);

  // 1. Sampled and storage images
  std::vector<VkDescriptorImageInfo> infoSampledImages;
  std::vector<VkDescriptorImageInfo> infoStorageImages;
  
  infoSampledImages.reserve(texturesPool_.numObjects());
  infoStorageImages.reserve(texturesPool_.numObjects());

  // use the dummy texture to avoid sparse array
  VkImageView dummyImageView = texturesPool_.objects_[0].obj_.imageView_->getVkImageView();

  for (const auto& obj : texturesPool_.objects_) {
    const VulkanTexture& texture = obj.obj_;
    // multisampled images cannot be directly accessed from shaders
    const bool isTextureAvailable =
        (texture.image_->samples_ & VK_SAMPLE_COUNT_1_BIT) == VK_SAMPLE_COUNT_1_BIT;
    const bool isSampledImage = isTextureAvailable && texture.image_->isSampledImage();
    const bool isStorageImage = isTextureAvailable && texture.image_->isStorageImage();
    infoSampledImages.push_back(
        {samplersPool_.objects_[0].obj_,
         isSampledImage ? texture.imageView_->getVkImageView() : dummyImageView,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    IGL_ASSERT(infoSampledImages.back().imageView != VK_NULL_HANDLE);
    infoStorageImages.push_back(VkDescriptorImageInfo{
        VK_NULL_HANDLE,
        isStorageImage ? texture.imageView_->getVkImageView() : dummyImageView,
        VK_IMAGE_LAYOUT_GENERAL});
  }

  // 2. Samplers
  std::vector<VkDescriptorImageInfo> infoSamplers;
  infoSamplers.reserve(samplersPool_.objects_.size());

  for (const auto& sampler : samplersPool_.objects_) {
    infoSamplers.push_back({sampler.obj_ ? sampler.obj_ : samplersPool_.objects_[0].obj_,
                            VK_NULL_HANDLE,
                            VK_IMAGE_LAYOUT_UNDEFINED});
  }

  VkWriteDescriptorSet write[kBinding_NumBindins] = {};
  uint32_t numBindings = 0;

  // we want to update the next available descriptor set
  const uint32_t nextDSetIndex = (currentDSetIndex_ + 1) % bindlessDSets_.size();
  auto& dsetToUpdate = bindlessDSets_[nextDSetIndex];

  if (!infoSampledImages.empty()) {
    write[numBindings++] = ivkGetWriteDescriptorSet_ImageInfo(dsetToUpdate.ds,
                                                              kBinding_Textures,
                                                              VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                              (uint32_t)infoSampledImages.size(),
                                                              infoSampledImages.data());
  };

  if (!infoSamplers.empty()) {
    write[numBindings++] = ivkGetWriteDescriptorSet_ImageInfo(dsetToUpdate.ds,
                                                              kBinding_Samplers,
                                                              VK_DESCRIPTOR_TYPE_SAMPLER,
                                                              (uint32_t)infoSamplers.size(),
                                                              infoSamplers.data());
  }

  if (!infoStorageImages.empty()) {
    write[numBindings++] = ivkGetWriteDescriptorSet_ImageInfo(dsetToUpdate.ds,
                                                              kBinding_StorageImages,
                                                              VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                              (uint32_t)infoStorageImages.size(),
                                                              infoStorageImages.data());
  }

  // do not switch to the next descriptor set if there is nothing to update
  if (numBindings) {
#if IGL_VULKAN_PRINT_COMMANDS
    LLOGL("Updating descriptor set %u\n", nextDSetIndex);
#endif // IGL_VULKAN_PRINT_COMMANDS
    currentDSetIndex_ = nextDSetIndex;
    immediate_->wait(std::exchange(dsetToUpdate.handle, immediate_->getLastSubmitHandle()));
    vkUpdateDescriptorSets(vkDevice_, numBindings, write, 0, nullptr);
  }

  awaitingCreation_ = false;
  awaitingDeletion_ = false;
}

SamplerHandle VulkanContext::createSampler(const VkSamplerCreateInfo& ci,
                                           lvk::Result* outResult,
                                           const char* debugName) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  VkSampler sampler = VK_NULL_HANDLE;

  VK_ASSERT(vkCreateSampler(vkDevice_, &ci, nullptr, &sampler));
  VK_ASSERT(ivkSetDebugObjectName(vkDevice_, VK_OBJECT_TYPE_SAMPLER, (uint64_t)sampler, debugName));

  SamplerHandle handle = samplersPool_.create(VkSampler(sampler));

  IGL_ASSERT(samplersPool_.numObjects() <= config_.maxSamplers);

  awaitingCreation_ = true;

  return handle;
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

uint64_t VulkanContext::getFrameNumber() const {
  return swapchain_ ? swapchain_->getFrameNumber() : 0u;
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
} // namespace lvk
