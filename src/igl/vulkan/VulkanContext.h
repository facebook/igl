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
#include <igl/vulkan/VulkanFeatures.h>
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
 *  1 - uniform/storage buffers
 *  2 - bindless textures/samplers  <--  optional
 */
enum {
  kBindPoint_CombinedImageSamplers = 0,
  kBindPoint_Buffers = 1,
  kBindPoint_Bindless = 2,
};

struct DeviceQueues {
  const static uint32_t INVALID = 0xFFFFFFFF;
  uint32_t graphicsQueueFamilyIndex = INVALID;
  uint32_t computeQueueFamilyIndex = INVALID;

  VkQueue IGL_NULLABLE graphicsQueue = VK_NULL_HANDLE;
  VkQueue IGL_NULLABLE computeQueue = VK_NULL_HANDLE;

  DeviceQueues() = default;
};

class VulkanContext final {
 public:
  VulkanContext(VulkanContextConfig config,
                void* IGL_NULLABLE window,
                size_t numExtraInstanceExtensions,
                const char* IGL_NULLABLE* IGL_NULLABLE extraInstanceExtensions,
                void* IGL_NULLABLE display = nullptr);
  ~VulkanContext();

  igl::Result queryDevices(const HWDeviceQueryDesc& desc, std::vector<HWDeviceDesc>& outDevices);
  igl::Result initContext(const HWDeviceDesc& desc,
                          size_t numExtraDeviceExtensions = 0,
                          const char* IGL_NULLABLE* IGL_NULLABLE extraDeviceExtensions = nullptr,
                          const VulkanFeatures* IGL_NULLABLE requestedFeatures = nullptr);

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
                          igl::Result* IGL_NULLABLE outResult,
                          const char* IGL_NULLABLE debugName = nullptr) const;
  std::unique_ptr<VulkanImage> createImageFromFileDescriptor(
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
      const char* IGL_NULLABLE debugName = nullptr) const;
  std::unique_ptr<VulkanBuffer> createBuffer(VkDeviceSize bufferSize,
                                             VkBufferUsageFlags usageFlags,
                                             VkMemoryPropertyFlags memFlags,
                                             igl::Result* IGL_NULLABLE outResult,
                                             const char* IGL_NULLABLE debugName = nullptr) const;
  std::shared_ptr<VulkanTexture> createTexture(VulkanImage&& image,
                                               VulkanImageView&& imageView,
                                               const char* IGL_NULLABLE debugName) const;
  std::shared_ptr<VulkanTexture> createTextureFromVkImage(
      VkImage vkImage,
      VulkanImageCreateInfo imageCreateInfo,
      VulkanImageViewCreateInfo imageViewCreateInfo,
      const char* IGL_NULLABLE debugName) const;

  std::shared_ptr<VulkanSampler> createSampler(const VkSamplerCreateInfo& ci,
                                               VkFormat yuvVkFormat,
                                               igl::Result* IGL_NULLABLE outResult,
                                               const char* IGL_NULLABLE debugName = nullptr) const;

  void createSurface(void* IGL_NULLABLE window, void* IGL_NULLABLE display);

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
  VkInstance IGL_NULLABLE getVkInstance() const {
    return vkInstance_;
  }
  VkDevice IGL_NULLABLE getVkDevice() const {
    return device_->getVkDevice();
  }
  VkPhysicalDevice IGL_NULLABLE getVkPhysicalDevice() const {
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

  void* IGL_NULLABLE getVmaAllocator() const;

  VkSamplerYcbcrConversionInfo getOrCreateYcbcrConversionInfo(VkFormat format) const;

  void freeResourcesForDescriptorSetLayout(VkDescriptorSetLayout dsl) const;

  const VulkanFeatures& features() const noexcept;

  const VkSurfaceCapabilitiesKHR & getSurfaceCapabilities() { return deviceSurfaceCaps_; }

  void createSurface(void* IGL_NULLABLE window, void* IGL_NULLABLE display);


#if defined(IGL_WITH_TRACY_GPU)
  TracyVkCtx tracyCtx_ = nullptr;
  std::unique_ptr<VulkanCommandPool> profilingCommandPool_;
  VkCommandBuffer profilingCommandBuffer_ = VK_NULL_HANDLE;
#endif

 private:
  void createInstance(size_t numExtraExtensions,
                      const char* IGL_NULLABLE* IGL_NULLABLE extraExtensions);
  VkResult checkAndUpdateDescriptorSets();
  void querySurfaceCapabilities();
  void processDeferredTasks() const;
  void waitDeferredTasks();
  void growBindlessDescriptorPool(uint32_t newMaxTextures, uint32_t newMaxSamplers);
  igl::BindGroupTextureHandle createBindGroup(const BindGroupTextureDesc& desc,
                                              const IRenderPipelineState* IGL_NULLABLE
                                                  compatiblePipeline,
                                              Result* IGL_NULLABLE outResult);
  igl::BindGroupBufferHandle createBindGroup(const BindGroupBufferDesc& desc,
                                             Result* IGL_NULLABLE outResult);
  void destroy(igl::BindGroupTextureHandle handle);
  void destroy(igl::BindGroupBufferHandle handle);
  VkDescriptorSet getBindGroupDescriptorSet(igl::BindGroupTextureHandle handle) const;
  VkDescriptorSet getBindGroupDescriptorSet(igl::BindGroupBufferHandle handle) const;
  uint32_t getBindGroupUsageMask(igl::BindGroupTextureHandle handle) const;
  uint32_t getBindGroupUsageMask(igl::BindGroupBufferHandle handle) const;

 private:
  friend class igl::vulkan::Device;
  friend class igl::vulkan::VulkanStagingDevice;
  friend class igl::vulkan::VulkanSwapchain;
  friend class igl::vulkan::CommandQueue;
  friend class igl::vulkan::ComputeCommandEncoder;
  friend class igl::vulkan::RenderCommandEncoder;

  // should be kept on the heap, otherwise global Vulkan functions can cause arbitrary crashes.
  std::unique_ptr<VulkanFunctionTable> tableImpl_;

  VkInstance IGL_NULLABLE vkInstance_ = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT vkDebugUtilsMessenger_ = VK_NULL_HANDLE;
  VkSurfaceKHR vkSurface_ = VK_NULL_HANDLE;
  VkPhysicalDevice IGL_NULLABLE vkPhysicalDevice_ = VK_NULL_HANDLE;
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

  VulkanFeatures features_;

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

  void updateBindingsTextures(VkCommandBuffer IGL_NONNULL cmdBuf,
                              VkPipelineLayout layout,
                              VkPipelineBindPoint bindPoint,
                              VulkanImmediateCommands::SubmitHandle nextSubmitHandle,
                              const BindingsTextures& data,
                              const VulkanDescriptorSetLayout& dsl,
                              const util::SpvModuleInfo& info) const;
  void updateBindingsBuffers(VkCommandBuffer IGL_NONNULL cmdBuf,
                             VkPipelineLayout layout,
                             VkPipelineBindPoint bindPoint,
                             VulkanImmediateCommands::SubmitHandle nextSubmitHandle,
                             BindingsBuffers& data,
                             const VulkanDescriptorSetLayout& dsl,
                             const util::SpvModuleInfo& info) const;

  struct DeferredTask {
    DeferredTask(std::packaged_task<void()>&& task, SubmitHandle handle) :
      task_(std::move(task)), handle_(handle) {}
    std::packaged_task<void()> task_;
    SubmitHandle handle_;
    uint64_t frameId_ = 0;
  };

  mutable std::deque<DeferredTask> deferredTasks_;

  std::unique_ptr<SyncManager> syncManager_;
};

} // namespace igl::vulkan
