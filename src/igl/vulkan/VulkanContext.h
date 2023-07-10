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

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanImmediateCommands.h>
#include <igl/vulkan/VulkanStagingDevice.h>

namespace igl {
namespace vulkan {

class Device;
class EnhancedShaderDebuggingStore;
class CommandBuffer;
class CommandQueue;
class RenderCommandEncoder;
class VulkanBuffer;
class VulkanDescriptorSetLayout;
class VulkanImage;
class VulkanImageView;
class VulkanPipelineLayout;
class VulkanSampler;
class VulkanSemaphore;
class VulkanSwapchain;
class VulkanTexture;

struct Bindings;
struct VulkanContextImpl;

struct DeviceQueues {
  const static uint32_t INVALID = 0xFFFFFFFF;
  uint32_t graphicsQueueFamilyIndex = INVALID;
  uint32_t computeQueueFamilyIndex = INVALID;

  VkQueue graphicsQueue = VK_NULL_HANDLE;
  VkQueue computeQueue = VK_NULL_HANDLE;

  DeviceQueues() = default;
};

struct VulkanContextConfig {
  uint32_t maxTextures = 512;
  uint32_t maxSamplers = 512;
  bool terminateOnValidationError = false; // invoke std::terminate() on any validation error
  bool enableValidation = true;
  igl::ColorSpace swapChainColorSpace = igl::ColorSpace::SRGB_NONLINEAR;

  // owned by the application - should be alive until initContext() returns
  const void* pipelineCacheData = nullptr;
  size_t pipelineCacheDataSize = 0;
};

class VulkanContext final {
 public:
  VulkanContext(const VulkanContextConfig& config,
                void* window,
                void* display = nullptr);
  ~VulkanContext();

  igl::Result queryDevices(HWDeviceType deviceType, std::vector<HWDeviceDesc>& outDevices);
  igl::Result initContext(const HWDeviceDesc& desc);

  igl::Result initSwapchain(uint32_t width, uint32_t height);
  VkExtent2D getSwapchainExtent() const;

  std::shared_ptr<VulkanImage> createImage(VkImageType imageType,
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
  std::shared_ptr<VulkanBuffer> createBuffer(VkDeviceSize bufferSize,
                                             VkBufferUsageFlags usageFlags,
                                             VkMemoryPropertyFlags memFlags,
                                             igl::Result* outResult,
                                             const char* debugName = nullptr) const;
  std::shared_ptr<VulkanTexture> createTexture(std::shared_ptr<VulkanImage> image,
                                               std::shared_ptr<VulkanImageView> imageView) const;
  std::shared_ptr<VulkanSampler> createSampler(const VkSamplerCreateInfo& ci,
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

  // OpenXR needs Vulkan instance to find physical device
  VkInstance getVkInstance() const {
    return vkInstance_;
  }
  VkDevice getVkDevice() const {
    return vkDevice_;
  }
  VkPhysicalDevice getVkPhysicalDevice() const {
    return vkPhysicalDevice_;
  }

  std::vector<uint8_t> getPipelineCacheData() const;

  uint64_t getFrameNumber() const;

  using SubmitHandle = VulkanImmediateCommands::SubmitHandle;

  // execute a task some time in the future after the submit handle finished processing
  void deferredTask(std::packaged_task<void()>&& task, SubmitHandle handle = SubmitHandle()) const;

  bool areValidationLayersEnabled() const;

  void* getVmaAllocator() const;

 private:
  void createInstance();
  void createSurface(void* window, void* display);
  void checkAndUpdateDescriptorSets() const;
  void bindDefaultDescriptorSets(VkCommandBuffer cmdBuf,
                                 VkPipelineBindPoint bindPoint) const;
  void querySurfaceCapabilities();
  void processDeferredTasks() const;
  void waitDeferredTasks();

 private:
  friend class igl::vulkan::Device;
  friend class igl::vulkan::VulkanSwapchain;
  friend class igl::vulkan::CommandQueue;
  friend class igl::vulkan::CommandBuffer;
  friend class igl::vulkan::RenderCommandEncoder;

  VkInstance vkInstance_ = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT vkDebugUtilsMessenger_ = VK_NULL_HANDLE;
  VkSurfaceKHR vkSurface_ = VK_NULL_HANDLE;
  VkPhysicalDevice vkPhysicalDevice_ = VK_NULL_HANDLE;
  VkDevice vkDevice_ = VK_NULL_HANDLE;

  // Provided by Vulkan 1.2
  VkPhysicalDeviceDescriptorIndexingProperties vkPhysicalDeviceDescriptorIndexingProperties_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES,
      nullptr};
  // Provided by Vulkan 1.2
  VkPhysicalDeviceDriverProperties vkPhysicalDeviceDriverProperties_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES,
      &vkPhysicalDeviceDescriptorIndexingProperties_};

  std::vector<VkFormat> deviceDepthFormats_;
  std::vector<VkSurfaceFormatKHR> deviceSurfaceFormats_;
  VkSurfaceCapabilitiesKHR deviceSurfaceCaps_;
  std::vector<VkPresentModeKHR> devicePresentModes_;

  // Provided by Vulkan 1.1
  VkPhysicalDeviceProperties2 vkPhysicalDeviceProperties2_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
      &vkPhysicalDeviceDriverProperties_,
      VkPhysicalDeviceProperties{}};
  VkPhysicalDeviceFeatures2 vkPhysicalDeviceFeatures2_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      nullptr};

 public:
  DeviceQueues deviceQueues_;
  std::unique_ptr<igl::vulkan::VulkanSwapchain> swapchain_;
  std::unique_ptr<igl::vulkan::VulkanImmediateCommands> immediate_;
  std::unique_ptr<igl::vulkan::VulkanStagingDevice> stagingDevice_;
  std::unique_ptr<igl::vulkan::VulkanDescriptorSetLayout> dslBindless_;
  VkDescriptorPool dpBindless_ = VK_NULL_HANDLE;
  struct BindlessDescriptorSet {
    VkDescriptorSet ds = VK_NULL_HANDLE;
    SubmitHandle handle =
        SubmitHandle(); // a handle of the last submit this descriptor set was a part of
  };
  mutable std::vector<BindlessDescriptorSet> bindlessDSets_;
  mutable uint32_t currentDSetIndex_ = 0;
  std::unique_ptr<igl::vulkan::VulkanPipelineLayout> pipelineLayout_;
  // don't use staging on devices with shared host-visible memory
  bool useStaging_ = true;

  std::unique_ptr<VulkanContextImpl> pimpl_;

  VkPipelineCache pipelineCache_ = VK_NULL_HANDLE;

  // 1. Textures can be safely deleted once they are not in use by GPU, hence our Vulkan context
  // owns all allocated textures (images+image views). The IGL interface vulkan::Texture does not
  // delete the underylying VulkanTexture but instead informs the context that it should be
  // deallocated. The context deallocates textures in a deferred way when it is safe to do so.
  // 2. Descriptor sets can be updated when they are not in use.
  mutable std::vector<std::shared_ptr<VulkanTexture>> textures_ = {nullptr}; // the guard element
                                                                             // [0] is always there
  mutable std::vector<std::shared_ptr<VulkanSampler>> samplers_ = {nullptr}; // the guard element
                                                                             // [0] is always there
  // contains a list of free indices inside the sparse array `textures_`
  mutable std::vector<uint32_t> freeIndicesTextures_;
  // contains a list of free indices inside the sparse array `samplers_`
  mutable std::vector<uint32_t> freeIndicesSamplers_;
  // a texture/sampler was created since the last descriptor set update
  mutable bool awaitingCreation_ = false;
  // a texture/sampler was deleted since the last descriptor set update
  mutable bool awaitingDeletion_ = false;
  mutable uint64_t lastDeletionFrame_ = 0;
    
  VulkanContextConfig config_;

  struct DeferredTask {
    DeferredTask(std::packaged_task<void()>&& task, SubmitHandle handle) :
      task_(std::move(task)), handle_(handle) {}
    std::packaged_task<void()> task_;
    SubmitHandle handle_;
  };

  mutable std::deque<DeferredTask> deferredTasks_;
};

} // namespace vulkan
} // namespace igl
