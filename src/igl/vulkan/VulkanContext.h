/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <deque>
#include <future>
#include <memory>
#include <unordered_map>

#include <igl/CommandEncoder.h>
#include <igl/HWDevice.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanExtensions.h>
#include <igl/vulkan/VulkanFunctions.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanImmediateCommands.h>
#include <igl/vulkan/VulkanQueuePool.h>
#include <igl/vulkan/VulkanRenderPassBuilder.h>
#include <igl/vulkan/VulkanStagingDevice.h>

#if defined(IGL_WITH_TRACY_GPU)
#include "tracy/TracyVulkan.hpp"
#endif

namespace igl::vulkan {
namespace util {
struct SpvModuleInfo;
} // namespace util

class Device;
class EnhancedShaderDebuggingStore;
class CommandQueue;
class ComputeCommandEncoder;
class RenderCommandEncoder;
class SyncManager;
class VulkanBuffer;
class VulkanDevice;
class VulkanDescriptorSetLayout;
class VulkanImage;
class VulkanImageView;
class VulkanPipelineLayout;
class VulkanSampler;
class VulkanSemaphore;
class VulkanSwapchain;
class VulkanTexture;

struct BindingsBuffers;
struct BindingsTextures;
struct VulkanContextImpl;
struct VulkanImageCreateInfo;
struct VulkanImageViewCreateInfo;

/*
 * Descriptor sets:
 *  0 - combined image samplers
 *  1 - uniform buffers
 *  2 - storage buffers
 *  3 - bindless textures/samplers  <--  optional
 */
enum {
  kBindPoint_CombinedImageSamplers = 0,
  kBindPoint_BuffersUniform = 1,
  kBindPoint_BuffersStorage = 2,
  kBindPoint_Bindless = 3,
};

struct DeviceQueues {
  const static uint32_t INVALID = 0xFFFFFFFF;
  uint32_t graphicsQueueFamilyIndex = INVALID;
  uint32_t computeQueueFamilyIndex = INVALID;

  VkQueue graphicsQueue = VK_NULL_HANDLE;
  VkQueue computeQueue = VK_NULL_HANDLE;

  DeviceQueues() = default;
};

struct VulkanContextConfig {
  bool terminateOnValidationError = false; // invoke std::terminate() on any validation error

  // enable/disable enhanced shader debugging capabilities (line drawing)
  bool enhancedShaderDebugging = false;

  bool enableConcurrentVkDevicesSupport = false;

  bool enableValidation = true;
  bool enableGPUAssistedValidation = true;
  bool enableSynchronizationValidation = false;
  bool enableBufferDeviceAddress = false;
  bool enableExtraLogs = true;
  bool enableDescriptorIndexing = false;
  // @fb-only

  igl::ColorSpace swapChainColorSpace = igl::ColorSpace::SRGB_NONLINEAR;
  igl::TextureFormat requestedSwapChainTextureFormat = igl::TextureFormat::RGBA_UNorm8;

  std::vector<CommandQueueType> userQueues;

  uint32_t maxResourceCount = 3u;

  // owned by the application - should be alive until initContext() returns
  const void* pipelineCacheData = nullptr;
  size_t pipelineCacheDataSize = 0;

  // This enables fences generated at the end of submission to be exported to the client.
  // The client can then use the SubmitHandle to wait for the completion of the GPU work.
  bool exportableFences = false;

  // Size for VulkanMemoryAllocator's default pool block size parameter.
  // Only relevant if VMA is used for memory allocation.
  // Passing 0 will prompt VMA to a large default value (currently 256 MB).
  // Using a smaller heap size would increase the chance of memory deallocation and result in less
  // memory wastage.
  size_t vmaPreferredLargeHeapBlockSize = 0;
};

class VulkanContext final {
 public:
  VulkanContext(VulkanContextConfig config,
                void* window,
                size_t numExtraInstanceExtensions,
                const char** extraInstanceExtensions,
                void* display = nullptr);
  ~VulkanContext();

  igl::Result queryDevices(const HWDeviceQueryDesc& desc, std::vector<HWDeviceDesc>& outDevices);
  igl::Result initContext(const HWDeviceDesc& desc,
                          size_t numExtraDeviceExtensions = 0,
                          const char** extraDeviceExtensions = nullptr);

  igl::Result initSwapchain(uint32_t width, uint32_t height);
  VkExtent2D getSwapchainExtent() const;

  VulkanImage createImage(VkImageType imageType,
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
                          const char* debugName = nullptr) const;
  std::unique_ptr<VulkanImage> createImageFromFileDescriptor(int32_t fileDescriptor,
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
                                                             const char* debugName = nullptr) const;
  std::unique_ptr<VulkanBuffer> createBuffer(VkDeviceSize bufferSize,
                                             VkBufferUsageFlags usageFlags,
                                             VkMemoryPropertyFlags memFlags,
                                             igl::Result* outResult,
                                             const char* debugName = nullptr) const;
  std::shared_ptr<VulkanTexture> createTexture(VulkanImage&& image,
                                               VulkanImageView&& imageView,
                                               const char* debugName) const;
  std::shared_ptr<VulkanTexture> createTextureFromVkImage(
      VkImage vkImage,
      VulkanImageCreateInfo imageCreateInfo,
      VulkanImageViewCreateInfo imageViewCreateInfo,
      const char* debugName) const;

  std::shared_ptr<VulkanSampler> createSampler(const VkSamplerCreateInfo& ci,
                                               VkFormat yuvVkFormat,
                                               igl::Result* outResult,
                                               const char* debugName = nullptr) const;

  bool hasSwapchain() const noexcept {
    return swapchain_ != nullptr;
  }

  Result waitIdle() const;
  Result present() const;

  const VkPhysicalDeviceProperties& getVkPhysicalDeviceProperties() const {
    return vkPhysicalDeviceProperties2_.properties;
  }

  const VkPhysicalDeviceFeatures2& getVkPhysicalDeviceFeatures2() const {
    return vkPhysicalDeviceFeatures2_;
  }

  VkFormat getClosestDepthStencilFormat(igl::TextureFormat desiredFormat) const;

  struct RenderPassHandle {
    VkRenderPass pass = VK_NULL_HANDLE;
    uint8_t index = 0;
  };

  // render passes are owned and managed by the context
  RenderPassHandle findRenderPass(const VulkanRenderPassBuilder& builder) const;
  RenderPassHandle getRenderPass(uint8_t index) const;

  // OpenXR needs Vulkan instance to find physical device
  VkInstance getVkInstance() const {
    return vkInstance_;
  }
  VkDevice getVkDevice() const {
    return device_->getVkDevice();
  }
  VkPhysicalDevice getVkPhysicalDevice() const {
    return vkPhysicalDevice_;
  }
  VkDescriptorSetLayout getBindlessVkDescriptorSetLayout() const;
  VkDescriptorSet getBindlessVkDescriptorSet() const;

  std::vector<uint8_t> getPipelineCacheData() const;

  uint64_t getFrameNumber() const;

  using SubmitHandle = VulkanImmediateCommands::SubmitHandle;

  // execute a task some time in the future after the submit handle finished processing
  void deferredTask(std::packaged_task<void()>&& task, SubmitHandle handle = SubmitHandle()) const;

  bool areValidationLayersEnabled() const;

  void* getVmaAllocator() const;

  VkSamplerYcbcrConversionInfo getOrCreateYcbcrConversionInfo(VkFormat format) const;

  void freeResourcesForDescriptorSetLayout(VkDescriptorSetLayout dsl) const;

#if defined(IGL_WITH_TRACY_GPU)
  TracyVkCtx tracyCtx_ = nullptr;
  std::unique_ptr<VulkanCommandPool> profilingCommandPool_;
  VkCommandBuffer profilingCommandBuffer_ = VK_NULL_HANDLE;
#endif

 private:
  void createInstance(size_t numExtraExtensions, const char** extraExtensions);
  void createSurface(void* window, void* display);
  void checkAndUpdateDescriptorSets();
  void querySurfaceCapabilities();
  void processDeferredTasks() const;
  void waitDeferredTasks();
  void growBindlessDescriptorPool(uint32_t newMaxTextures, uint32_t newMaxSamplers);

 private:
  friend class igl::vulkan::Device;
  friend class igl::vulkan::VulkanStagingDevice;
  friend class igl::vulkan::VulkanSwapchain;
  friend class igl::vulkan::CommandQueue;
  friend class igl::vulkan::ComputeCommandEncoder;
  friend class igl::vulkan::RenderCommandEncoder;

  // should be kept on the heap, otherwise global Vulkan functions can cause arbitrary crashes.
  std::unique_ptr<VulkanFunctionTable> tableImpl_;

  VkInstance vkInstance_ = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT vkDebugUtilsMessenger_ = VK_NULL_HANDLE;
  VkSurfaceKHR vkSurface_ = VK_NULL_HANDLE;
  VkPhysicalDevice vkPhysicalDevice_ = VK_NULL_HANDLE;
  FOLLY_PUSH_WARNING
  FOLLY_GNU_DISABLE_WARNING("-Wmissing-field-initializers")
  VkPhysicalDeviceDescriptorIndexingPropertiesEXT vkPhysicalDeviceDescriptorIndexingProperties_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT,
      // Ignore clang-diagnostic-missing-field-initializers
      // @lint-ignore CLANGTIDY
      nullptr};

  // Provided by VK_KHR_driver_properties
  VkPhysicalDeviceDriverPropertiesKHR vkPhysicalDeviceDriverProperties_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR,
      // Ignore clang-diagnostic-missing-field-initializers
      // @lint-ignore CLANGTIDY
      &vkPhysicalDeviceDescriptorIndexingProperties_};

  std::vector<VkFormat> deviceDepthFormats_;
  std::vector<VkSurfaceFormatKHR> deviceSurfaceFormats_;
  VkSurfaceCapabilitiesKHR deviceSurfaceCaps_{};
  std::vector<VkPresentModeKHR> devicePresentModes_;

  // Provided by VK_VERSION_1_2
  VkPhysicalDeviceShaderFloat16Int8Features vkPhysicalDeviceShaderFloat16Int8Features_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR,
      nullptr};

  // Provided by VK_VERSION_1_1
  VkPhysicalDeviceProperties2 vkPhysicalDeviceProperties2_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
      &vkPhysicalDeviceDriverProperties_,
      VkPhysicalDeviceProperties{}};
  VkPhysicalDeviceMultiviewFeatures vkPhysicalDeviceMultiviewFeatures_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
      &vkPhysicalDeviceShaderFloat16Int8Features_};
  VkPhysicalDeviceShaderDrawParametersFeatures vkPhysicalDeviceShaderDrawParametersFeatures_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
      &vkPhysicalDeviceMultiviewFeatures_,
  };
  VkPhysicalDeviceSamplerYcbcrConversionFeatures vkPhysicalDeviceSamplerYcbcrConversionFeatures_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,
      &vkPhysicalDeviceShaderDrawParametersFeatures_,
  };
  VkPhysicalDeviceFeatures2 vkPhysicalDeviceFeatures2_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      &vkPhysicalDeviceSamplerYcbcrConversionFeatures_};
  FOLLY_POP_WARNING

 public:
  const VulkanFunctionTable& vf_;
  DeviceQueues deviceQueues_;
  std::unordered_map<CommandQueueType, VulkanQueueDescriptor> userQueues_;
  std::unique_ptr<igl::vulkan::VulkanDevice> device_;
  std::unique_ptr<igl::vulkan::VulkanSwapchain> swapchain_;
  std::unique_ptr<igl::vulkan::VulkanImmediateCommands> immediate_;
  std::unique_ptr<igl::vulkan::VulkanStagingDevice> stagingDevice_;

  std::unique_ptr<igl::vulkan::VulkanBuffer> dummyUniformBuffer_;
  std::unique_ptr<igl::vulkan::VulkanBuffer> dummyStorageBuffer_;
  // don't use staging on devices with device-local host-visible memory
  bool useStagingForBuffers_ = true;

  std::unique_ptr<VulkanContextImpl> pimpl_;

  VkPipelineCache pipelineCache_ = VK_NULL_HANDLE;

  mutable std::unordered_map<VkFormat, VkSamplerYcbcrConversionInfo> ycbcrConversionInfos_;

  // 1. Textures can be safely deleted once they are not in use by GPU, hence our Vulkan context
  // owns all allocated textures (images+image views). The IGL interface vulkan::Texture does not
  // delete the underlying VulkanTexture but instead informs the context that it should be
  // deallocated. The context deallocates textures in a deferred way when it is safe to do so.
  // 2. Descriptor sets can be updated when they are not in use.
  mutable Pool<TextureTag, std::shared_ptr<VulkanTexture>> textures_;
  mutable Pool<SamplerTag, std::shared_ptr<VulkanSampler>> samplers_;
  // a texture/sampler was created since the last descriptor set update
  mutable bool awaitingCreation_ = false;
  Pool<BindGroupBufferTag, BindGroupBufferDesc> bindGroupBuffersPool_;
  Pool<BindGroupTextureTag, BindGroupTextureDesc> bindGroupTexturesPool_;

  mutable size_t drawCallCount_ = 0;

  // stores an index into renderPasses_
  mutable std::
      unordered_map<VulkanRenderPassBuilder, uint8_t, VulkanRenderPassBuilder::HashFunction>
          renderPassesHash_;
  mutable std::vector<VkRenderPass> renderPasses_;

  VulkanExtensions extensions_;
  VulkanContextConfig config_;

  // Enhanced shader debug: line drawing
  std::unique_ptr<EnhancedShaderDebuggingStore> enhancedShaderDebuggingStore_;

  void updateBindingsTextures(VkCommandBuffer cmdBuf,
                              VkPipelineLayout layout,
                              VkPipelineBindPoint bindPoint,
                              const BindingsTextures& data,
                              const VulkanDescriptorSetLayout& dsl,
                              const util::SpvModuleInfo& info) const;
  void updateBindingsUniformBuffers(VkCommandBuffer cmdBuf,
                                    VkPipelineLayout layout,
                                    VkPipelineBindPoint bindPoint,
                                    BindingsBuffers& data,
                                    const VulkanDescriptorSetLayout& dsl,
                                    const util::SpvModuleInfo& info) const;
  void updateBindingsStorageBuffers(VkCommandBuffer cmdBuf,
                                    VkPipelineLayout layout,
                                    VkPipelineBindPoint bindPoint,
                                    BindingsBuffers& data,
                                    const VulkanDescriptorSetLayout& dsl,
                                    const util::SpvModuleInfo& info) const;
  void markSubmitted(const SubmitHandle& handle) const;

  struct DeferredTask {
    DeferredTask(std::packaged_task<void()>&& task, SubmitHandle handle) :
      task_(std::move(task)), handle_(handle) {}
    std::packaged_task<void()> task_;
    SubmitHandle handle_;
    uint64_t frameId_ = 0;
  };

  mutable std::deque<DeferredTask> deferredTasks_;

  mutable std::mutex deferredTasksMutex_;

  std::unique_ptr<SyncManager> syncManager_;
};

} // namespace igl::vulkan
