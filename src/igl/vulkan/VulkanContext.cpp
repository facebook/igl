/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <array>
#include <cstring>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

// For vk_mem_alloc.h, define this before including VulkanContext.h in exactly
// one CPP file
#if defined(IGL_CMAKE_BUILD)
#define VMA_IMPLEMENTATION
#endif // IGL_CMAKE_BUILD

// For volk.h, define this before including volk.h in exactly one CPP file.
// @fb-only
#if defined(IGL_CMAKE_BUILD)
#define VOLK_IMPLEMENTATION
#endif // IGL_CMAKE_BUILD

#include <igl/glslang/GlslCompiler.h>

#include <igl/SamplerState.h>
#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/EnhancedShaderDebuggingStore.h>
#include <igl/vulkan/RenderPipelineState.h>
#include <igl/vulkan/SamplerState.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanBuffer.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDescriptorSetLayout.h>
#include <igl/vulkan/VulkanFeatures.h>
#include <igl/vulkan/VulkanImageView.h>
#include <igl/vulkan/VulkanPipelineBuilder.h>
#include <igl/vulkan/VulkanSwapchain.h>
#include <igl/vulkan/VulkanTexture.h>
#include <igl/vulkan/VulkanVma.h>
#include <igl/vulkan/util/SpvReflection.h>

#if IGL_PLATFORM_APPLE
#include <igl/vulkan/moltenvk/MoltenVkHelpers.h>
#endif

namespace {

[[maybe_unused]] const char* kValidationLayerName = "VK_LAYER_KHRONOS_validation";
const char* kGfxReconstructLayerName = "VK_LAYER_LUNARG_gfxreconstruct";

/*
 BINDLESS ONLY: these bindings should match GLSL declarations injected into shaders in
 Device::compileShaderModule(). Same with SparkSL.
 */
// NOLINTBEGIN(readability-identifier-naming)
const uint32_t kBinding_Texture2D = 0;
const uint32_t kBinding_Texture2DArray = 1;
const uint32_t kBinding_Texture3D = 2;
const uint32_t kBinding_TextureCube = 3;
const uint32_t kBinding_Sampler = 4;
const uint32_t kBinding_SamplerShadow = 5;
const uint32_t kBinding_StorageImages = 6;
// NOLINTEND(readability-identifier-naming)

#if defined(VK_EXT_debug_utils) && IGL_PLATFORM_WINDOWS
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

#if IGL_LOGGING_ENABLED
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
#if IGL_VULKAN_VALIDATION_LAYER_ERROR_SUMMARY
    ctx.validationErrorsSummary_[errorName.data()]++;
#endif
  } else {
    const bool isWarning = (msgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0;

    if (isError || isWarning || ctx->config_.enableExtraLogs) {
      IGL_LOG_INFO("%sValidation layer:\n%s\n", isError ? "\nERROR:\n" : "", cbData->pMessage);
    }
  }
#endif

  if (ctx->config_.terminateOnValidationError) {
    if (IGL_DEBUG_VERIFY_NOT(isError)) {
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

bool validateImageLimits(VkImageType imageType,
                         VkSampleCountFlagBits samples,
                         const VkExtent3D& extent,
                         const VkPhysicalDeviceLimits& limits,
                         igl::Result* IGL_NULLABLE outResult) {
  using igl::Result;

  if (samples != VK_SAMPLE_COUNT_1_BIT && !IGL_DEBUG_VERIFY(imageType == VK_IMAGE_TYPE_2D)) {
    Result::setResult(
        outResult,
        Result(Result::Code::InvalidOperation, "Multisampling is supported only for 2D images"));
    return false;
  }

  if (imageType == VK_IMAGE_TYPE_1D &&
      !IGL_DEBUG_VERIFY(extent.width <= limits.maxImageDimension1D)) {
    Result::setResult(outResult,
                      Result(Result::Code::InvalidOperation, "1D texture size exceeded"));
    return false;
  } else if (imageType == VK_IMAGE_TYPE_2D &&
             !IGL_DEBUG_VERIFY(extent.width <= limits.maxImageDimension2D &&
                               extent.height <= limits.maxImageDimension2D)) {
    Result::setResult(outResult,
                      Result(Result::Code::InvalidOperation, "2D texture size exceeded"));
    return false;
  } else if (imageType == VK_IMAGE_TYPE_3D &&
             !IGL_DEBUG_VERIFY(extent.width <= limits.maxImageDimension3D &&
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

namespace igl::vulkan {

// @fb-only
class DescriptorPoolsArena final {
 public:
  DescriptorPoolsArena(const VulkanContext& ctx,
                       VkDescriptorType type,
                       VkDescriptorSetLayout dsl,
                       uint32_t numDescriptorsPerDSet,
                       const char* IGL_NULLABLE debugName) :
    ctx_(ctx),
    device_(ctx.getVkDevice()),
    numTypes_(1),
    types_{type},
    numDescriptorsPerDSet_(numDescriptorsPerDSet),
    dsl_(dsl) {
    IGL_DEBUG_ASSERT(debugName);
    dpDebugName_ = IGL_FORMAT("Descriptor Pool: {}", debugName ? debugName : "");
  }
  DescriptorPoolsArena(const VulkanContext& ctx,
                       VkDescriptorType type0,
                       VkDescriptorType type1,
                       VkDescriptorSetLayout dsl,
                       uint32_t numDescriptorsPerDSet,
                       const char* debugName) :
    ctx_(ctx),
    device_(ctx.getVkDevice()),
    numTypes_(2),
    types_{type0, type1},
    numDescriptorsPerDSet_(numDescriptorsPerDSet),
    dsl_(dsl) {
    IGL_DEBUG_ASSERT(debugName);
    dpDebugName_ = IGL_FORMAT("Descriptor Pool: {}", debugName ? debugName : "");
  }
  ~DescriptorPoolsArena() {
    extinct_.push_back({pool_, {}});
    ctx_.deferredTask(std::packaged_task<void()>(
        [extinct = std::move(extinct_), vf = ctx_.vf_, device = device_]() {
          for (const auto& p : extinct) {
            vf.vkDestroyDescriptorPool(device, p.pool, nullptr);
          }
        }));
  }
  [[nodiscard]] VkDescriptorSetLayout getVkDescriptorSetLayout() const {
    return dsl_;
  }
  [[nodiscard]] VkDescriptorSet getNextDescriptorSet(
      VulkanImmediateCommands& ic,
      VulkanImmediateCommands::SubmitHandle nextSubmitHandle) {
    IGL_DEBUG_ASSERT(!nextSubmitHandle.empty());

    VkDescriptorSet dset = VK_NULL_HANDLE;
    if (!numRemainingDSetsInPool_) {
      switchToNewDescriptorPool(ic, nextSubmitHandle);
    }
    VK_ASSERT(ivkAllocateDescriptorSet(&ctx_.vf_, device_, pool_, dsl_, &dset));
    numRemainingDSetsInPool_--;
    return dset;
  }

 private:
  void switchToNewDescriptorPool(VulkanImmediateCommands& ic,
                                 VulkanImmediateCommands::SubmitHandle nextSubmitHandle) {
    numRemainingDSetsInPool_ = kNumDSetsPerPool;

    if (pool_ != VK_NULL_HANDLE) {
      extinct_.push_back({pool_, nextSubmitHandle});
    }
    // first, let's try to reuse the oldest extinct pool (never reuse pools that are tagged with the
    // same SubmitHandle because they have not yet been submitted)
    if (extinct_.size() > 1 && extinct_.front().handle != nextSubmitHandle) {
      const ExtinctDescriptorPool p = extinct_.front();
      if (ic.isReady(p.handle)) {
        pool_ = p.pool;
        extinct_.pop_front();
        VK_ASSERT(ctx_.vf_.vkResetDescriptorPool(device_, pool_, VkDescriptorPoolResetFlags{}));
        return;
      }
    }
    // @fb-only
    VkDescriptorPoolSize poolSizes[IGL_ARRAY_NUM_ELEMENTS(types_)];
    for (uint32_t i = 0; i != numTypes_; i++) {
      poolSizes[i] = VkDescriptorPoolSize{
          types_[i], numDescriptorsPerDSet_ ? kNumDSetsPerPool * numDescriptorsPerDSet_ : 1u};
    }
    VK_ASSERT(ivkCreateDescriptorPool(&ctx_.vf_,
                                      device_,
                                      VkDescriptorPoolCreateFlags{},
                                      kNumDSetsPerPool,
                                      numTypes_,
                                      poolSizes,
                                      &pool_));
    VK_ASSERT(ivkSetDebugObjectName(
        &ctx_.vf_, device_, VK_OBJECT_TYPE_DESCRIPTOR_POOL, (uint64_t)pool_, dpDebugName_.c_str()));
  }

 private:
  static constexpr uint32_t kNumDSetsPerPool = 64;

  const VulkanContext& ctx_;
  VkDevice device_ = VK_NULL_HANDLE;
  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  const uint32_t numTypes_ = 0;
  VkDescriptorType types_[2] = {VK_DESCRIPTOR_TYPE_MAX_ENUM, VK_DESCRIPTOR_TYPE_MAX_ENUM};
  const uint32_t numDescriptorsPerDSet_ = 0;
  uint32_t numRemainingDSetsInPool_ = 0;
  std::string dpDebugName_;

  VkDescriptorSetLayout dsl_ = VK_NULL_HANDLE; // owned elsewhere

  struct ExtinctDescriptorPool {
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VulkanImmediateCommands::SubmitHandle handle = {};
  };

  std::deque<ExtinctDescriptorPool> extinct_;
};

namespace {

struct BindGroupMetadataTextures {
  // cold
  BindGroupTextureDesc desc = {};
  VkDescriptorPool pool = VK_NULL_HANDLE;
  // hot
  VkDescriptorSet dset = VK_NULL_HANDLE;
  uint32_t usageMask = 0;
};

struct BindGroupMetadataBuffers {
  // cold
  BindGroupBufferDesc desc = {};
  VkDescriptorPool pool = VK_NULL_HANDLE;
  // hot
  VkDescriptorSet dset = VK_NULL_HANDLE;
  uint32_t usageMask = 0;
};

} // namespace

struct VulkanContextImpl final {
  std::thread::id contextThread = std::this_thread::get_id();

  // Vulkan Memory Allocator
  VmaAllocator vma = VK_NULL_HANDLE;
  // :)
  std::unordered_map<VkDescriptorSetLayout, std::unique_ptr<igl::vulkan::DescriptorPoolsArena>>
      arenaCombinedImageSamplers;
  std::unordered_map<VkDescriptorSetLayout, std::unique_ptr<igl::vulkan::DescriptorPoolsArena>>
      arenaBuffers;
  std::unordered_map<VkDescriptorSetLayout, std::unique_ptr<igl::vulkan::DescriptorPoolsArena>>
      arenaStorageImages;
  std::unique_ptr<igl::vulkan::VulkanDescriptorSetLayout> dslBindless; // everything
  VkDescriptorPool dpBindless = VK_NULL_HANDLE;
  VkDescriptorSet dsBindless = VK_NULL_HANDLE;
  uint32_t currentMaxBindlessTextures = 8;
  uint32_t currentMaxBindlessSamplers = 8;

  Pool<BindGroupBufferTag, BindGroupMetadataBuffers> bindGroupBuffersPool;
  Pool<BindGroupTextureTag, BindGroupMetadataTextures> bindGroupTexturesPool;

  SamplerHandle dummySampler = {};
  TextureHandle dummyTexture = {};

  // NOLINTBEGIN(readability-identifier-naming)
  igl::vulkan::DescriptorPoolsArena& getOrCreateArena_CombinedImageSamplers(
      const VulkanContext& ctx,
      VkDescriptorSetLayout dsl,
      uint32_t numBindings)
  // NOLINTEND(readability-identifier-naming)
  {
    auto it = arenaCombinedImageSamplers.find(dsl);
    if (it != arenaCombinedImageSamplers.end()) {
      return *it->second;
    }
    arenaCombinedImageSamplers[dsl] =
        std::make_unique<DescriptorPoolsArena>(ctx,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               dsl,
                                               numBindings,
                                               "arenaCombinedImageSamplers_");
    return *arenaCombinedImageSamplers[dsl].get();
  }
  // NOLINTBEGIN(readability-identifier-naming)
  igl::vulkan::DescriptorPoolsArena& getOrCreateArena_StorageImages(const VulkanContext& ctx,
                                                                    VkDescriptorSetLayout dsl,
                                                                    uint32_t numBindings)
  // NOLINTEND(readability-identifier-naming)
  {
    auto it = arenaStorageImages.find(dsl);
    if (it != arenaStorageImages.end()) {
      return *it->second;
    }
    arenaStorageImages[dsl] = std::make_unique<DescriptorPoolsArena>(
        ctx, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, dsl, numBindings, "arenaStorageImages_");
    return *arenaStorageImages[dsl].get();
  }
  // NOLINTBEGIN(readability-identifier-naming)
  igl::vulkan::DescriptorPoolsArena& getOrCreateArena_Buffers(const VulkanContext& ctx,
                                                              VkDescriptorSetLayout dsl,
                                                              uint32_t numBindings)
  // NOLINTEND(readability-identifier-naming)
  {
    auto it = arenaBuffers.find(dsl);
    if (it != arenaBuffers.end()) {
      return *it->second;
    }
    arenaBuffers[dsl] = std::make_unique<DescriptorPoolsArena>(ctx,
                                                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                               dsl,
                                                               numBindings,
                                                               "arenaBuffers_");
    return *arenaBuffers[dsl].get();
  }
};

VulkanContext::VulkanContext(VulkanContextConfig config,
                             void* IGL_NULLABLE window,
                             void* IGL_NULLABLE display) :
  tableImpl_(std::make_unique<VulkanFunctionTable>()),
  vkPhysicalDeviceDescriptorIndexingProperties_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT,
      .pNext = nullptr,
  }),
  vkPhysicalDeviceDriverProperties_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR,
      .pNext = &vkPhysicalDeviceDescriptorIndexingProperties_,
  }),
  vkPhysicalDeviceProperties2_({
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
      .pNext = &vkPhysicalDeviceDriverProperties_,
  }),
  features_(config),
  vf_(*tableImpl_),
  config_(config) {
  IGL_PROFILER_THREAD("MainThread");

  pimpl_ = std::make_unique<VulkanContextImpl>();

#if defined(IGL_CMAKE_BUILD)
  const auto result = volkInitialize();

  // Do not remove for backward compatibility with projects using global functions.
  if (result != VK_SUCCESS) {
    IGL_LOG_ERROR("volkInitialize() failed with error code %d\n", static_cast<int>(result));
    abort();
  };
#endif // IGL_CMAKE_BUILD
  vulkan::functions::initialize(*tableImpl_);

  glslang::initializeCompiler();

  createInstance();

  if (config_.headless) {
    IGL_DEBUG_ASSERT(features_.has_VK_EXT_headless_surface,
                     "VK_EXT_headless_surface extension is not supported");
    createHeadlessSurface();
  } else if (window || display) {
    createSurface(window, display);
  }
}

VulkanContext::~VulkanContext() {
  IGL_PROFILER_FUNCTION();

  if (vkDevice_) {
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

#if IGL_DEBUG_ABORT_ENABLED
  for (const auto& t : pimpl_->bindGroupTexturesPool.objects_) {
    if (t.obj_.dset != VK_NULL_HANDLE) {
      IGL_DEBUG_ABORT("Leaked texture bind group detected! %s", t.obj_.desc.debugName.c_str());
    }
  }
  for (const auto& t : pimpl_->bindGroupBuffersPool.objects_) {
    if (t.obj_.dset != VK_NULL_HANDLE) {
      IGL_DEBUG_ABORT("Leaked buffer bind group detected! %s", t.obj_.desc.debugName.c_str());
    }
  }
#endif // IGL_DEBUG_ABORT_ENABLED

  // BindGroups can hold shared pointers to textures/samplers/buffers. Release them here.
  pimpl_->bindGroupTexturesPool.clear();
  pimpl_->bindGroupBuffersPool.clear();

  destroy(pimpl_->dummySampler);
  destroy(pimpl_->dummyTexture);

  pruneTextures();

#if IGL_LOGGING_ENABLED
  if (textures_.numObjects()) {
    IGL_LOG_ERROR("Leaked %u textures\n", textures_.numObjects());
  }
  if (samplers_.numObjects()) {
    IGL_LOG_ERROR("Leaked %u samplers\n", samplers_.numObjects());
  }
#endif // IGL_LOGGING_ENABLED
  textures_.clear();
  samplers_.clear();

  // This will free an internal buffer that was allocated by VMA
  stagingDevice_.reset(nullptr);

  if (vkDevice_) {
    for (auto r : renderPasses_) {
      vf_.vkDestroyRenderPass(vkDevice_, r, nullptr);
    }
  }

  pimpl_->dslBindless.reset(nullptr);

  swapchain_.reset(nullptr); // Swapchain has to be destroyed prior to Surface

  waitDeferredTasks();

  immediate_.reset(nullptr);
  timelineSemaphore_.reset(nullptr);

  if (vkDevice_) {
    if (pimpl_->dpBindless != VK_NULL_HANDLE) {
      vf_.vkDestroyDescriptorPool(vkDevice_, pimpl_->dpBindless, nullptr);
    }
    for (auto& p : ycbcrConversionInfos_) {
      if (p.second.conversion != VK_NULL_HANDLE) {
        vf_.vkDestroySamplerYcbcrConversion(vkDevice_, p.second.conversion, nullptr);
      }
    }
    pimpl_->arenaCombinedImageSamplers.clear();
    pimpl_->arenaStorageImages.clear();
    pimpl_->arenaBuffers.clear();
    vf_.vkDestroyPipelineCache(vkDevice_, pipelineCache_, nullptr);
  }

  if (vkSurface_ != VK_NULL_HANDLE) {
    vf_.vkDestroySurfaceKHR(vkInstance_, vkSurface_, nullptr);
  }

  // Clean up VMA
  if (IGL_VULKAN_USE_VMA) {
    vmaDestroyAllocator(pimpl_->vma);
  }

  if (vkDevice_) {
    vf_.vkDestroyDevice(vkDevice_, nullptr); // Device has to be destroyed prior to Instance
  }
#if defined(VK_EXT_debug_utils) && !IGL_PLATFORM_ANDROID
  if (vf_.vkDestroyDebugUtilsMessengerEXT != nullptr) {
    vf_.vkDestroyDebugUtilsMessengerEXT(vkInstance_, vkDebugUtilsMessenger_, nullptr);
  }
#endif // defined(VK_EXT_debug_utils) && !IGL_PLATFORM_ANDROID
  if (vf_.vkDestroyInstance != nullptr) {
    vf_.vkDestroyInstance(vkInstance_, nullptr);
  }

  glslang::finalizeCompiler();

#if IGL_LOGGING_ENABLED
  if (config_.enableExtraLogs) {
    IGL_LOG_INFO("Vulkan graphics pipelines created: %u\n",
                 VulkanPipelineBuilder::getNumPipelinesCreated());
    IGL_LOG_INFO("Vulkan compute pipelines created: %u\n",
                 VulkanComputePipelineBuilder::getNumPipelinesCreated());
  }
#endif // IGL_LOGGING_ENABLED

#if defined(IGL_CMAKE_BUILD)
  volkFinalize();
#endif
}

void VulkanContext::createInstance() {
  IGL_DEBUG_ASSERT(vkInstance_ == VK_NULL_HANDLE, "createInstance() is not reentrant");

  // Enumerate all instance extensions
  features_.enumerate(vf_);
  // NOLINTBEGIN(readability-identifier-naming)
  features_.enableCommonInstanceExtensions(config_);
  for (size_t index = 0; index < config_.numExtraInstanceExtensions; ++index) {
    features_.enable(config_.extraInstanceExtensions[index],
                     VulkanFeatures::ExtensionType::Instance);
  }
  // NOLINTEND(readability-identifier-naming)
  auto instanceExtensions = features_.allEnabled(VulkanFeatures::ExtensionType::Instance);

  std::vector<const char*> layers;
  // @fb-only
#if !IGL_PLATFORM_ANDROID && !IGL_PLATFORM_MACOSX
  if (config_.enableValidation) {
    layers.emplace_back(kValidationLayerName);
  }
#endif
  if (config_.enableGfxReconstruct) {
    layers.emplace_back(kGfxReconstructLayerName);
  }

  // Validation Features not available on most Android devices
#if !IGL_PLATFORM_ANDROID && !IGL_PLATFORM_MACOSX
  std::vector<VkValidationFeatureEnableEXT> valFeatures;
  if (config_.enableGPUAssistedValidation) {
    valFeatures.push_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
  }
  const VkValidationFeaturesEXT features = {
      .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
      .enabledValidationFeatureCount = (uint32_t)valFeatures.size(),
      .pEnabledValidationFeatures = valFeatures.empty() ? nullptr : valFeatures.data(),
  };
#endif // !IGL_PLATFORM_ANDROID

  const VkApplicationInfo appInfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = config_.applicationName,
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = config_.engineName,
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_2,
  };

  const VkInstanceCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
#if !IGL_PLATFORM_ANDROID && !IGL_PLATFORM_MACOSX
      .pNext = config_.enableValidation ? &features : nullptr,
#endif
#if IGL_PLATFORM_MACOSX || IGL_PLATFORM_MACCATALYST
      .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
#endif
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = static_cast<uint32_t>(layers.size()),
      .ppEnabledLayerNames = !layers.empty() ? layers.data() : nullptr,
      .enabledExtensionCount = (uint32_t)instanceExtensions.size(),
      .ppEnabledExtensionNames = instanceExtensions.data(),
  };

  {
    // Prints information about available instance layers
    uint32_t count = 0;
    vf_.vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layerProperties(count);
    vf_.vkEnumerateInstanceLayerProperties(&count, layerProperties.data());

    IGL_LOG_INFO("Found %u Vulkan instance layers\n", count);
    for ([[maybe_unused]] const auto& layer : layerProperties) {
      IGL_LOG_INFO("\t%s - %u.%u.%u.%u, %u\n",
                   layer.layerName,
                   VK_API_VERSION_MAJOR(layer.specVersion),
                   VK_API_VERSION_MINOR(layer.specVersion),
                   VK_API_VERSION_VARIANT(layer.specVersion),
                   VK_API_VERSION_PATCH(layer.specVersion),
                   layer.implementationVersion);
    }
  }

  const VkResult result = vf_.vkCreateInstance(&ci, nullptr, &vkInstance_);

  IGL_DEBUG_ASSERT(result != VK_ERROR_LAYER_NOT_PRESENT,
                   "vkCreateInstance() failed. Did you forget to install the Vulkan SDK?");

  VK_ASSERT(result);

#if defined(IGL_CMAKE_BUILD)
  // Do not remove for backward compatibility with projects using global functions.
  volkLoadInstance(vkInstance_);
#endif
  const bool enableExtDebugUtils =
      features_.enable(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VulkanFeatures::ExtensionType::Instance);
  vulkan::functions::loadInstanceFunctions(*tableImpl_, vkInstance_, enableExtDebugUtils);

#if defined(VK_EXT_debug_utils) && IGL_PLATFORM_WINDOWS
  if (features_.enabled(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    VK_ASSERT(ivkCreateDebugUtilsMessenger(
        &vf_, vkInstance_, &vulkanDebugCallback, this, &vkDebugUtilsMessenger_));
  }
#endif // if defined(VK_EXT_debug_utils) && IGL_PLATFORM_WINDOWS

#if IGL_LOGGING_ENABLED
  if (config_.enableExtraLogs) {
    // log available instance extensions
    IGL_LOG_INFO("Vulkan instance extensions:\n");
    for (const auto& extension :
         features_.allAvailableExtensions(VulkanFeatures::ExtensionType::Instance)) {
      IGL_LOG_INFO("  %s\n", extension.c_str());
    }
  }
#endif
}

void VulkanContext::createHeadlessSurface() {
  const VkHeadlessSurfaceCreateInfoEXT ci = {
      .sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT,
      .pNext = nullptr,
      .flags = 0,
  };

  VK_ASSERT(vf_.vkCreateHeadlessSurfaceEXT(vkInstance_, &ci, nullptr, &vkSurface_));
}

void VulkanContext::createSurface(void* IGL_NULLABLE window, void* IGL_NULLABLE display) {
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

  if (vf_.vkEnumeratePhysicalDevices == nullptr) {
    return Result(Result::Code::Unsupported, "Vulkan functions are not loaded");
  }

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
                                       const char* IGL_NULLABLE* IGL_NULLABLE extraDeviceExtensions,
                                       const VulkanFeatures* IGL_NULLABLE requestedFeatures,
                                       const char* IGL_NULLABLE debugName) {
  IGL_DEBUG_ASSERT(vkDevice_ == VK_NULL_HANDLE);

  if (desc.guid == 0UL) {
    IGL_LOG_ERROR("Invalid hardwareGuid(%lu)", desc.guid);
    return Result(Result::Code::Unsupported, "Vulkan is not supported");
  }

  vkPhysicalDevice_ = (VkPhysicalDevice)desc.guid; // NOLINT(performance-no-int-to-ptr)

  useStagingForBuffers_ = !ivkIsHostVisibleSingleHeapMemory(&vf_, vkPhysicalDevice_);

  // Get the available physical device features
  VulkanFeatures availableFeatures(config_);
  availableFeatures.populateWithAvailablePhysicalDeviceFeatures(*this, vkPhysicalDevice_);

  // Use the requested features passed to the function (if any) or use the default features
  if (requestedFeatures) {
    features_ = *requestedFeatures;
  }

  features_.populateWithAvailablePhysicalDeviceFeatures(*this, vkPhysicalDevice_);

  // ... and check whether they are available in the physical device (they should be)
  {
    auto featureCheckResult = features_.checkSelectedFeatures(availableFeatures);
    if (!featureCheckResult.isOk()) {
      return featureCheckResult;
    }
  }

  vf_.vkGetPhysicalDeviceProperties2(vkPhysicalDevice_, &vkPhysicalDeviceProperties2_);

  const uint32_t apiVersion = vkPhysicalDeviceProperties2_.properties.apiVersion;

  if (config_.enableExtraLogs) {
    IGL_LOG_INFO("Device: %s\n", debugName ? debugName : "igl/vulkan/VulkanContext.cpp");
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

  features_.enumerate(vf_, vkPhysicalDevice_);

#if IGL_LOGGING_ENABLED
  if (config_.enableExtraLogs) {
    IGL_LOG_INFO("Vulkan physical device extensions:\n");
    // log available physical device extensions
    for (const auto& extension :
         features_.allAvailableExtensions(VulkanFeatures::ExtensionType::Device)) {
      IGL_LOG_INFO("  %s\n", extension.c_str());
    }
  }
#endif

  features_.enableCommonDeviceExtensions(config_);
  // Enable extra device extensions
  for (size_t i = 0; i < numExtraDeviceExtensions; i++) {
    features_.enable(extraDeviceExtensions[i], VulkanFeatures::ExtensionType::Device);
  }

  // @fb-only
    // @fb-only
                     // @fb-only
  // @fb-only

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

  const auto qcis = queuePool.getQueueCreationInfos();

  auto deviceExtensions = features_.allEnabled(VulkanFeatures::ExtensionType::Device);

  VkDevice device = nullptr;
  VK_ASSERT_RETURN(ivkCreateDevice(&vf_,
                                   vkPhysicalDevice_,
                                   qcis.size(),
                                   qcis.data(),
                                   deviceExtensions.size(),
                                   deviceExtensions.data(),
                                   &features_.vkPhysicalDeviceFeatures2,
                                   &device));

  // Check that device is not null before proceeding
  if (device == VK_NULL_HANDLE) {
    return Result(Result::Code::InvalidOperation, "Failed to create Vulkan device");
  }
#if defined(IGL_CMAKE_BUILD)
  if (!config_.enableConcurrentVkDevicesSupport) {
    // Do not remove for backward compatibility with projects using global functions.
    volkLoadDevice(device);
  }
#endif

  // Table functions are always bound to a device. Project using enableConcurrentVkDevicesSupport
  // should use own copy of function table bound to a device.
  vulkan::functions::loadDeviceFunctions(*tableImpl_, device);

  if (features_.has_VK_KHR_buffer_device_address && vf_.vkGetBufferDeviceAddressKHR == nullptr) {
    return Result(Result::Code::InvalidOperation, "Cannot initialize VK_KHR_buffer_device_address");
  }

  vf_.vkGetDeviceQueue(
      device, deviceQueues_.graphicsQueueFamilyIndex, 0, &deviceQueues_.graphicsQueue);
  vf_.vkGetDeviceQueue(
      device, deviceQueues_.computeQueueFamilyIndex, 0, &deviceQueues_.computeQueue);

  vkDevice_ = device;

  VK_ASSERT(ivkSetDebugObjectName(&vf_,
                                  vkDevice_,
                                  VK_OBJECT_TYPE_DEVICE,
                                  (uint64_t)vkDevice_,
                                  IGL_FORMAT("Device: VulkanContext::device_ {}",
                                             debugName ? debugName : "igl/vulkan/VulkanContext.cpp")
                                      .c_str()));

  VK_ASSERT(ivkSetDebugObjectName(
      &vf_,
      vkDevice_,
      VK_OBJECT_TYPE_QUEUE,
      (uint64_t)deviceQueues_.graphicsQueue,
      IGL_FORMAT("Graphics queue: {}", debugName ? debugName : "igl/vulkan/VulkanContext.cpp")
          .c_str()));
  VK_ASSERT(ivkSetDebugObjectName(
      &vf_,
      vkDevice_,
      VK_OBJECT_TYPE_QUEUE,
      (uint64_t)deviceQueues_.computeQueue,
      IGL_FORMAT("Compute queue: {}", debugName ? debugName : "igl/vulkan/VulkanContext.cpp")
          .c_str()));

  immediate_ = std::make_unique<VulkanImmediateCommands>(vf_,
                                                         device,
                                                         deviceQueues_.graphicsQueueFamilyIndex,
                                                         config_.exportableFences,
                                                         features_.has_VK_KHR_timeline_semaphore &&
                                                             features_.has_VK_KHR_synchronization2,
                                                         "VulkanContext::immediate_");
  IGL_DEBUG_ASSERT(config_.maxResourceCount > 0,
                   "Max resource count needs to be greater than zero");
  syncSubmitHandles_.resize(config_.maxResourceCount);

  // create Vulkan pipeline cache
  {
    const VkPipelineCacheCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .flags = VkPipelineCacheCreateFlags(0),
        .initialDataSize = config_.pipelineCacheDataSize,
        .pInitialData = config_.pipelineCacheData,
    };
    vf_.vkCreatePipelineCache(device, &ci, nullptr, &pipelineCache_);
  }

  // Create Vulkan Memory Allocator
  if (IGL_VULKAN_USE_VMA) {
    VK_ASSERT_RETURN(
        ivkVmaCreateAllocator(&vf_,
                              vkPhysicalDevice_,
                              vkDevice_,
                              vkInstance_,
                              apiVersion > VK_API_VERSION_1_3 ? VK_API_VERSION_1_3 : apiVersion,
                              features_.has_VK_KHR_buffer_device_address,
                              (VkDeviceSize)config_.vmaPreferredLargeHeapBlockSize,
                              &pimpl_->vma));
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
    if (!IGL_DEBUG_VERIFY(result.isOk())) {
      return result;
    }
    if (!IGL_DEBUG_VERIFY(image.valid())) {
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
    if (!IGL_DEBUG_VERIFY(imageView.valid())) {
      return Result(Result::Code::InvalidOperation, "Cannot create VulkanImageView");
    }
    pimpl_->dummyTexture =
        textures_.create(std::make_shared<VulkanTexture>(std::move(image), std::move(imageView)));
    IGL_DEBUG_ASSERT(textures_.numObjects() == 1);
    const uint32_t pixel = 0xFF000000;

    const VkImageAspectFlags imageAspectFlags =
        (*textures_.get(pimpl_->dummyTexture))->imageView_.getVkImageAspectFlags();
    stagingDevice_->imageData(
        (*textures_.get(pimpl_->dummyTexture))->image_,
        TextureType::TwoD,
        TextureRangeDesc::new2D(0, 0, 1, 1),
        TextureFormatProperties::fromTextureFormat(TextureFormat::RGBA_UNorm8),
        0,
        imageAspectFlags,
        &pixel);
  }

  // default sampler
  pimpl_->dummySampler = createSampler(
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
      VK_FORMAT_UNDEFINED,
      nullptr,
      "Sampler: default");
  IGL_DEBUG_ASSERT(samplers_.numObjects() == 1);

  growBindlessDescriptorPool(pimpl_->currentMaxBindlessTextures,
                             pimpl_->currentMaxBindlessSamplers);

  querySurfaceCapabilities();

#if defined(IGL_WITH_TRACY_GPU)
  profilingCommandPool_ = std::make_unique<VulkanCommandPool>(
      vf_,
      vkDevice_,
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      deviceQueues_.graphicsQueueFamilyIndex,
      "VulkanContext::profilingCommandPool_ (Tracy)");

  profilingCommandBuffer_ = VK_NULL_HANDLE;
  VK_ASSERT(ivkAllocateCommandBuffer(
      &vf_, vkDevice_, profilingCommandPool_->getVkCommandPool(), &profilingCommandBuffer_));

#if defined(VK_EXT_calibrated_timestamps)
  if (features_.enabled(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME)) {
    tracyCtx_ = TracyVkContextCalibrated(vkInstance_,
                                         getVkPhysicalDevice(),
                                         getVkDevice(),
                                         deviceQueues_.graphicsQueue,
                                         profilingCommandBuffer_,
                                         tableImpl_->vkGetInstanceProcAddr,
                                         tableImpl_->vkGetDeviceProcAddr);
  }
#endif // VK_EXT_calibrated_timestamps
  // If VK_EXT_calibrated_timestamps is not available or it has not been enabled, use the
  // uncalibrated Tracy context
  if (!tracyCtx_) {
    tracyCtx_ = TracyVkContext(vkInstance_,
                               getVkPhysicalDevice(),
                               getVkDevice(),
                               deviceQueues_.graphicsQueue,
                               profilingCommandBuffer_,
                               tableImpl_->vkGetInstanceProcAddr,
                               tableImpl_->vkGetDeviceProcAddr);
  }

  IGL_DEBUG_ASSERT(tracyCtx_, "Failed to create Tracy GPU profiling context");
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

  pimpl_->currentMaxBindlessTextures = newMaxTextures;
  pimpl_->currentMaxBindlessSamplers = newMaxSamplers;

#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("growBindlessDescriptorPool(%u, %u)\n", newMaxTextures, newMaxSamplers);
#endif // IGL_VULKAN_PRINT_COMMANDS

  // macOS: MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS is required when using this with MoltenVK
  IGL_DEBUG_ASSERT(
      newMaxTextures <= vkPhysicalDeviceDescriptorIndexingProperties_
                            .maxDescriptorSetUpdateAfterBindSampledImages,
      "Max Textures exceeded: %u (hardware max %u)",
      newMaxTextures,
      vkPhysicalDeviceDescriptorIndexingProperties_.maxDescriptorSetUpdateAfterBindSampledImages);

  // macOS: MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS is required when using this with MoltenVK
  IGL_DEBUG_ASSERT(
      newMaxSamplers <=
          vkPhysicalDeviceDescriptorIndexingProperties_.maxDescriptorSetUpdateAfterBindSamplers,
      "Max Samplers exceeded %u (hardware max %u)",
      newMaxSamplers,
      vkPhysicalDeviceDescriptorIndexingProperties_.maxDescriptorSetUpdateAfterBindSamplers);

  VkDevice device = getVkDevice();

  if (pimpl_->dpBindless != VK_NULL_HANDLE) {
    deferredTask(std::packaged_task<void()>([vf = &vf_, device, dp = pimpl_->dpBindless]() {
      vf->vkDestroyDescriptorPool(device, dp, nullptr);
    }));
  }

  // create default descriptor set layout which is going to be shared by graphics pipelines
  constexpr uint32_t kNumBindings = 7;
  constexpr VkShaderStageFlags stageFlags =
      VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
  const std::array<VkDescriptorSetLayoutBinding, kNumBindings> bindings = {
      ivkGetDescriptorSetLayoutBinding(kBinding_Texture2D,
                                       VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                       pimpl_->currentMaxBindlessTextures,
                                       stageFlags),
      ivkGetDescriptorSetLayoutBinding(kBinding_Texture2DArray,
                                       VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                       pimpl_->currentMaxBindlessTextures,
                                       stageFlags),
      ivkGetDescriptorSetLayoutBinding(kBinding_Texture3D,
                                       VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                       pimpl_->currentMaxBindlessTextures,
                                       stageFlags),
      ivkGetDescriptorSetLayoutBinding(kBinding_TextureCube,
                                       VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                       pimpl_->currentMaxBindlessTextures,
                                       stageFlags),
      ivkGetDescriptorSetLayoutBinding(kBinding_Sampler,
                                       VK_DESCRIPTOR_TYPE_SAMPLER,
                                       pimpl_->currentMaxBindlessSamplers,
                                       stageFlags),
      ivkGetDescriptorSetLayoutBinding(kBinding_SamplerShadow,
                                       VK_DESCRIPTOR_TYPE_SAMPLER,
                                       pimpl_->currentMaxBindlessSamplers,
                                       stageFlags),
      ivkGetDescriptorSetLayoutBinding(kBinding_StorageImages,
                                       VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                       pimpl_->currentMaxBindlessTextures,
                                       stageFlags),
  };
  const uint32_t flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                         VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
  const std::array<VkDescriptorBindingFlags, kNumBindings> bindingFlags = {
      flags, flags, flags, flags, flags, flags, flags};
  IGL_DEBUG_ASSERT(bindingFlags.back() == flags);
  pimpl_->dslBindless = std::make_unique<VulkanDescriptorSetLayout>(
      *this,
      VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
      kNumBindings,
      bindings.data(),
      bindingFlags.data(),
      "Descriptor Set Layout: VulkanContext::dslBindless_");
  // create default descriptor pool and allocate 1 descriptor set
  const std::array<VkDescriptorPoolSize, kNumBindings> poolSizes = {
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, pimpl_->currentMaxBindlessTextures},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, pimpl_->currentMaxBindlessTextures},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, pimpl_->currentMaxBindlessTextures},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, pimpl_->currentMaxBindlessTextures},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, pimpl_->currentMaxBindlessSamplers},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, pimpl_->currentMaxBindlessSamplers},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, pimpl_->currentMaxBindlessTextures},
  };
  VK_ASSERT(ivkCreateDescriptorPool(&vf_,
                                    device,
                                    VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                                    1,
                                    static_cast<uint32_t>(poolSizes.size()),
                                    poolSizes.data(),
                                    &pimpl_->dpBindless));
  VK_ASSERT(ivkSetDebugObjectName(&vf_,
                                  device,
                                  VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                                  (uint64_t)pimpl_->dpBindless,
                                  "Descriptor Pool: dpBindless_"));
  VK_ASSERT(ivkAllocateDescriptorSet(&vf_,
                                     device,
                                     pimpl_->dpBindless,
                                     pimpl_->dslBindless->getVkDescriptorSetLayout(),
                                     &pimpl_->dsBindless));
  VK_ASSERT(ivkSetDebugObjectName(&vf_,
                                  device,
                                  VK_OBJECT_TYPE_DESCRIPTOR_SET,
                                  (uint64_t)pimpl_->dsBindless,
                                  "Descriptor Set: dsBindless_"));
}

igl::Result VulkanContext::initSwapchain(uint32_t width, uint32_t height) {
  IGL_PROFILER_FUNCTION();

  if (!vkDevice_ || !immediate_) {
    IGL_LOG_ERROR("Call initContext() first");
    return Result(Result::Code::Unsupported, "Call initContext() first");
  }

  if (swapchain_) {
    vf_.vkDeviceWaitIdle(vkDevice_);
    swapchain_ = nullptr; // Destroy old swapchain first
  }

  if (!width || !height) {
    return Result();
  }

  swapchain_ = std::make_unique<igl::vulkan::VulkanSwapchain>(*this, width, height);

  if (features_.has_VK_KHR_timeline_semaphore && features_.has_VK_KHR_synchronization2) {
    timelineSemaphore_ = std::make_unique<VulkanSemaphore>(
        vf_, getVkDevice(), 0, false, "Semaphore: VulkanContext::timelineSemaphore_");
  }

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
                                                          igl::Result* IGL_NULLABLE outResult,
                                                          const char* IGL_NULLABLE
                                                              debugName) const {
  IGL_PROFILER_FUNCTION();

#define ENSURE_BUFFER_SIZE(flag, maxSize)                                                      \
  if (usageFlags & flag) {                                                                     \
    if (!IGL_DEBUG_VERIFY(bufferSize <= maxSize)) {                                            \
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
      *this, vkDevice_, bufferSize, usageFlags, memFlags, debugName);
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
                                       igl::Result* IGL_NULLABLE outResult,
                                       const char* IGL_NULLABLE debugName) const {
  IGL_PROFILER_FUNCTION();

  if (!validateImageLimits(
          imageType, samples, extent, getVkPhysicalDeviceProperties().limits, outResult)) {
    return VulkanImage();
  }

  return {*this,
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
          debugName};
}

// @fb-only
// @fb-only
    // @fb-only
    // @fb-only
    // @fb-only
    // @fb-only
    // @fb-only
    // @fb-only
    // @fb-only
    // @fb-only
    // @fb-only
    // @fb-only
    // @fb-only
    // @fb-only
    // @fb-only
  // @fb-only
          // @fb-only
          // @fb-only
          // @fb-only
          // @fb-only
          // @fb-only
    // @fb-only
  // @fb-only
  // @fb-only
                                       // @fb-only
                                       // @fb-only
                                       // @fb-only
                                       // @fb-only
                                       // @fb-only
                                       // @fb-only
                                       // @fb-only
                                       // @fb-only
                                       // @fb-only
                                       // @fb-only
                                       // @fb-only
                                       // @fb-only
                                       // @fb-only
// @fb-only
// @fb-only

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
    igl::Result* IGL_NULLABLE outResult,
    const char* IGL_NULLABLE debugName) const {
  if (!validateImageLimits(
          imageType, samples, extent, getVkPhysicalDeviceProperties().limits, outResult)) {
    return nullptr;
  }

  return std::make_unique<VulkanImage>(*this,
                                       fileDescriptor,
                                       memoryAllocationSize,
                                       vkDevice_,
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

void VulkanContext::pruneTextures() {
  // here we remove deleted textures - everything which has only 1 reference is owned by this
  // context and can be released safely

  // textures
  {
    for (uint32_t i = 1; i < (uint32_t)textures_.objects_.size(); i++) {
      if (textures_.objects_[i].obj_ && textures_.objects_[i].obj_.use_count() == 1) {
        textures_.destroy(i);
      }
    }
  }
}

VkResult VulkanContext::checkAndUpdateDescriptorSets() {
  if (!awaitingCreation_) {
    // nothing to update here
    return VK_SUCCESS;
  }

  // newly created resources can be used immediately - make sure they are put into descriptor sets
  IGL_PROFILER_FUNCTION();

  pruneTextures();

  // update Vulkan bindless descriptor sets here
  if (!config_.enableDescriptorIndexing) {
    return VK_SUCCESS;
  }

  uint32_t newMaxTextures = pimpl_->currentMaxBindlessTextures;
  uint32_t newMaxSamplers = pimpl_->currentMaxBindlessSamplers;

  while (textures_.objects_.size() > newMaxTextures) {
    newMaxTextures *= 2;
  }
  while (samplers_.objects_.size() > newMaxSamplers) {
    newMaxSamplers *= 2;
  }
  if (newMaxTextures != pimpl_->currentMaxBindlessTextures ||
      newMaxSamplers != pimpl_->currentMaxBindlessSamplers) {
    growBindlessDescriptorPool(newMaxTextures, newMaxSamplers);
  }

  // make sure the guard values are always there
  IGL_DEBUG_ASSERT(!textures_.objects_.empty());
  IGL_DEBUG_ASSERT(!samplers_.objects_.empty());

  // 1. Sampled and storage images
  std::vector<VkDescriptorImageInfo> infoSampledImages;
  std::vector<VkDescriptorImageInfo> infoStorageImages;
  infoSampledImages.reserve(textures_.objects_.size());
  infoStorageImages.reserve(textures_.objects_.size());

  // use the dummy texture/sampler to avoid sparse array
  VkImageView dummyImageView = textures_.objects_[0].obj_->imageView_.getVkImageView();
  VkSampler dummySampler = samplers_.objects_[0].obj_.vkSampler;

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
    IGL_DEBUG_ASSERT(infoSampledImages.back().imageView != VK_NULL_HANDLE);
    IGL_DEBUG_ASSERT(infoStorageImages.back().imageView != VK_NULL_HANDLE);
  }

  // 2. Samplers
  std::vector<VkDescriptorImageInfo> infoSamplers;
  infoSamplers.reserve(samplers_.objects_.size());

  for (const auto& entry : samplers_.objects_) {
    const VulkanSampler* sampler = &entry.obj_;
    infoSamplers.push_back(
        {sampler ? sampler->vkSampler : dummySampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED});
  }

  std::vector<VkWriteDescriptorSet> write;

  if (!infoSampledImages.empty()) {
    // use the same indexing for every texture type
    for (uint32_t i = kBinding_Texture2D; i != kBinding_TextureCube + 1; i++) {
      write.push_back(ivkGetWriteDescriptorSet_ImageInfo(pimpl_->dsBindless,
                                                         i,
                                                         VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                         (uint32_t)infoSampledImages.size(),
                                                         infoSampledImages.data()));
    }
  };

  if (!infoSamplers.empty()) {
    for (uint32_t i = kBinding_Sampler; i != kBinding_SamplerShadow + 1; i++) {
      write.push_back(ivkGetWriteDescriptorSet_ImageInfo(pimpl_->dsBindless,
                                                         i,
                                                         VK_DESCRIPTOR_TYPE_SAMPLER,
                                                         (uint32_t)infoSamplers.size(),
                                                         infoSamplers.data()));
    }
  }

  if (!infoStorageImages.empty()) {
    write.push_back(ivkGetWriteDescriptorSet_ImageInfo(pimpl_->dsBindless,
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
    VK_ASSERT(immediate_->wait(immediate_->getLastSubmitHandle()));
    vf_.vkUpdateDescriptorSets(
        vkDevice_, static_cast<uint32_t>(write.size()), write.data(), 0, nullptr);
  }

  awaitingCreation_ = false;
  return VK_SUCCESS;
}

std::shared_ptr<VulkanTexture> VulkanContext::createTexture(
    VulkanImage&& image,
    VulkanImageView&& imageView,
    [[maybe_unused]] const char* IGL_NULLABLE debugName) const {
  IGL_PROFILER_FUNCTION();

  const TextureHandle handle =
      textures_.create(std::make_shared<VulkanTexture>(std::move(image), std::move(imageView)));

  auto texture = *textures_.get(handle);

  if (!IGL_DEBUG_VERIFY(texture)) {
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
    const char* IGL_NULLABLE debugName) const {
  auto iglImage = VulkanImage(*this, vkDevice_, vkImage, imageCreateInfo, debugName);
  auto imageView = iglImage.createImageView(imageViewCreateInfo, debugName);
  return createTexture(std::move(iglImage), std::move(imageView), debugName);
}

SamplerHandle VulkanContext::createSampler(const VkSamplerCreateInfo& ci,
                                           VkFormat yuvVkFormat,
                                           igl::Result* IGL_NULLABLE outResult,
                                           const char* IGL_NULLABLE debugName) const {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);

  VkSamplerCreateInfo cInfo = ci;
  VkSamplerYcbcrConversionInfo conversionInfo{};

  if (yuvVkFormat != VK_FORMAT_UNDEFINED) {
    conversionInfo = getOrCreateYcbcrConversionInfo(yuvVkFormat);
    cInfo.pNext = &conversionInfo;
    // must be CLAMP_TO_EDGE
    // https://vulkan.lunarg.com/doc/view/1.3.268.0/windows/1.3-extensions/vkspec.html#VUID-VkSamplerCreateInfo-addressModeU-01646
    cInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    cInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    cInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    cInfo.anisotropyEnable = VK_FALSE;
    cInfo.unnormalizedCoordinates = VK_FALSE;
  }

  VkDevice device = getVkDevice();
  VulkanSampler sampler;
  VK_ASSERT(vf_.vkCreateSampler(device, &cInfo, nullptr, &sampler.vkSampler));
  VK_ASSERT(ivkSetDebugObjectName(
      &vf_, device, VK_OBJECT_TYPE_SAMPLER, (uint64_t)sampler.vkSampler, debugName));
  const SamplerHandle handle = samplers_.create(static_cast<VulkanSampler&&>(sampler));

  samplers_.get(handle)->samplerId = handle.index();

  awaitingCreation_ = true;

  return handle;
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

    uint32_t formatCount = 0;
    vf_.vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_, vkSurface_, &formatCount, nullptr);

    if (formatCount) {
      deviceSurfaceFormats_.resize(formatCount);
      vf_.vkGetPhysicalDeviceSurfaceFormatsKHR(
          vkPhysicalDevice_, vkSurface_, &formatCount, deviceSurfaceFormats_.data());
    }

    uint32_t presentModeCount = 0;
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
  IGL_DEBUG_ASSERT(!deviceDepthFormats_.empty());
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
  builder.build(vf_, vkDevice_, &pass);

  const size_t index = renderPasses_.size();

  IGL_DEBUG_ASSERT(index <= 255);

  renderPassesHash_[builder] = uint8_t(index);
  // @fb-only
  // @lint-ignore CLANGTIDY
  renderPasses_.push_back(pass);

  return RenderPassHandle{pass, uint8_t(index)};
}

std::vector<uint8_t> VulkanContext::getPipelineCacheData() const {
  size_t size = 0;
  vf_.vkGetPipelineCacheData(vkDevice_, pipelineCache_, &size, nullptr);

  std::vector<uint8_t> data(size);

  if (size) {
    vf_.vkGetPipelineCacheData(vkDevice_, pipelineCache_, &size, data.data());
  }

  return data;
}

uint64_t VulkanContext::getFrameNumber() const {
  return swapchain_ ? swapchain_->getFrameNumber() : 0u;
}

void VulkanContext::updateBindingsTextures(VkCommandBuffer IGL_NONNULL cmdBuf,
                                           VkPipelineLayout layout,
                                           VkPipelineBindPoint bindPoint,
                                           VulkanImmediateCommands::SubmitHandle nextSubmitHandle,
                                           const BindingsTextures& data,
                                           const VulkanDescriptorSetLayout& dsl,
                                           const util::SpvModuleInfo& info) const {
  IGL_PROFILER_FUNCTION();

  DescriptorPoolsArena& arena = pimpl_->getOrCreateArena_CombinedImageSamplers(
      *this, dsl.getVkDescriptorSetLayout(), dsl.numBindings_);

  VkDescriptorSet dset = arena.getNextDescriptorSet(*immediate_, nextSubmitHandle);

  // @fb-only
  VkDescriptorImageInfo infoSampledImages[IGL_TEXTURE_SAMPLERS_MAX]; // uninitialized
  uint32_t numImages = 0;

  // @fb-only
  VkWriteDescriptorSet writes[IGL_TEXTURE_SAMPLERS_MAX]; // uninitialized
  uint32_t numWrites = 0;

  // make sure the guard value is always there
  IGL_DEBUG_ASSERT(!textures_.objects_.empty());
  IGL_DEBUG_ASSERT(!samplers_.objects_.empty());

  // use the dummy texture/sampler to avoid sparse array
  VkImageView dummyImageView = textures_.objects_[0].obj_->imageView_.getVkImageView();
  VkSampler dummySampler = samplers_.objects_[0].obj_.vkSampler;

  const bool isGraphics = bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS;

  for (const util::TextureDescription& d : info.textures) {
    IGL_DEBUG_ASSERT(d.descriptorSet == kBindPoint_CombinedImageSamplers);
    const uint32_t loc = d.bindingLocation;
    IGL_DEBUG_ASSERT(loc < IGL_TEXTURE_SAMPLERS_MAX);
    VkImageView texture = data.textures[loc];
    const bool hasTexture = texture != VK_NULL_HANDLE;
    if (hasTexture && isGraphics) {
      IGL_DEBUG_ASSERT(data.samplers[loc], "A sampler should be bound to every bound texture slot");
    }
    VkSampler sampler = data.samplers[loc] ? data.samplers[loc] : dummySampler;
    writes[numWrites++] = ivkGetWriteDescriptorSet_ImageInfo(
        dset, loc, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &infoSampledImages[numImages]);
    infoSampledImages[numImages++] = VkDescriptorImageInfo{
        .sampler = hasTexture ? sampler : dummySampler,
        .imageView = hasTexture ? texture : dummyImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
  }

  if (numWrites) {
    IGL_PROFILER_ZONE("vkUpdateDescriptorSets()", IGL_PROFILER_COLOR_UPDATE);
    vf_.vkUpdateDescriptorSets(vkDevice_, numWrites, writes, 0, nullptr);
    IGL_PROFILER_ZONE_END();

#if IGL_VULKAN_PRINT_COMMANDS
    IGL_LOG_INFO("%p vkCmdBindDescriptorSets(%u) - textures\n", cmdBuf, bindPoint);
#endif // IGL_VULKAN_PRINT_COMMANDS
    vf_.vkCmdBindDescriptorSets(
        cmdBuf, bindPoint, layout, kBindPoint_CombinedImageSamplers, 1, &dset, 0, nullptr);
  }
}

void VulkanContext::updateBindingsStorageImages(
    VkCommandBuffer IGL_NONNULL cmdBuf,
    VkPipelineLayout layout,
    VkPipelineBindPoint bindPoint,
    VulkanImmediateCommands::SubmitHandle nextSubmitHandle,
    const BindingsStorageImages& data,
    const VulkanDescriptorSetLayout& dsl,
    const util::SpvModuleInfo& info) const {
  IGL_PROFILER_FUNCTION();

  DescriptorPoolsArena& arena = pimpl_->getOrCreateArena_StorageImages(
      *this, dsl.getVkDescriptorSetLayout(), dsl.numBindings_);

  VkDescriptorSet dset = arena.getNextDescriptorSet(*immediate_, nextSubmitHandle);

  // @fb-only
  VkDescriptorImageInfo infoStorageImages[IGL_TEXTURE_SAMPLERS_MAX]; // uninitialized
  uint32_t numStorageImages = 0;

  // @fb-only
  VkWriteDescriptorSet writes[IGL_TEXTURE_SAMPLERS_MAX]; // uninitialized
  uint32_t numWrites = 0;

  // make sure the guard value is always there
  IGL_DEBUG_ASSERT(!textures_.objects_.empty());

  // use the dummy texture to avoid sparse array
  VkImageView dummyImageView = textures_.objects_[0].obj_->imageView_.getVkImageView();

  for (const util::ImageDescription& d : info.images) {
    IGL_DEBUG_ASSERT(d.descriptorSet == kBindPoint_StorageImages);
    const uint32_t loc = d.bindingLocation;
    IGL_DEBUG_ASSERT(loc < IGL_TEXTURE_SAMPLERS_MAX);
    VkImageView imageView = data.images[loc];
    writes[numWrites++] = ivkGetWriteDescriptorSet_ImageInfo(
        dset, loc, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &infoStorageImages[numStorageImages]);
    infoStorageImages[numStorageImages++] = VkDescriptorImageInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = imageView ? imageView : dummyImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
  }

  if (numWrites) {
    IGL_PROFILER_ZONE("vkUpdateDescriptorSets()", IGL_PROFILER_COLOR_UPDATE);
    vf_.vkUpdateDescriptorSets(vkDevice_, numWrites, writes, 0, nullptr);
    IGL_PROFILER_ZONE_END();

#if IGL_VULKAN_PRINT_COMMANDS
    IGL_LOG_INFO("%p vkCmdBindDescriptorSets(%u) - storage images\n", cmdBuf, bindPoint);
#endif // IGL_VULKAN_PRINT_COMMANDS
    vf_.vkCmdBindDescriptorSets(
        cmdBuf, bindPoint, layout, kBindPoint_StorageImages, 1, &dset, 0, nullptr);
  }
}

void VulkanContext::updateBindingsBuffers(VkCommandBuffer IGL_NONNULL cmdBuf,
                                          VkPipelineLayout layout,
                                          VkPipelineBindPoint bindPoint,
                                          VulkanImmediateCommands::SubmitHandle nextSubmitHandle,
                                          BindingsBuffers& data,
                                          const VulkanDescriptorSetLayout& dsl,
                                          const util::SpvModuleInfo& info) const {
  IGL_PROFILER_FUNCTION();

  DescriptorPoolsArena& arena =
      pimpl_->getOrCreateArena_Buffers(*this, dsl.getVkDescriptorSetLayout(), dsl.numBindings_);

  VkDescriptorSet dset = arena.getNextDescriptorSet(*immediate_, nextSubmitHandle);

  // @fb-only
  VkWriteDescriptorSet writes[IGL_UNIFORM_BLOCKS_BINDING_MAX]; // uninitialized
  uint32_t numWrites = 0;

  for (const util::BufferDescription& b : info.buffers) {
    IGL_DEBUG_ASSERT(b.descriptorSet == kBindPoint_Buffers);
    IGL_DEBUG_ASSERT(
        data.buffers[b.bindingLocation].buffer != VK_NULL_HANDLE,
        IGL_FORMAT("Did you forget to call bindBuffer() for a buffer at the binding location {}?",
                   b.bindingLocation)
            .c_str());
    writes[numWrites++] = ivkGetWriteDescriptorSet_BufferInfo(
        dset,
        b.bindingLocation,
        b.isStorage ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        1,
        &data.buffers[b.bindingLocation]);
  }

  if (numWrites) {
    IGL_PROFILER_ZONE("vkUpdateDescriptorSets()", IGL_PROFILER_COLOR_UPDATE);
    vf_.vkUpdateDescriptorSets(vkDevice_, numWrites, writes, 0, nullptr);
    IGL_PROFILER_ZONE_END();

#if IGL_VULKAN_PRINT_COMMANDS
    IGL_LOG_INFO("%p vkCmdBindDescriptorSets(%u) - buffers\n", cmdBuf, bindPoint);
#endif // IGL_VULKAN_PRINT_COMMANDS
    vf_.vkCmdBindDescriptorSets(
        cmdBuf, bindPoint, layout, kBindPoint_Buffers, 1, &dset, 0, nullptr);
  }
}

void VulkanContext::deferredTask(std::packaged_task<void()>&& task, SubmitHandle handle) const {
  if (handle.empty()) {
    handle = immediate_->getNextSubmitHandle();
  }
  deferredTasks_.emplace_back(std::move(task), handle);
  deferredTasks_.back().frameId_ = this->getFrameNumber();
}

bool VulkanContext::areValidationLayersEnabled() const {
  return config_.enableValidation;
}

void* IGL_NULLABLE VulkanContext::getVmaAllocator() const {
  return pimpl_->vma;
}

void VulkanContext::processDeferredTasks() const {
  IGL_PROFILER_FUNCTION();

  const uint64_t frameId = getFrameNumber();
  constexpr uint64_t kNumWaitFrames = 3u;

  while (!deferredTasks_.empty() && immediate_->isReady(deferredTasks_.front().handle_)) {
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
    immediate_->wait(task.handle_, config_.fenceTimeoutNanoseconds);
    task.task_();
  }
  deferredTasks_.clear();
}

VkFence VulkanContext::getVkFenceFromSubmitHandle(igl::SubmitHandle handle) const noexcept {
  if (handle == 0) {
    IGL_LOG_ERROR("Invalid submit handle passed to getVkFenceFromSubmitHandle");
    return VK_NULL_HANDLE;
  }

  VkFence vkFence =
      immediate_->getVkFenceFromSubmitHandle(VulkanImmediateCommands::SubmitHandle(handle));

  return vkFence;
}

int VulkanContext::getFenceFdFromSubmitHandle(igl::SubmitHandle handle) const noexcept {
  int fenceFd = -1;
#if defined(IGL_PLATFORM_ANDROID) && defined(VK_KHR_external_fence_fd)
  if (handle == 0) {
    IGL_LOG_ERROR("Invalid submit handle passed to getFenceFDFromSubmitHandle");
    return -1;
  }

  const VkFence vkFence = getVkFenceFromSubmitHandle(handle);
  IGL_DEBUG_ASSERT(vkFence != VK_NULL_HANDLE);

  const VkFenceGetFdInfoKHR getFdInfo = {
      .sType = VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR,
      .fence = vkFence,
      .handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT,
  };

  const VkResult result = vf_.vkGetFenceFdKHR(vkDevice_, &getFdInfo, &fenceFd);
  if (result != VK_SUCCESS) {
    IGL_LOG_ERROR("Unable to get fence fd from submit handle: %lu", handle);
  }
  immediate_->storeFDInSubmitHandle(VulkanImmediateCommands::SubmitHandle(handle), fenceFd);
#endif // defined(IGL_PLATFORM_ANDROID)
  return fenceFd;
}

VkDescriptorSetLayout VulkanContext::getBindlessVkDescriptorSetLayout() const {
  return config_.enableDescriptorIndexing ? pimpl_->dslBindless->getVkDescriptorSetLayout()
                                          : VK_NULL_HANDLE;
}

VkDescriptorSet VulkanContext::getBindlessVkDescriptorSet() const {
  return config_.enableDescriptorIndexing ? pimpl_->dsBindless : VK_NULL_HANDLE;
}

VkSamplerYcbcrConversionInfo VulkanContext::getOrCreateYcbcrConversionInfo(VkFormat format) const {
  auto it = ycbcrConversionInfos_.find(format);

  if (it != ycbcrConversionInfos_.end()) {
    return it->second;
  }

  if (!IGL_DEBUG_VERIFY(features_.featuresSamplerYcbcrConversion.samplerYcbcrConversion)) {
    IGL_DEBUG_ABORT("Ycbcr samplers are not supported");
    return {};
  }

  VkFormatProperties props;
  vf_.vkGetPhysicalDeviceFormatProperties(getVkPhysicalDevice(), format, &props);

  const bool cosited =
      (props.optimalTilingFeatures & VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT) != 0;
  const bool midpoint =
      (props.optimalTilingFeatures & VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT) != 0;

  if (!IGL_DEBUG_VERIFY(cosited || midpoint)) {
    IGL_DEBUG_ASSERT(cosited || midpoint, "Unsupported Ycbcr feature");
    return {};
  }

  const VkSamplerYcbcrConversionCreateInfo ciYcbcr = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
      .format = format,
      .ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709,
      .ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_FULL,
      .components =
          {
              VK_COMPONENT_SWIZZLE_IDENTITY,
              VK_COMPONENT_SWIZZLE_IDENTITY,
              VK_COMPONENT_SWIZZLE_IDENTITY,
              VK_COMPONENT_SWIZZLE_IDENTITY,
          },
      .xChromaOffset = midpoint ? VK_CHROMA_LOCATION_MIDPOINT : VK_CHROMA_LOCATION_COSITED_EVEN,
      .yChromaOffset = midpoint ? VK_CHROMA_LOCATION_MIDPOINT : VK_CHROMA_LOCATION_COSITED_EVEN,
      .chromaFilter = VK_FILTER_LINEAR,
      .forceExplicitReconstruction = VK_FALSE,
  };

  VkSamplerYcbcrConversionInfo info = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
      .conversion = VK_NULL_HANDLE,
  };
  vf_.vkCreateSamplerYcbcrConversion(getVkDevice(), &ciYcbcr, nullptr, &info.conversion);

  // check properties
  VkSamplerYcbcrConversionImageFormatProperties samplerYcbcrConversionImageFormatProps = {
      VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES, nullptr, 0};
  VkImageFormatProperties2 imageFormatProps = {
      VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2, &samplerYcbcrConversionImageFormatProps, {}};
  const VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
      nullptr,
      format,
      VK_IMAGE_TYPE_2D,
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_IMAGE_CREATE_DISJOINT_BIT,
  };
  vf_.vkGetPhysicalDeviceImageFormatProperties2(
      getVkPhysicalDevice(), &imageFormatInfo, &imageFormatProps);

  IGL_DEBUG_ASSERT(samplerYcbcrConversionImageFormatProps.combinedImageSamplerDescriptorCount <= 3);

  ycbcrConversionInfos_[format] = info;

  return info;
}

void VulkanContext::freeResourcesForDescriptorSetLayout(VkDescriptorSetLayout dsl) const {
  pimpl_->arenaBuffers.erase(dsl);
  pimpl_->arenaCombinedImageSamplers.erase(dsl);
  pimpl_->arenaStorageImages.erase(dsl);
}

igl::BindGroupTextureHandle VulkanContext::createBindGroup(const BindGroupTextureDesc& desc,
                                                           const IRenderPipelineState* IGL_NULLABLE
                                                               compatiblePipeline,
                                                           Result* IGL_NULLABLE outResult) {
  VkDevice device = getVkDevice();

  BindGroupMetadataTextures metadata{desc};

  // @fb-only
  VkDescriptorSetLayoutBinding bindings[IGL_TEXTURE_SAMPLERS_MAX]; // uninitialized
  uint32_t numBindings = 0;

  const VkShaderStageFlags stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  const uint32_t usageMaskPipeline =
      compatiblePipeline ? static_cast<const igl::vulkan::RenderPipelineState&>(*compatiblePipeline)
                               .getSpvModuleInfo()
                               .usageMaskTextures
                         : 0ul;

  for (uint32_t loc = 0; loc != IGL_ARRAY_NUM_ELEMENTS(desc.textures); loc++) {
    const bool isInPipeline = (usageMaskPipeline & (1ul << loc)) != 0;
    if (compatiblePipeline ? isInPipeline : desc.samplers[loc] != nullptr) {
      IGL_DEBUG_ASSERT(compatiblePipeline || desc.samplers[loc]);
      bindings[numBindings++] = ivkGetDescriptorSetLayoutBinding(
          loc, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, stageFlags);
      metadata.usageMask |= 1ul << loc;
    }
  }

  VkDescriptorSetLayout dsl = VK_NULL_HANDLE;

  {
    // @fb-only
    const VkDescriptorBindingFlags bindingFlags[IGL_TEXTURE_SAMPLERS_MAX] = {};

    VK_ASSERT(ivkCreateDescriptorSetLayout(&vf_,
                                           device,
                                           VkDescriptorSetLayoutCreateFlags{},
                                           numBindings,
                                           bindings,
                                           bindingFlags,
                                           &dsl));
    VK_ASSERT(ivkSetDebugObjectName(
        &vf_,
        device,
        VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
        (uint64_t)dsl,
        IGL_FORMAT("Descriptor Set Layout (COMBINED_IMAGE_SAMPLER): BindGroup = {}", desc.debugName)
            .c_str()));

    const VkDescriptorPoolSize poolSize =
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numBindings};

    VK_ASSERT(ivkCreateDescriptorPool(
        &vf_, device, VkDescriptorPoolCreateFlags{}, 1u, 1u, &poolSize, &metadata.pool));
    VK_ASSERT(ivkSetDebugObjectName(
        &vf_,
        device,
        VK_OBJECT_TYPE_DESCRIPTOR_POOL,
        (uint64_t)metadata.pool,
        IGL_FORMAT("Descriptor Pool (COMBINED_IMAGE_SAMPLER): BindGroup = {}", desc.debugName)
            .c_str()));

    VK_ASSERT(ivkAllocateDescriptorSet(&vf_, device, metadata.pool, dsl, &metadata.dset));
  }

  // make sure the guard values are always there
  IGL_DEBUG_ASSERT(!textures_.objects_.empty());
  IGL_DEBUG_ASSERT(!samplers_.objects_.empty());
  // use the dummy texture to ensure pipeline compatibility
  VkImageView dummyImageView = textures_.objects_[0].obj_->imageView_.getVkImageView();

  // @fb-only
  VkDescriptorImageInfo images[IGL_TEXTURE_SAMPLERS_MAX]; // uninitialized
  // @fb-only
  VkWriteDescriptorSet writes[IGL_TEXTURE_SAMPLERS_MAX]; // uninitialized
  uint32_t numWrites = 0;

  for (uint32_t loc = 0; loc != IGL_ARRAY_NUM_ELEMENTS(desc.textures); loc++) {
    if (compatiblePipeline ? (usageMaskPipeline & (1ul << loc)) == 0
                           : desc.textures[loc] == nullptr) {
      continue;
    }
    const igl::vulkan::VulkanTexture& texture =
        desc.textures[loc]
            ? static_cast<igl::vulkan::Texture*>(desc.textures[loc].get())->getVulkanTexture()
            : *textures_.objects_[0].obj_; // use a dummy texture when necessary
    const igl::vulkan::VulkanSampler& sampler =
        desc.samplers[loc]
            ? *samplers_.get(static_cast<igl::vulkan::SamplerState&>(*desc.samplers[loc]).sampler_)
            : samplers_.objects_[0].obj_; // use a dummy sampler when necessary

    // multisampled images cannot be directly accessed from shaders
    const bool isTextureAvailable =
        (texture.image_.samples_ & VK_SAMPLE_COUNT_1_BIT) == VK_SAMPLE_COUNT_1_BIT;
    const bool isSampledImage = isTextureAvailable && texture.image_.isSampledImage();

    if (!IGL_DEBUG_VERIFY(isSampledImage)) {
      IGL_LOG_ERROR("Each bound texture should have TextureUsageBits::Sampled (slot = %u)", loc);
      continue;
    }

    writes[numWrites] = ivkGetWriteDescriptorSet_ImageInfo(
        metadata.dset, loc, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &images[numWrites]);
    images[numWrites++] = {
        sampler.vkSampler,
        isSampledImage ? texture.imageView_.getVkImageView() : dummyImageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
  }

  if (!IGL_DEBUG_VERIFY(numWrites)) {
    IGL_LOG_ERROR("Cannot create an empty bind group");
    Result::setResult(outResult,
                      Result(Result::Code::RuntimeError, "Cannot create an empty bind group"));
    return {};
  }

  IGL_PROFILER_ZONE("vkUpdateDescriptorSets() - textures bind group", IGL_PROFILER_COLOR_UPDATE);
  vf_.vkUpdateDescriptorSets(vkDevice_, numWrites, writes, 0, nullptr);
  IGL_PROFILER_ZONE_END();

  // once a descriptor set has been updated, destroy the DSL
  vf_.vkDestroyDescriptorSetLayout(device, dsl, nullptr);

  Result::setOk(outResult);

  return pimpl_->bindGroupTexturesPool.create(std::move(metadata));
}

igl::BindGroupBufferHandle VulkanContext::createBindGroup(const BindGroupBufferDesc& desc,
                                                          Result* outResult) {
  VkDevice device = getVkDevice();

  BindGroupMetadataBuffers metadata{desc};

  // @fb-only
  VkDescriptorSetLayoutBinding bindings[IGL_UNIFORM_BLOCKS_BINDING_MAX]; // uninitialized
  uint32_t numBindings = 0;

  // @fb-only
  VkDescriptorPoolSize poolSizes[] = {
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 0},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0},
  };

  const VkShaderStageFlags stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  for (uint32_t loc = 0; loc != IGL_ARRAY_NUM_ELEMENTS(desc.buffers); loc++) {
    if (!desc.buffers[loc]) {
      continue;
    }
    auto* buf = static_cast<igl::vulkan::Buffer*>(desc.buffers[loc].get());
    const bool isDynamic = (desc.isDynamicBufferMask & (1ul << loc)) != 0;
    const bool isUniform = ((buf->getBufferType() & BufferDesc::BufferTypeBits::Uniform) != 0);
    const VkDescriptorType type =
        isUniform
            ? (isDynamic
                   ? (poolSizes[0].descriptorCount++, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                   : (poolSizes[1].descriptorCount++, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER))
            : (isDynamic
                   ? (poolSizes[2].descriptorCount++, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
                   : (poolSizes[3].descriptorCount++, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
    if (isDynamic && !desc.size[loc]) {
      IGL_LOG_ERROR(
          "A buffer at the binding location '%u' is marked as dynamic but the corresponding size "
          "value is 0. You have to specify the binding size for all dynamic buffers.",
          loc);
    }
    if (desc.offset[loc]) {
      const auto& limits = getVkPhysicalDeviceProperties().limits;
      const uint32_t alignment =
          static_cast<uint32_t>(isUniform ? limits.minUniformBufferOffsetAlignment
                                          : limits.minStorageBufferOffsetAlignment);
      if (!IGL_DEBUG_VERIFY((alignment == 0) || (desc.offset[loc] % alignment == 0))) {
        IGL_LOG_ERROR(
            "`desc.offset[loc] = %u` must be a multiple of `VkPhysicalDeviceLimits::%s = %u`",
            static_cast<uint32_t>(desc.offset[loc]),
            isUniform ? "minUniformBufferOffsetAlignment" : "minStorageBufferOffsetAlignment",
            alignment);
      }
    }
    bindings[numBindings++] = ivkGetDescriptorSetLayoutBinding(loc, type, 1, stageFlags);
    metadata.usageMask |= 1ul << loc;
  }

  // construct a dense array of non-zero VkDescriptorPoolSize elements
  qsort(poolSizes,
        IGL_ARRAY_NUM_ELEMENTS(poolSizes),
        sizeof(VkDescriptorPoolSize),
        [](const void* a, const void* b) {
          return ((VkDescriptorPoolSize*)a)->descriptorCount <
                         ((VkDescriptorPoolSize*)b)->descriptorCount
                     ? 1
                     : 0;
        });
  uint32_t numPoolSizes = 0;
  while (numPoolSizes < IGL_ARRAY_NUM_ELEMENTS(poolSizes) &&
         poolSizes[numPoolSizes].descriptorCount > 0) {
    numPoolSizes++;
  }
  IGL_DEBUG_ASSERT(numPoolSizes);

  VkDescriptorSetLayout dsl = VK_NULL_HANDLE;

  {
    // @fb-only
    const VkDescriptorBindingFlags bindingFlags[IGL_UNIFORM_BLOCKS_BINDING_MAX] = {};

    VK_ASSERT(ivkCreateDescriptorSetLayout(&vf_,
                                           device,
                                           VkDescriptorSetLayoutCreateFlags{},
                                           numBindings,
                                           bindings,
                                           bindingFlags,
                                           &dsl));
    VK_ASSERT(ivkSetDebugObjectName(
        &vf_,
        device,
        VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
        (uint64_t)dsl,
        IGL_FORMAT("Descriptor Set Layout (BUFFERS): BindGroup = {}", desc.debugName).c_str()));

    VK_ASSERT(ivkCreateDescriptorPool(
        &vf_, device, VkDescriptorPoolCreateFlags{}, 1u, numPoolSizes, poolSizes, &metadata.pool));
    VK_ASSERT(ivkSetDebugObjectName(
        &vf_,
        device,
        VK_OBJECT_TYPE_DESCRIPTOR_POOL,
        (uint64_t)metadata.pool,
        IGL_FORMAT("Descriptor Pool (BUFFERS): BindGroup = {}", desc.debugName).c_str()));

    VK_ASSERT(ivkAllocateDescriptorSet(&vf_, device, metadata.pool, dsl, &metadata.dset));
  }

  // @fb-only
  VkDescriptorBufferInfo buffers[IGL_UNIFORM_BLOCKS_BINDING_MAX]; // uninitialized
  // @fb-only
  VkWriteDescriptorSet writes[IGL_UNIFORM_BLOCKS_BINDING_MAX]; // uninitialized
  uint32_t numWrites = 0;

  for (uint32_t loc = 0; loc != IGL_ARRAY_NUM_ELEMENTS(desc.buffers); loc++) {
    if (!desc.buffers[loc]) {
      continue;
    }
    auto* buf = static_cast<igl::vulkan::Buffer*>(desc.buffers[loc].get());
    const bool isDynamic = (desc.isDynamicBufferMask & (1ul << loc)) != 0;
    const bool isUniform = ((buf->getBufferType() & BufferDesc::BufferTypeBits::Uniform) != 0);
    const VkDescriptorType type = isUniform ? (isDynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
                                                         : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                                            : (isDynamic ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
                                                         : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    writes[numWrites] =
        ivkGetWriteDescriptorSet_BufferInfo(metadata.dset, loc, type, 1, &buffers[numWrites]);
    buffers[numWrites++] = VkDescriptorBufferInfo{
        buf->getVkBuffer(),
        desc.offset[loc],
        desc.size[loc] ? desc.size[loc] : VK_WHOLE_SIZE,
    };
  }

  if (!IGL_DEBUG_VERIFY(numWrites)) {
    IGL_LOG_ERROR("Cannot create an empty bind group");
    Result::setResult(outResult,
                      Result(Result::Code::RuntimeError, "Cannot create an empty bind group"));
    return {};
  }

  IGL_PROFILER_ZONE("vkUpdateDescriptorSets() - textures bind group", IGL_PROFILER_COLOR_UPDATE);
  vf_.vkUpdateDescriptorSets(vkDevice_, numWrites, writes, 0, nullptr);
  IGL_PROFILER_ZONE_END();

  // once a descriptor set has been updated, destroy the DSL
  vf_.vkDestroyDescriptorSetLayout(vkDevice_, dsl, nullptr);

  Result::setOk(outResult);

  return pimpl_->bindGroupBuffersPool.create(std::move(metadata));
}

void VulkanContext::destroy(igl::BindGroupTextureHandle handle) {
  if (handle.empty()) {
    return;
  }

  deferredTask(std::packaged_task<void()>(
      [vf = &vf_, device = getVkDevice(), pool = pimpl_->bindGroupTexturesPool.get(handle)->pool] {
        vf->vkDestroyDescriptorPool(device, pool, nullptr);
      }));

  pimpl_->bindGroupTexturesPool.destroy(handle);
}

void VulkanContext::destroy(igl::BindGroupBufferHandle handle) {
  if (handle.empty()) {
    return;
  }

  deferredTask(std::packaged_task<void()>(
      [vf = &vf_, device = getVkDevice(), pool = pimpl_->bindGroupBuffersPool.get(handle)->pool] {
        vf->vkDestroyDescriptorPool(device, pool, nullptr);
      }));

  pimpl_->bindGroupBuffersPool.destroy(handle);
}

void VulkanContext::destroy(igl::SamplerHandle handle) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  if (handle.empty()) {
    return;
  }

  deferredTask(std::packaged_task<void()>(
      [vf = &vf_, device = getVkDevice(), sampler = samplers_.get(handle)->vkSampler]() {
        vf->vkDestroySampler(device, sampler, nullptr);
      }));

  samplers_.destroy(handle);
}

void VulkanContext::destroy(igl::TextureHandle handle) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);

  if (handle.empty()) {
    return;
  }

  textures_.destroy(handle);
}

VkDescriptorSet VulkanContext::getBindGroupDescriptorSet(igl::BindGroupTextureHandle handle) const {
  return handle.valid() ? pimpl_->bindGroupTexturesPool.get(handle)->dset : VK_NULL_HANDLE;
}

uint32_t VulkanContext::getBindGroupUsageMask(igl::BindGroupTextureHandle handle) const {
  return handle.valid() ? pimpl_->bindGroupTexturesPool.get(handle)->usageMask : 0;
}

VkDescriptorSet VulkanContext::getBindGroupDescriptorSet(igl::BindGroupBufferHandle handle) const {
  return handle.valid() ? pimpl_->bindGroupBuffersPool.get(handle)->dset : VK_NULL_HANDLE;
}

uint32_t VulkanContext::getBindGroupUsageMask(igl::BindGroupBufferHandle handle) const {
  return handle.valid() ? pimpl_->bindGroupBuffersPool.get(handle)->usageMask : 0;
}

const VulkanFeatures& VulkanContext::features() const noexcept {
  return features_;
}

void VulkanContext::syncAcquireNext() noexcept {
  IGL_PROFILER_FUNCTION();

  syncCurrentIndex_ = (syncCurrentIndex_ + 1) % config_.maxResourceCount;

  // Wait for the current buffer to become available
  immediate_->wait(syncSubmitHandles_[syncCurrentIndex_], config_.fenceTimeoutNanoseconds);
}

void VulkanContext::syncMarkSubmitted(VulkanImmediateCommands::SubmitHandle handle) noexcept {
  IGL_PROFILER_FUNCTION();

  syncSubmitHandles_[syncCurrentIndex_] = handle;

  syncAcquireNext();
}

void VulkanContext::ensureCurrentContextThread() const {
  IGL_DEBUG_ASSERT(
      pimpl_->contextThread == std::this_thread::get_id(),
      "IGL/Vulkan functions can only be accessed by 1 thread at a time. Call "
      "`setCurrentContextThread()` to mark the current thread as the `owning` thread.");
}

void VulkanContext::setCurrentContextThread() {
  pimpl_->contextThread = std::this_thread::get_id();
}

} // namespace igl::vulkan
