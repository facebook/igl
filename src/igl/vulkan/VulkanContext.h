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

#include <igl/HWDevice.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanExtensions.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanImmediateCommands.h>
#include <igl/vulkan/VulkanQueuePool.h>
#include <igl/vulkan/VulkanRenderPassBuilder.h>
#include <igl/vulkan/VulkanStagingDevice.h>

namespace igl {
namespace vulkan {

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

class DescriptorPoolsArena;
struct BindingsBuffers;
struct BindingsTextures;
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

  igl::ColorSpace swapChainColorSpace = igl::ColorSpace::SRGB_NONLINEAR;

  std::vector<CommandQueueType> userQueues;

  uint32_t maxResourceCount = 3u;

  // owned by the application - should be alive until initContext() returns
  const void* pipelineCacheData = nullptr;
  size_t pipelineCacheDataSize = 0;
};

class VulkanContext final {
 public:
  VulkanContext(const VulkanContextConfig& config,
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
  std::shared_ptr<VulkanImage> createImageFromFileDescriptor(int32_t fileDescriptor,
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

  std::vector<uint8_t> getPipelineCacheData() const;

  uint64_t getFrameNumber() const;

  using SubmitHandle = VulkanImmediateCommands::SubmitHandle;

  // execute a task some time in the future after the submit handle finished processing
  void deferredTask(std::packaged_task<void()>&& task, SubmitHandle handle = SubmitHandle()) const;

  bool areValidationLayersEnabled() const;

  void* getVmaAllocator() const;

 private:
  void createInstance(const size_t numExtraExtensions, const char** extraExtensions);
  void createSurface(void* window, void* display);
  void checkAndUpdateDescriptorSets();
  void bindDefaultDescriptorSets(VkCommandBuffer cmdBuf, VkPipelineBindPoint bindPointa) const;
  void querySurfaceCapabilities();
  void processDeferredTasks() const;
  void waitDeferredTasks();
  void growBindlessDescriptorPool(uint32_t newMaxTextures, uint32_t newMaxSamplers);
  void updatePipelineLayouts();

 private:
  friend class igl::vulkan::Device;
  friend class igl::vulkan::VulkanStagingDevice;
  friend class igl::vulkan::VulkanSwapchain;
  friend class igl::vulkan::CommandQueue;
  friend class igl::vulkan::ComputeCommandEncoder;
  friend class igl::vulkan::RenderCommandEncoder;

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
  VkSurfaceCapabilitiesKHR deviceSurfaceCaps_;
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
  VkPhysicalDeviceFeatures2 vkPhysicalDeviceFeatures2_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      &vkPhysicalDeviceMultiviewFeatures_};
  FOLLY_POP_WARNING

 public:
  DeviceQueues deviceQueues_;
  std::unordered_map<CommandQueueType, VulkanQueueDescriptor> userQueues_;
  std::unique_ptr<igl::vulkan::VulkanDevice> device_;
  std::unique_ptr<igl::vulkan::VulkanSwapchain> swapchain_;
  std::unique_ptr<igl::vulkan::VulkanImmediateCommands> immediate_;
  std::unique_ptr<igl::vulkan::VulkanStagingDevice> stagingDevice_;
  // combined image sampler slots for the current drawcall
  std::unique_ptr<igl::vulkan::VulkanDescriptorSetLayout> dslCombinedImageSamplers_;
  // uniform buffer slots for the current drawcall
  std::unique_ptr<igl::vulkan::VulkanDescriptorSetLayout> dslBuffersUniform_;
  std::unique_ptr<igl::vulkan::DescriptorPoolsArena> arenaBuffersStorage_;
  std::unique_ptr<igl::vulkan::VulkanDescriptorSetLayout> dslBindless_; // everything
  VkDescriptorPool dpCombinedImageSamplers_ = VK_NULL_HANDLE;
  VkDescriptorPool dpBuffersUniform_ = VK_NULL_HANDLE;
  VkDescriptorPool dpBindless_ = VK_NULL_HANDLE;
  struct DescriptorSet {
    VkDescriptorSet ds = VK_NULL_HANDLE;
    SubmitHandle handle =
        SubmitHandle(); // a handle of the last submit this descriptor set was a part of
  };
  struct DescriptorSetArray {
    std::vector<DescriptorSet> dsets;
    uint32_t current = 0;
    uint32_t prev = 0;
    VkDescriptorSet acquireNext(VulkanImmediateCommands& ic) {
      IGL_ASSERT(!dsets.empty());
      VkDescriptorSet ds = dsets[current].ds;
      ic.wait(std::exchange(dsets[current].handle, {}));
      current = (current + 1) % dsets.size();
      return ds;
    }
    void updateHandles(SubmitHandle handle) {
      IGL_ASSERT(!dsets.empty());
      for (uint32_t i = prev; i != current; i = (i + 1) % dsets.size()) {
        dsets[i].handle = handle;
      }
      prev = current;
    }
  };
  uint32_t currentMaxBindlessTextures_ = 8;
  uint32_t currentMaxBindlessSamplers_ = 8;
  mutable DescriptorSet bindlessDSet_;
  mutable DescriptorSetArray combinedImageSamplerDSets_;
  mutable DescriptorSetArray bufferUniformDSets_;
  std::unique_ptr<igl::vulkan::VulkanPipelineLayout> pipelineLayoutGraphics_;
  std::unique_ptr<igl::vulkan::VulkanPipelineLayout> pipelineLayoutCompute_;
  std::shared_ptr<igl::vulkan::VulkanBuffer> dummyUniformBuffer_;
  std::shared_ptr<igl::vulkan::VulkanBuffer> dummyStorageBuffer_;
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
  mutable uint64_t lastDeletionFrame_ = 0;

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
                              VkPipelineBindPoint bindPoint,
                              const BindingsTextures& data) const;
  void updateBindingsUniformBuffers(VkCommandBuffer cmdBuf,
                                    VkPipelineBindPoint bindPoint,
                                    BindingsBuffers& data) const;
  void updateBindingsStorageBuffers(VkCommandBuffer cmdBuf,
                                    VkPipelineBindPoint bindPoint,
                                    BindingsBuffers& data) const;
  void markSubmit(const SubmitHandle& handle) const;

  struct DeferredTask {
    DeferredTask(std::packaged_task<void()>&& task, SubmitHandle handle) :
      task_(std::move(task)), handle_(handle) {}
    std::packaged_task<void()> task_;
    SubmitHandle handle_;
  };

  mutable std::deque<DeferredTask> deferredTasks_;

  std::unique_ptr<SyncManager> syncManager_;
};

} // namespace vulkan
} // namespace igl
