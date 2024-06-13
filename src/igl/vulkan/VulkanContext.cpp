/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <array>
#include <cstring>
#include <memory>
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

#include <igl/glslang/GlslCompiler.h>

#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/EnhancedShaderDebuggingStore.h>
#include <igl/vulkan/SyncManager.h>
#include <igl/vulkan/VulkanBuffer.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDescriptorSetLayout.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanExtensions.h>
#include <igl/vulkan/VulkanImageView.h>
#include <igl/vulkan/VulkanPipelineBuilder.h>
#include <igl/vulkan/VulkanPipelineLayout.h>
#include <igl/vulkan/VulkanSampler.h>
#include <igl/vulkan/VulkanSemaphore.h>
#include <igl/vulkan/VulkanSwapchain.h>
#include <igl/vulkan/VulkanTexture.h>
#include <igl/vulkan/VulkanVma.h>
#include <igl/vulkan/util/SpvReflection.h>

#if IGL_PLATFORM_APPLE
#include <igl/vulkan/moltenvk/MoltenVkHelpers.h>
#endif

namespace {

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
  IGL_UNREACHABLE_RETURN(VK_QUEUE_GRAPHICS_BIT)
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

// @fb-only
class DescriptorPoolsArena final {
 public:
  DescriptorPoolsArena(const VulkanContext& ctx,
                       VkDescriptorType type,
                       VkDescriptorSetLayout dsl,
                       uint32_t numDescriptorsPerDSet,
                       const char* debugName) :
    vf_(ctx.vf_),
    device_(ctx.getVkDevice()),
    type_(type),
    numDescriptorsPerDSet_(numDescriptorsPerDSet),
    dsl_(dsl) {
    IGL_ASSERT(debugName);
    dpDebugName_ = IGL_FORMAT("Descriptor Pool: {}", debugName ? debugName : "");
    switchToNewDescriptorPool(*ctx.immediate_, {});
  }
  ~DescriptorPoolsArena() {
    // arenas are destroyed by VulkanContext after the GPU has been synchronized, so we do not have
    // to defer the destruction
    extinct_.push_back({pool_, {}});
    for (const auto& p : extinct_) {
      vf_.vkDestroyDescriptorPool(device_, p.pool_, nullptr);
    }
  }
  [[nodiscard]] VkDescriptorSetLayout getVkDescriptorSetLayout() const {
    return dsl_;
  }
  [[nodiscard]] VkDescriptorSet getNextDescriptorSet(
      VulkanImmediateCommands& ic,
      VulkanImmediateCommands::SubmitHandle lastSubmitHandle) {
    VkDescriptorSet dset = VK_NULL_HANDLE;
    if (!numRemainingDSetsInPool_) {
      switchToNewDescriptorPool(ic, lastSubmitHandle);
    }
    VK_ASSERT(ivkAllocateDescriptorSet(&vf_, device_, pool_, dsl_, &dset));
    numRemainingDSetsInPool_--;
    return dset;
  }

 private:
  void switchToNewDescriptorPool(VulkanImmediateCommands& ic,
                                 VulkanImmediateCommands::SubmitHandle lastSubmitHandle) {
    numRemainingDSetsInPool_ = kNumDSetsPerPool_;

    if (pool_ != VK_NULL_HANDLE) {
      extinct_.push_back({pool_, lastSubmitHandle});
    }
    // first, let's try to reuse the oldest extinct pool
    if (extinct_.size() > 1) {
      const ExtinctDescriptorPool p = extinct_.front();
      if (ic.isRecycled(p.handle_)) {
        pool_ = p.pool_;
        extinct_.pop_front();
        VK_ASSERT(vf_.vkResetDescriptorPool(device_, pool_, VkDescriptorPoolResetFlags{}));
        return;
      }
    }
    const VkDescriptorPoolSize poolSize = VkDescriptorPoolSize{
        type_, numDescriptorsPerDSet_ ? kNumDSetsPerPool_ * numDescriptorsPerDSet_ : 1u};
    VK_ASSERT(ivkCreateDescriptorPool(
        &vf_, device_, VkDescriptorPoolCreateFlags{}, kNumDSetsPerPool_, 1, &poolSize, &pool_));
    VK_ASSERT(ivkSetDebugObjectName(
        &vf_, device_, VK_OBJECT_TYPE_DESCRIPTOR_POOL, (uint64_t)pool_, dpDebugName_.c_str()));
  }

 private:
  static constexpr uint32_t kNumDSetsPerPool_ = 256;

  const VulkanFunctionTable& vf_;
  VkDevice device_ = VK_NULL_HANDLE;
  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  const VkDescriptorType type_ = VK_DESCRIPTOR_TYPE_MAX_ENUM;
  const uint32_t numDescriptorsPerDSet_ = 0;
  uint32_t numRemainingDSetsInPool_ = 0;
  std::string dpDebugName_;

  VkDescriptorSetLayout dsl_ = VK_NULL_HANDLE; // owned elsewhere

  struct ExtinctDescriptorPool {
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    VulkanImmediateCommands::SubmitHandle handle_ = {};
  };

  std::deque<ExtinctDescriptorPool> extinct_;
};

struct VulkanContextImpl final {
  // Vulkan Memory Allocator
  VmaAllocator vma_ = VK_NULL_HANDLE;
  // :)
  std::unordered_map<VkDescriptorSetLayout, std::unique_ptr<igl::vulkan::DescriptorPoolsArena>>
      arenaCombinedImageSamplers_;
  std::unordered_map<VkDescriptorSetLayout, std::unique_ptr<igl::vulkan::DescriptorPoolsArena>>
      arenaBuffersUniform_;
  std::unordered_map<VkDescriptorSetLayout, std::unique_ptr<igl::vulkan::DescriptorPoolsArena>>
      arenaBuffersStorage_;
  std::unique_ptr<igl::vulkan::VulkanDescriptorSetLayout> dslBindless_; // everything
  VkDescriptorPool dpBindless_ = VK_NULL_HANDLE;
  VkDescriptorSet dsBindless_ = VK_NULL_HANDLE;
  VulkanImmediateCommands::SubmitHandle lastSubmitHandle_ = {};
  uint32_t currentMaxBindlessTextures_ = 8;
  uint32_t currentMaxBindlessSamplers_ = 8;

  igl::vulkan::DescriptorPoolsArena& getOrCreateArena_CombinedImageSamplers(
      const VulkanContext& ctx,
      VkDescriptorSetLayout dsl,
      uint32_t numBindings) {
    auto it = arenaCombinedImageSamplers_.find(dsl);
    if (it != arenaCombinedImageSamplers_.end()) {
      return *it->second;
    }
    arenaCombinedImageSamplers_[dsl] =
        std::make_unique<DescriptorPoolsArena>(ctx,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               dsl,
                                               numBindings,
                                               "arenaCombinedImageSamplers_");
    return *arenaCombinedImageSamplers_[dsl].get();
  }
  igl::vulkan::DescriptorPoolsArena& getOrCreateArena_UniformBuffers(const VulkanContext& ctx,
                                                                     VkDescriptorSetLayout dsl,
                                                                     uint32_t numBindings) {
    auto it = arenaBuffersUniform_.find(dsl);
    if (it != arenaBuffersUniform_.end()) {
      return *it->second;
    }
    arenaBuffersUniform_[dsl] = std::make_unique<DescriptorPoolsArena>(
        ctx, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, dsl, numBindings, "arenaBuffersUniform_");
    return *arenaBuffersUniform_[dsl].get();
  }
  igl::vulkan::DescriptorPoolsArena& getOrCreateArena_StorageBuffers(const VulkanContext& ctx,
                                                                     VkDescriptorSetLayout dsl,
                                                                     uint32_t numBindings) {
    auto it = arenaBuffersStorage_.find(dsl);
    if (it != arenaBuffersStorage_.end()) {
      return *it->second;
    }
    arenaBuffersStorage_[dsl] = std::make_unique<DescriptorPoolsArena>(
        ctx, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, dsl, numBindings, "arenaBuffersStorage_");
    return *arenaBuffersStorage_[dsl].get();
  }
};

VulkanContext::VulkanContext(const VulkanContextConfig& config,
                             void* window,
                             size_t numExtraInstanceExtensions,
                             const char** extraInstanceExtensions,
                             void* display) :
  tableImpl_(std::make_unique<VulkanFunctionTable>()), vf_(*tableImpl_), config_(config) {
  IGL_PROFILER_THREAD("MainThread");

  pimpl_ = std::make_unique<VulkanContextImpl>();

  const auto result = volkInitialize();

  // Do not remove for backward compatibility with projects using global functions.
  if (result != VK_SUCCESS) {
    IGL_LOG_ERROR("volkInitialize() failed with error code %d\n", static_cast<int>(result));
    exit(255);
  };

  vulkan::functions::initialize(*tableImpl_);

  glslang::initializeCompiler();

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

#if defined(IGL_WITH_TRACY_GPU)
  if (tracyCtx_) {
    TracyVkDestroy(tracyCtx_);
    profilingCommandPool_.reset(nullptr);
  }
#endif

  enhancedShaderDebuggingStore_.reset(nullptr);

  dummyStorageBuffer_.reset();
  dummyUniformBuffer_.reset();
#if IGL_DEBUG
  for (const auto& t : textures_.objects_) {
    if (t.obj_.use_count() > 1) {
      IGL_ASSERT_MSG(false,
                     "Leaked texture detected! %u %s",
                     t.obj_->getTextureId(),
                     t.obj_->getVulkanImage().name_.c_str());
    }
  }
  for (const auto& s : samplers_.objects_) {
    if (s.obj_.use_count() > 1) {
      IGL_ASSERT_MSG(false,
                     "Leaked sampler detected! %u %s",
                     s.obj_->getSamplerId(),
                     s.obj_->debugName_.c_str());
    }
  }
#endif // IGL_DEBUG
  textures_.clear();
  samplers_.clear();

  // This will free an internal buffer that was allocated by VMA
  stagingDevice_.reset(nullptr);

  VkDevice device = device_ ? device_->getVkDevice() : VK_NULL_HANDLE;
  if (device_) {
    for (auto r : renderPasses_) {
      vf_.vkDestroyRenderPass(device, r, nullptr);
    }
  }

  pimpl_->dslBindless_.reset(nullptr);

  swapchain_.reset(nullptr); // Swapchain has to be destroyed prior to Surface

  waitDeferredTasks();

  immediate_.reset(nullptr);

  if (device_) {
    if (pimpl_->dpBindless_ != VK_NULL_HANDLE) {
      vf_.vkDestroyDescriptorPool(device, pimpl_->dpBindless_, nullptr);
    }
    pimpl_->arenaCombinedImageSamplers_.clear();
    pimpl_->arenaBuffersUniform_.clear();
    pimpl_->arenaBuffersStorage_.clear();
    vf_.vkDestroyPipelineCache(device, pipelineCache_, nullptr);
  }

  if (vkSurface_ != VK_NULL_HANDLE) {
    vf_.vkDestroySurfaceKHR(vkInstance_, vkSurface_, nullptr);
  }

  // Clean up VMA
  if (IGL_VULKAN_USE_VMA) {
    vmaDestroyAllocator(pimpl_->vma_);
  }

  device_.reset(nullptr); // Device has to be destroyed prior to Instance
#if defined(VK_EXT_debug_utils) && !IGL_PLATFORM_ANDROID
  vf_.vkDestroyDebugUtilsMessengerEXT(vkInstance_, vkDebugUtilsMessenger_, nullptr);
#endif // defined(VK_EXT_debug_utils) && !IGL_PLATFORM_ANDROID
  vf_.vkDestroyInstance(vkInstance_, nullptr);

  glslang::finalizeCompiler();

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
  extensions_.enumerate(vf_);
  extensions_.enableCommonExtensions(VulkanExtensions::ExtensionType::Instance, config_);
  for (size_t index = 0; index < numExtraExtensions; ++index) {
    extensions_.enable(extraExtensions[index], VulkanExtensions::ExtensionType::Instance);
  }

  auto instanceExtensions = extensions_.allEnabled(VulkanExtensions::ExtensionType::Instance);

  vkInstance_ = VK_NULL_HANDLE;
  const VkResult creationErrorCode =
      (ivkCreateInstance(&vf_,
                         VK_API_VERSION_1_1,
                         static_cast<uint32_t>(config_.enableValidation),
                         static_cast<uint32_t>(config_.enableGPUAssistedValidation),
                         static_cast<uint32_t>(config_.enableSynchronizationValidation),
                         instanceExtensions.size(),
                         instanceExtensions.data(),
                         &vkInstance_));

  IGL_ASSERT_MSG(creationErrorCode != VK_ERROR_LAYER_NOT_PRESENT,
                 "ivkCreateInstance() failed. Did you forget to install the Vulkan SDK?");

  VK_ASSERT(creationErrorCode);

  // Do not remove for backward compatibility with projects using global functions.
  volkLoadInstance(vkInstance_);

  vulkan::functions::loadInstanceFunctions(*tableImpl_, vkInstance_);

#if defined(VK_EXT_debug_utils) && IGL_PLATFORM_WIN
  if (extensions_.enabled(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    VK_ASSERT(ivkCreateDebugUtilsMessenger(
        &vf_, vkInstance_, &vulkanDebugCallback, this, &vkDebugUtilsMessenger_));
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
  [[maybe_unused]] void* layer = nullptr;
#if IGL_PLATFORM_APPLE
  layer = igl::vulkan::getCAMetalLayer(window);
#endif
  VK_ASSERT(ivkCreateSurface(&vf_, vkInstance_, window, display, layer, &vkSurface_));
}

igl::Result VulkanContext::queryDevices(const HWDeviceQueryDesc& desc,
                                        std::vector<HWDeviceDesc>& outDevices) {
  outDevices.clear();

  // Physical devices
  uint32_t deviceCount = 0;
  VK_ASSERT_RETURN(vf_.vkEnumeratePhysicalDevices(vkInstance_, &deviceCount, nullptr));
  std::vector<VkPhysicalDevice> vkDevices(deviceCount);
  VK_ASSERT_RETURN(vf_.vkEnumeratePhysicalDevices(vkInstance_, &deviceCount, vkDevices.data()));

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
    vf_.vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

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

  useStagingForBuffers_ = !ivkIsHostVisibleSingleHeapMemory(&vf_, vkPhysicalDevice_);

  vf_.vkGetPhysicalDeviceFeatures2(vkPhysicalDevice_, &vkPhysicalDeviceFeatures2_);
  vf_.vkGetPhysicalDeviceProperties2(vkPhysicalDevice_, &vkPhysicalDeviceProperties2_);

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

  extensions_.enumerate(vf_, vkPhysicalDevice_);

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

  extensions_.enableCommonExtensions(VulkanExtensions::ExtensionType::Device, config_);
  // Enable extra device extensions
  for (size_t i = 0; i < numExtraDeviceExtensions; i++) {
    extensions_.enable(extraDeviceExtensions[i], VulkanExtensions::ExtensionType::Device);
  }
  if (config_.enableBufferDeviceAddress) {
    (void)IGL_VERIFY(extensions_.enable(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
                                        VulkanExtensions::ExtensionType::Device));
  }

  VulkanQueuePool queuePool(vf_, vkPhysicalDevice_);

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
      ivkCreateDevice(&vf_,
                      vkPhysicalDevice_,
                      qcis.size(),
                      qcis.data(),
                      extensions_.allEnabled(VulkanExtensions::ExtensionType::Device).size(),
                      extensions_.allEnabled(VulkanExtensions::ExtensionType::Device).data(),
                      vkPhysicalDeviceMultiviewFeatures_.multiview,
                      vkPhysicalDeviceShaderFloat16Int8Features_.shaderFloat16,
                      config_.enableBufferDeviceAddress,
                      config_.enableDescriptorIndexing,
                      vkPhysicalDeviceShaderDrawParametersFeatures_.shaderDrawParameters,
                      vkPhysicalDeviceSamplerYcbcrConversionFeatures_.samplerYcbcrConversion,
                      &vkPhysicalDeviceFeatures2_.features,
                      &device));
  if (!config_.enableConcurrentVkDevicesSupport) {
    // Do not remove for backward compatibility with projects using global functions.
    volkLoadDevice(device);
  }

  // Table functions are always bound to a device. Project using enableConcurrentVkDevicesSupport
  // should use own copy of function table bound to a device.
  vulkan::functions::loadDeviceFunctions(*tableImpl_, device);

  if (config_.enableBufferDeviceAddress && vf_.vkGetBufferDeviceAddressKHR == nullptr) {
    return Result(Result::Code::InvalidOperation, "Cannot initialize VK_KHR_buffer_device_address");
  }

  vf_.vkGetDeviceQueue(
      device, deviceQueues_.graphicsQueueFamilyIndex, 0, &deviceQueues_.graphicsQueue);
  vf_.vkGetDeviceQueue(
      device, deviceQueues_.computeQueueFamilyIndex, 0, &deviceQueues_.computeQueue);

  device_ =
      std::make_unique<igl::vulkan::VulkanDevice>(vf_, device, "Device: VulkanContext::device_");
  immediate_ =
      std::make_unique<igl::vulkan::VulkanImmediateCommands>(vf_,
                                                             device,
                                                             deviceQueues_.graphicsQueueFamilyIndex,
                                                             config_.exportableFences,
                                                             "VulkanContext::immediate_");
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
    vf_.vkCreatePipelineCache(device, &ci, nullptr, &pipelineCache_);
  }

  // Create Vulkan Memory Allocator
  if (IGL_VULKAN_USE_VMA) {
    VK_ASSERT_RETURN(ivkVmaCreateAllocator(&vf_,
                                           vkPhysicalDevice_,
                                           device_->getVkDevice(),
                                           vkInstance_,
                                           apiVersion,
                                           config_.enableBufferDeviceAddress,
                                           (VkDeviceSize)config_.vmaPreferredLargeHeapBlockSize,
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
  {
    const VkFormat dummyTextureFormat = VK_FORMAT_R8G8B8A8_UNORM;
    Result result;
    auto image = createImage(VK_IMAGE_TYPE_2D,
                             VkExtent3D{1, 1, 1},
                             dummyTextureFormat,
                             1,
                             1,
                             VK_IMAGE_TILING_OPTIMAL,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                 VK_IMAGE_USAGE_STORAGE_BIT,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                             0,
                             VK_SAMPLE_COUNT_1_BIT,
                             &result,
                             "Image: dummy 1x1");
    if (!IGL_VERIFY(result.isOk())) {
      return result;
    }
    if (!IGL_VERIFY(image.valid())) {
      return Result(Result::Code::InvalidOperation, "Cannot create VulkanImage");
    }
    auto imageView = image.createImageView(VK_IMAGE_VIEW_TYPE_2D,
                                           dummyTextureFormat,
                                           VK_IMAGE_ASPECT_COLOR_BIT,
                                           0,
                                           VK_REMAINING_MIP_LEVELS,
                                           0,
                                           1,
                                           "Image View: dummy 1x1");
    if (!IGL_VERIFY(imageView.valid())) {
      return Result(Result::Code::InvalidOperation, "Cannot create VulkanImageView");
    }
    const TextureHandle dummyTexture =
        textures_.create(std::make_shared<VulkanTexture>(std::move(image), std::move(imageView)));
    IGL_ASSERT(textures_.numObjects() == 1);
    const uint32_t pixel = 0xFF000000;
    stagingDevice_->imageData(
        (*textures_.get(dummyTexture))->getVulkanImage(),
        TextureType::TwoD,
        TextureRangeDesc::new2D(0, 0, 1, 1),
        TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8),
        0,
        &pixel);
  }

  // default sampler
  (void)samplers_.create(
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
                                      "Sampler: default"));
  IGL_ASSERT(samplers_.numObjects() == 1);

  growBindlessDescriptorPool(pimpl_->currentMaxBindlessTextures_,
                             pimpl_->currentMaxBindlessSamplers_);

  querySurfaceCapabilities();

#if defined(IGL_WITH_TRACY_GPU)
  profilingCommandPool_ = std::make_unique<VulkanCommandPool>(
      vf_,
      device_->getVkDevice(),
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      deviceQueues_.graphicsQueueFamilyIndex,
      "VulkanContext::profilingCommandPool_ (Tracy)");

  profilingCommandBuffer_ = VK_NULL_HANDLE;
  VK_ASSERT(ivkAllocateCommandBuffer(&vf_,
                                     device_->getVkDevice(),
                                     profilingCommandPool_->getVkCommandPool(),
                                     &profilingCommandBuffer_));

#if defined(VK_EXT_calibrated_timestamps)
  if (extensions_.enabled(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME)) {
    tracyCtx_ = TracyVkContextCalibrated(getVkPhysicalDevice(),
                                         getVkDevice(),
                                         deviceQueues_.graphicsQueue,
                                         profilingCommandBuffer_,
                                         vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
                                         vkGetCalibratedTimestampsEXT);
  }
#endif // VK_EXT_calibrated_timestamps
  // If VK_EXT_calibrated_timestamps is not available or it has not been enabled, use the
  // uncalibrated Tracy context
  if (!tracyCtx_) {
    tracyCtx_ = TracyVkContext(
        getVkPhysicalDevice(), getVkDevice(), deviceQueues_.graphicsQueue, profilingCommandBuffer_);
  }

  IGL_ASSERT_MSG(tracyCtx_, "Failed to create Tracy GPU profiling context");
#endif // IGL_WITH_TRACY_GPU

  // enables/disables enhanced shader debugging
  if (config_.enhancedShaderDebugging) {
    enhancedShaderDebuggingStore_ = std::make_unique<EnhancedShaderDebuggingStore>();
  }

  return Result();
}

void VulkanContext::growBindlessDescriptorPool(uint32_t newMaxTextures, uint32_t newMaxSamplers) {
  // only do allocations if actually enabled
  if (!config_.enableDescriptorIndexing) {
    return;
  }

  IGL_PROFILER_FUNCTION();

  pimpl_->currentMaxBindlessTextures_ = newMaxTextures;
  pimpl_->currentMaxBindlessSamplers_ = newMaxSamplers;

#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("growBindlessDescriptorPool(%u, %u)\n", newMaxTextures, newMaxSamplers);
#endif // IGL_VULKAN_PRINT_COMMANDS

  if (!IGL_VERIFY(newMaxTextures <= vkPhysicalDeviceDescriptorIndexingProperties_
                                        .maxDescriptorSetUpdateAfterBindSampledImages)) {
    // macOS: MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS is required when using this with MoltenVK
    IGL_LOG_ERROR(
        "Max Textures exceeded: %u (hardware max %u)",
        newMaxTextures,
        vkPhysicalDeviceDescriptorIndexingProperties_.maxDescriptorSetUpdateAfterBindSampledImages);
  }

  if (!IGL_VERIFY(
          newMaxSamplers <=
          vkPhysicalDeviceDescriptorIndexingProperties_.maxDescriptorSetUpdateAfterBindSamplers)) {
    // macOS: MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS is required when using this with MoltenVK
    IGL_LOG_ERROR(
        "Max Samplers exceeded %u (hardware max %u)",
        newMaxSamplers,
        vkPhysicalDeviceDescriptorIndexingProperties_.maxDescriptorSetUpdateAfterBindSamplers);
  }

  VkDevice device = getVkDevice();

  if (pimpl_->dpBindless_ != VK_NULL_HANDLE) {
    deferredTask(std::packaged_task<void()>([vf = &vf_, device, dp = pimpl_->dpBindless_]() {
      vf->vkDestroyDescriptorPool(device, dp, nullptr);
    }));
  }

  // create default descriptor set layout which is going to be shared by graphics pipelines
  constexpr uint32_t kNumBindings = 7;
  const std::array<VkDescriptorSetLayoutBinding, kNumBindings> bindings = {
      ivkGetDescriptorSetLayoutBinding(kBinding_Texture2D,
                                       VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                       pimpl_->currentMaxBindlessTextures_),
      ivkGetDescriptorSetLayoutBinding(kBinding_Texture2DArray,
                                       VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                       pimpl_->currentMaxBindlessTextures_),
      ivkGetDescriptorSetLayoutBinding(kBinding_Texture3D,
                                       VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                       pimpl_->currentMaxBindlessTextures_),
      ivkGetDescriptorSetLayoutBinding(kBinding_TextureCube,
                                       VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                       pimpl_->currentMaxBindlessTextures_),
      ivkGetDescriptorSetLayoutBinding(
          kBinding_Sampler, VK_DESCRIPTOR_TYPE_SAMPLER, pimpl_->currentMaxBindlessSamplers_),
      ivkGetDescriptorSetLayoutBinding(
          kBinding_SamplerShadow, VK_DESCRIPTOR_TYPE_SAMPLER, pimpl_->currentMaxBindlessSamplers_),
      ivkGetDescriptorSetLayoutBinding(kBinding_StorageImages,
                                       VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                       pimpl_->currentMaxBindlessTextures_),
  };
  const uint32_t flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                         VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
  const std::array<VkDescriptorBindingFlags, kNumBindings> bindingFlags = {
      flags, flags, flags, flags, flags, flags, flags};
  IGL_ASSERT(bindingFlags.back() == flags);
  pimpl_->dslBindless_ = std::make_unique<VulkanDescriptorSetLayout>(
      vf_,
      device,
      VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
      kNumBindings,
      bindings.data(),
      bindingFlags.data(),
      "Descriptor Set Layout: VulkanContext::dslBindless_");
  // create default descriptor pool and allocate 1 descriptor set
  const std::array<VkDescriptorPoolSize, kNumBindings> poolSizes = {
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, pimpl_->currentMaxBindlessTextures_},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, pimpl_->currentMaxBindlessTextures_},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, pimpl_->currentMaxBindlessTextures_},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, pimpl_->currentMaxBindlessTextures_},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, pimpl_->currentMaxBindlessSamplers_},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, pimpl_->currentMaxBindlessSamplers_},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, pimpl_->currentMaxBindlessTextures_},
  };
  VK_ASSERT(ivkCreateDescriptorPool(&vf_,
                                    device,
                                    VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                                    1,
                                    static_cast<uint32_t>(poolSizes.size()),
                                    poolSizes.data(),
                                    &pimpl_->dpBindless_));
  VK_ASSERT(ivkSetDebugObjectName(&vf_,
                                  device,
                                  VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                                  (uint64_t)pimpl_->dpBindless_,
                                  "Descriptor Pool: dpBindless_"));
  VK_ASSERT(ivkAllocateDescriptorSet(&vf_,
                                     device,
                                     pimpl_->dpBindless_,
                                     pimpl_->dslBindless_->getVkDescriptorSetLayout(),
                                     &pimpl_->dsBindless_));
  VK_ASSERT(ivkSetDebugObjectName(&vf_,
                                  device,
                                  VK_OBJECT_TYPE_DESCRIPTOR_SET,
                                  (uint64_t)pimpl_->dsBindless_,
                                  "Descriptor Set: dsBindless_"));
}

igl::Result VulkanContext::initSwapchain(uint32_t width, uint32_t height) {
  IGL_PROFILER_FUNCTION();

  if (!device_ || !immediate_) {
    IGL_LOG_ERROR("Call initContext() first");
    return Result(Result::Code::Unsupported, "Call initContext() first");
  }

  if (swapchain_) {
    vf_.vkDeviceWaitIdle(device_->device_);
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
    VK_ASSERT_RETURN(vf_.vkQueueWaitIdle(queue));
  }

  return getResultFromVkResult(VK_SUCCESS);
}

Result VulkanContext::present() const {
  if (!hasSwapchain()) {
    return Result(Result::Code::InvalidOperation, "No swapchain available");
  }

  return swapchain_->present(immediate_->acquireLastSubmitSemaphore());
}

std::unique_ptr<VulkanBuffer> VulkanContext::createBuffer(VkDeviceSize bufferSize,
                                                          VkBufferUsageFlags usageFlags,
                                                          VkMemoryPropertyFlags memFlags,
                                                          igl::Result* outResult,
                                                          const char* debugName) const {
  IGL_PROFILER_FUNCTION();

#define ENSURE_BUFFER_SIZE(flag, maxSize)                                                      \
  if (usageFlags & flag) {                                                                     \
    if (!IGL_VERIFY(bufferSize <= maxSize)) {                                                  \
      IGL_LOG_INFO("Max size of buffer exceeded " #flag ": %llu > %llu", bufferSize, maxSize); \
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
  return std::make_unique<VulkanBuffer>(
      *this, device_->getVkDevice(), bufferSize, usageFlags, memFlags, debugName);
}

VulkanImage VulkanContext::createImage(VkImageType imageType,
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
  IGL_PROFILER_FUNCTION();

  if (!validateImageLimits(
          imageType, samples, extent, getVkPhysicalDeviceProperties().limits, outResult)) {
    return VulkanImage();
  }

  return {*this,
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
          debugName};
}

std::unique_ptr<VulkanImage> VulkanContext::createImageFromFileDescriptor(
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

  return std::make_unique<VulkanImage>(*this,
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

void VulkanContext::checkAndUpdateDescriptorSets() {
  if (!awaitingCreation_) {
    // nothing to update here
    return;
  }

  // newly created resources can be used immediately - make sure they are put into descriptor sets
  IGL_PROFILER_FUNCTION();

  // here we remove deleted textures and samplers - everything which has only 1 reference is owned
  // by this context and can be released safely

  // textures
  {
    while (textures_.objects_.size() > 1 && textures_.objects_.back().obj_.use_count() == 1) {
      textures_.objects_.pop_back();
    }
    for (uint32_t i = 1; i < (uint32_t)textures_.objects_.size(); i++) {
      if (textures_.objects_[i].obj_ && textures_.objects_[i].obj_.use_count() == 1) {
        textures_.destroy(i);
      }
    }
  }
  // samplers
  {
    while (samplers_.objects_.size() > 1 && samplers_.objects_.back().obj_.use_count() == 1) {
      samplers_.objects_.pop_back();
    }
    for (uint32_t i = 1; i < (uint32_t)samplers_.objects_.size(); i++) {
      if (samplers_.objects_[i].obj_ && samplers_.objects_[i].obj_.use_count() == 1) {
        samplers_.destroy(i);
      }
    }
  }

  // update Vulkan bindless descriptor sets here
  if (!config_.enableDescriptorIndexing) {
    return;
  }

  uint32_t newMaxTextures = pimpl_->currentMaxBindlessTextures_;
  uint32_t newMaxSamplers = pimpl_->currentMaxBindlessSamplers_;

  while (textures_.objects_.size() > newMaxTextures) {
    newMaxTextures *= 2;
  }
  while (samplers_.objects_.size() > newMaxSamplers) {
    newMaxSamplers *= 2;
  }
  if (newMaxTextures != pimpl_->currentMaxBindlessTextures_ ||
      newMaxSamplers != pimpl_->currentMaxBindlessSamplers_) {
    growBindlessDescriptorPool(newMaxTextures, newMaxSamplers);
  }

  // make sure the guard values are always there
  IGL_ASSERT(!textures_.objects_.empty());
  IGL_ASSERT(!samplers_.objects_.empty());

  // 1. Sampled and storage images
  std::vector<VkDescriptorImageInfo> infoSampledImages;
  std::vector<VkDescriptorImageInfo> infoStorageImages;
  infoSampledImages.reserve(textures_.objects_.size());
  infoStorageImages.reserve(textures_.objects_.size());

  // use the dummy texture/sampler to avoid sparse array
  VkImageView dummyImageView = textures_.objects_[0].obj_->imageView_.getVkImageView();
  VkSampler dummySampler = samplers_.objects_[0].obj_->getVkSampler();

  for (const auto& entry : textures_.objects_) {
    const VulkanTexture* texture = entry.obj_.get();
    if (texture) {
      // multisampled images cannot be directly accessed from shaders
      const bool isTextureAvailable =
          (texture->image_.samples_ & VK_SAMPLE_COUNT_1_BIT) == VK_SAMPLE_COUNT_1_BIT;
      const bool isSampledImage = isTextureAvailable && texture->image_.isSampledImage();
      const bool isStorageImage = isTextureAvailable && texture->image_.isStorageImage();
      infoSampledImages.push_back(
          {dummySampler,
           isSampledImage ? texture->imageView_.getVkImageView() : dummyImageView,
           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
      infoStorageImages.push_back(VkDescriptorImageInfo{
          VK_NULL_HANDLE,
          isStorageImage ? texture->imageView_.getVkImageView() : dummyImageView,
          VK_IMAGE_LAYOUT_GENERAL});
    } else {
      infoSampledImages.push_back(
          {dummySampler, dummyImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
      infoStorageImages.push_back(
          VkDescriptorImageInfo{VK_NULL_HANDLE, dummyImageView, VK_IMAGE_LAYOUT_GENERAL});
    }
    IGL_ASSERT(infoSampledImages.back().imageView != VK_NULL_HANDLE);
    IGL_ASSERT(infoStorageImages.back().imageView != VK_NULL_HANDLE);
  }

  // 2. Samplers
  std::vector<VkDescriptorImageInfo> infoSamplers;
  infoSamplers.reserve(samplers_.objects_.size());

  for (const auto& entry : samplers_.objects_) {
    const VulkanSampler* sampler = entry.obj_.get();
    infoSamplers.push_back({sampler ? sampler->getVkSampler() : dummySampler,
                            VK_NULL_HANDLE,
                            VK_IMAGE_LAYOUT_UNDEFINED});
  }

  std::vector<VkWriteDescriptorSet> write;

  if (!infoSampledImages.empty()) {
    // use the same indexing for every texture type
    for (uint32_t i = kBinding_Texture2D; i != kBinding_TextureCube + 1; i++) {
      write.push_back(ivkGetWriteDescriptorSet_ImageInfo(pimpl_->dsBindless_,
                                                         i,
                                                         VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                         (uint32_t)infoSampledImages.size(),
                                                         infoSampledImages.data()));
    }
  };

  if (!infoSamplers.empty()) {
    for (uint32_t i = kBinding_Sampler; i != kBinding_SamplerShadow + 1; i++) {
      write.push_back(ivkGetWriteDescriptorSet_ImageInfo(pimpl_->dsBindless_,
                                                         i,
                                                         VK_DESCRIPTOR_TYPE_SAMPLER,
                                                         (uint32_t)infoSamplers.size(),
                                                         infoSamplers.data()));
    }
  }

  if (!infoStorageImages.empty()) {
    write.push_back(ivkGetWriteDescriptorSet_ImageInfo(pimpl_->dsBindless_,
                                                       kBinding_StorageImages,
                                                       VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                       (uint32_t)infoStorageImages.size(),
                                                       infoStorageImages.data()));
  };

  // do not switch to the next descriptor set if there is nothing to update
  if (!write.empty()) {
#if IGL_VULKAN_PRINT_COMMANDS
    IGL_LOG_INFO("Updating descriptor set dsBindless_\n");
#endif // IGL_VULKAN_PRINT_COMMANDS
    immediate_->wait(std::exchange(pimpl_->lastSubmitHandle_, immediate_->getLastSubmitHandle()));
    vf_.vkUpdateDescriptorSets(
        device_->getVkDevice(), static_cast<uint32_t>(write.size()), write.data(), 0, nullptr);
  }

  awaitingCreation_ = false;
}

std::shared_ptr<VulkanTexture> VulkanContext::createTexture(
    VulkanImage&& image,
    VulkanImageView&& imageView,
    [[maybe_unused]] const char* debugName) const {
  IGL_PROFILER_FUNCTION();

  const TextureHandle handle =
      textures_.create(std::make_shared<VulkanTexture>(std::move(image), std::move(imageView)));

  auto texture = *textures_.get(handle);

  if (!IGL_VERIFY(texture)) {
    return nullptr;
  }

  texture->textureId_ = handle.index();

  awaitingCreation_ = true;

  return texture;
}

std::shared_ptr<VulkanTexture> VulkanContext::createTextureFromVkImage(
    VkImage vkImage,
    VulkanImageCreateInfo imageCreateInfo,
    VulkanImageViewCreateInfo imageViewCreateInfo,
    const char* debugName) const {
  auto iglImage = VulkanImage(*this, device_->getVkDevice(), vkImage, imageCreateInfo, debugName);
  auto imageView = iglImage.createImageView(imageViewCreateInfo, debugName);
  return createTexture(std::move(iglImage), std::move(imageView), debugName);
}

std::shared_ptr<VulkanSampler> VulkanContext::createSampler(const VkSamplerCreateInfo& ci,
                                                            igl::Result* outResult,
                                                            const char* debugName) const {
  IGL_PROFILER_FUNCTION();

  const SamplerHandle handle = samplers_.create(
      std::make_shared<VulkanSampler>(*this, device_->getVkDevice(), ci, debugName));

  auto sampler = *samplers_.get(handle);

  if (!IGL_VERIFY(sampler)) {
    Result::setResult(outResult, Result::Code::InvalidOperation);
    return nullptr;
  }

  sampler->samplerId_ = handle.index();

  awaitingCreation_ = true;

  return sampler;
}

void VulkanContext::querySurfaceCapabilities() {
  // This is not an exhaustive list. It's only formats that we are using.
  // @fb-only
  const VkFormat depthFormats[] = {VK_FORMAT_D32_SFLOAT_S8_UINT,
                                   VK_FORMAT_D24_UNORM_S8_UINT,
                                   VK_FORMAT_D16_UNORM_S8_UINT,
                                   VK_FORMAT_D32_SFLOAT,
                                   VK_FORMAT_D16_UNORM,
                                   VK_FORMAT_S8_UINT};
  deviceDepthFormats_.reserve(IGL_ARRAY_NUM_ELEMENTS(depthFormats));
  for (const auto& depthFormat : depthFormats) {
    VkFormatProperties formatProps;
    vf_.vkGetPhysicalDeviceFormatProperties(vkPhysicalDevice_, depthFormat, &formatProps);

    if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ||
        formatProps.bufferFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ||
        formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      deviceDepthFormats_.push_back(depthFormat);
    }
  }

  if (vkSurface_ != VK_NULL_HANDLE) {
    vf_.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        vkPhysicalDevice_, vkSurface_, &deviceSurfaceCaps_);

    uint32_t formatCount;
    vf_.vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_, vkSurface_, &formatCount, nullptr);

    if (formatCount) {
      deviceSurfaceFormats_.resize(formatCount);
      vf_.vkGetPhysicalDeviceSurfaceFormatsKHR(
          vkPhysicalDevice_, vkSurface_, &formatCount, deviceSurfaceFormats_.data());
    }

    uint32_t presentModeCount;
    vf_.vkGetPhysicalDeviceSurfacePresentModesKHR(
        vkPhysicalDevice_, vkSurface_, &presentModeCount, nullptr);

    if (presentModeCount) {
      devicePresentModes_.resize(presentModeCount);
      vf_.vkGetPhysicalDeviceSurfacePresentModesKHR(
          vkPhysicalDevice_, vkSurface_, &presentModeCount, devicePresentModes_.data());
    }
  }
}

VkFormat VulkanContext::getClosestDepthStencilFormat(igl::TextureFormat desiredFormat) const {
  IGL_ASSERT(!deviceDepthFormats_.empty());
  // get a list of compatible depth formats for a given desired format
  // The list will contain depth format that are ordered from most to least closest
  const std::vector<VkFormat> compatibleDepthStencilFormatList =
      getCompatibleDepthStencilFormats(desiredFormat);

  // check if any of the format in compatible list is supported
  for (auto depthStencilFormat : compatibleDepthStencilFormatList) {
    if (std::find(deviceDepthFormats_.begin(), deviceDepthFormats_.end(), depthStencilFormat) !=
        deviceDepthFormats_.end()) {
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
  builder.build(vf_, device_->getVkDevice(), &pass);

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
  vf_.vkGetPipelineCacheData(device, pipelineCache_, &size, nullptr);

  std::vector<uint8_t> data(size);

  if (size) {
    vf_.vkGetPipelineCacheData(device, pipelineCache_, &size, data.data());
  }

  return data;
}

uint64_t VulkanContext::getFrameNumber() const {
  return swapchain_ ? swapchain_->getFrameNumber() : 0u;
}

void VulkanContext::updateBindingsTextures(VkCommandBuffer cmdBuf,
                                           VkPipelineLayout layout,
                                           VkPipelineBindPoint bindPoint,
                                           const BindingsTextures& data,
                                           const VulkanDescriptorSetLayout& dsl,
                                           const util::SpvModuleInfo& info) const {
  IGL_PROFILER_FUNCTION();

  DescriptorPoolsArena& arena = pimpl_->getOrCreateArena_CombinedImageSamplers(
      *this, dsl.getVkDescriptorSetLayout(), dsl.numBindings_);

  VkDescriptorSet dset = arena.getNextDescriptorSet(*immediate_, pimpl_->lastSubmitHandle_);

  // @fb-only
  VkDescriptorImageInfo infoSampledImages[IGL_TEXTURE_SAMPLERS_MAX]; // uninitialized
  uint32_t numImages = 0;

  // @fb-only
  VkWriteDescriptorSet writes[IGL_TEXTURE_SAMPLERS_MAX]; // uninitialized
  uint32_t numWrites = 0;

  // make sure the guard value is always there
  IGL_ASSERT(!textures_.objects_.empty());
  IGL_ASSERT(!samplers_.objects_.empty());

  // use the dummy texture/sampler to avoid sparse array
  VkImageView dummyImageView = textures_.objects_[0].obj_->imageView_.getVkImageView();
  VkSampler dummySampler = samplers_.objects_[0].obj_->getVkSampler();

  const bool isGraphics = bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS;

  for (const util::TextureDescription& d : info.textures) {
    IGL_ASSERT(d.descriptorSet == kBindPoint_CombinedImageSamplers);
    const uint32_t loc = d.bindingLocation;
    IGL_ASSERT(loc < IGL_TEXTURE_SAMPLERS_MAX);
    igl::vulkan::VulkanTexture* texture = data.textures[loc];
    if (texture && isGraphics) {
      IGL_ASSERT_MSG(data.samplers[loc], "A sampler should be bound to every bound texture slot");
    }
    VkSampler sampler = data.samplers[loc] ? data.samplers[loc]->getVkSampler() : dummySampler;
    // multisampled images cannot be directly accessed from shaders
    const bool isTextureAvailable =
        (texture != nullptr) &&
        ((texture->image_.samples_ & VK_SAMPLE_COUNT_1_BIT) == VK_SAMPLE_COUNT_1_BIT);
    const bool isSampledImage = isTextureAvailable && texture->image_.isSampledImage();
    writes[numWrites++] = ivkGetWriteDescriptorSet_ImageInfo(
        dset, loc, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &infoSampledImages[numImages]);
    infoSampledImages[numImages++] = {isSampledImage ? sampler : dummySampler,
                                      isSampledImage ? texture->imageView_.getVkImageView()
                                                     : dummyImageView,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  }

  if (numWrites) {
    IGL_PROFILER_ZONE("vkUpdateDescriptorSets()", IGL_PROFILER_COLOR_UPDATE);
    vf_.vkUpdateDescriptorSets(device_->getVkDevice(), numWrites, writes, 0, nullptr);
    IGL_PROFILER_ZONE_END();

#if IGL_VULKAN_PRINT_COMMANDS
    IGL_LOG_INFO("%p vkCmdBindDescriptorSets(%u) - textures\n", cmdBuf, bindPoint);
#endif // IGL_VULKAN_PRINT_COMMANDS
    vf_.vkCmdBindDescriptorSets(
        cmdBuf, bindPoint, layout, kBindPoint_CombinedImageSamplers, 1, &dset, 0, nullptr);
  }
}

void VulkanContext::updateBindingsUniformBuffers(VkCommandBuffer cmdBuf,
                                                 VkPipelineLayout layout,
                                                 VkPipelineBindPoint bindPoint,
                                                 BindingsBuffers& data,
                                                 const VulkanDescriptorSetLayout& dsl,
                                                 const util::SpvModuleInfo& info) const {
  IGL_PROFILER_FUNCTION();

  DescriptorPoolsArena& arena = pimpl_->getOrCreateArena_UniformBuffers(
      *this, dsl.getVkDescriptorSetLayout(), dsl.numBindings_);

  VkDescriptorSet dsetBufUniform =
      arena.getNextDescriptorSet(*immediate_, pimpl_->lastSubmitHandle_);

  // @fb-only
  VkWriteDescriptorSet writes[IGL_UNIFORM_BLOCKS_BINDING_MAX]; // uninitialized
  uint32_t numWrites = 0;

  for (const util::BufferDescription& b : info.uniformBuffers) {
    IGL_ASSERT(b.descriptorSet == kBindPoint_BuffersUniform);
    IGL_ASSERT_MSG(
        data.buffers[b.bindingLocation].buffer != VK_NULL_HANDLE,
        IGL_FORMAT(
            "Did you forget to call bindBuffer() for a uniform buffer at the binding location {}?",
            b.bindingLocation)
            .c_str());
    writes[numWrites++] = ivkGetWriteDescriptorSet_BufferInfo(dsetBufUniform,
                                                              b.bindingLocation,
                                                              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                              1,
                                                              &data.buffers[b.bindingLocation]);
  }

  if (numWrites) {
    IGL_PROFILER_ZONE("vkUpdateDescriptorSets()", IGL_PROFILER_COLOR_UPDATE);
    vf_.vkUpdateDescriptorSets(device_->getVkDevice(), numWrites, writes, 0, nullptr);
    IGL_PROFILER_ZONE_END();

#if IGL_VULKAN_PRINT_COMMANDS
    IGL_LOG_INFO("%p vkCmdBindDescriptorSets(%u) - uniform buffers\n", cmdBuf, bindPoint);
#endif // IGL_VULKAN_PRINT_COMMANDS
    vf_.vkCmdBindDescriptorSets(
        cmdBuf, bindPoint, layout, kBindPoint_BuffersUniform, 1, &dsetBufUniform, 0, nullptr);
  }
}

void VulkanContext::updateBindingsStorageBuffers(VkCommandBuffer cmdBuf,
                                                 VkPipelineLayout layout,
                                                 VkPipelineBindPoint bindPoint,
                                                 BindingsBuffers& data,
                                                 const VulkanDescriptorSetLayout& dsl,
                                                 const util::SpvModuleInfo& info) const {
  IGL_PROFILER_FUNCTION();

  DescriptorPoolsArena& arena = pimpl_->getOrCreateArena_StorageBuffers(
      *this, dsl.getVkDescriptorSetLayout(), dsl.numBindings_);

  VkDescriptorSet dsetBufStorage =
      arena.getNextDescriptorSet(*immediate_, pimpl_->lastSubmitHandle_);

  // @fb-only
  VkWriteDescriptorSet writes[IGL_UNIFORM_BLOCKS_BINDING_MAX]; // uninitialized
  uint32_t numWrites = 0;

  for (const util::BufferDescription& b : info.storageBuffers) {
    IGL_ASSERT(b.descriptorSet == kBindPoint_BuffersStorage);
    IGL_ASSERT_MSG(
        data.buffers[b.bindingLocation].buffer != VK_NULL_HANDLE,
        IGL_FORMAT(
            "Did you forget to call bindBuffer() for a storage buffer at the binding location {}?",
            b.bindingLocation)
            .c_str());
    writes[numWrites++] = ivkGetWriteDescriptorSet_BufferInfo(dsetBufStorage,
                                                              b.bindingLocation,
                                                              VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              1,
                                                              &data.buffers[b.bindingLocation]);
  }

  if (numWrites) {
    IGL_PROFILER_ZONE("vkUpdateDescriptorSets()", IGL_PROFILER_COLOR_UPDATE);
    vf_.vkUpdateDescriptorSets(device_->getVkDevice(), numWrites, writes, 0, nullptr);
    IGL_PROFILER_ZONE_END();

#if IGL_VULKAN_PRINT_COMMANDS
    IGL_LOG_INFO("%p vkCmdBindDescriptorSets(%u) - storage buffers\n", cmdBuf, bindPoint);
#endif // IGL_VULKAN_PRINT_COMMANDS
    vf_.vkCmdBindDescriptorSets(
        cmdBuf, bindPoint, layout, kBindPoint_BuffersStorage, 1, &dsetBufStorage, 0, nullptr);
  }
}

void VulkanContext::markSubmitted(const VulkanImmediateCommands::SubmitHandle& handle) const {
  pimpl_->lastSubmitHandle_ = handle;
}

void VulkanContext::deferredTask(std::packaged_task<void()>&& task, SubmitHandle handle) const {
  if (handle.empty()) {
    handle = immediate_->getLastSubmitHandle();
  }
  deferredTasks_.emplace_back(std::move(task), handle);
  deferredTasks_.back().frameId_ = this->getFrameNumber();
}

bool VulkanContext::areValidationLayersEnabled() const {
  return config_.enableValidation;
}

void* VulkanContext::getVmaAllocator() const {
  return pimpl_->vma_;
}

void VulkanContext::processDeferredTasks() const {
  IGL_PROFILER_FUNCTION();

  const uint64_t frameId = getFrameNumber();
  constexpr uint64_t kNumWaitFrames = 1u;

  while (!deferredTasks_.empty() && immediate_->isRecycled(deferredTasks_.front().handle_)) {
    if (frameId && frameId <= deferredTasks_.front().frameId_ + kNumWaitFrames) {
      // do not check anything if it is not yet older than kNumWaitFrames
      break;
    }
    deferredTasks_.front().task_();
    deferredTasks_.pop_front();
  }
}

void VulkanContext::waitDeferredTasks() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_WAIT);

  for (auto& task : deferredTasks_) {
    immediate_->wait(task.handle_);
    task.task_();
  }
  deferredTasks_.clear();
}

VkDescriptorSetLayout VulkanContext::getBindlessVkDescriptorSetLayout() const {
  return config_.enableDescriptorIndexing ? pimpl_->dslBindless_->getVkDescriptorSetLayout()
                                          : VK_NULL_HANDLE;
}

VkDescriptorSet VulkanContext::getBindlessVkDescriptorSet() const {
  return config_.enableDescriptorIndexing ? pimpl_->dsBindless_ : VK_NULL_HANDLE;
}

} // namespace vulkan
} // namespace igl
