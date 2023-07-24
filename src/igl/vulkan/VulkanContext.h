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

struct Bindings;
struct TextureBindings;
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
  // small default values are used to speed up debugging via RenderDoc and Validation Layers
  // macOS: MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS is required when using this with MoltenVK
  uint32_t maxTextures = 512;
  uint32_t maxSamplers = 512;
  bool terminateOnValidationError = false; // invoke std::terminate() on any validation error

  // enable/disable enhanced shader debugging capabilities (line drawing)
  bool enhancedShaderDebugging = false;

  bool enableConcurrentVkDevicesSupport = false;

  bool enableValidation = true;
  bool enableGPUAssistedValidation = true;
  bool enableSynchronizationValidation = false;
  bool enableBufferDeviceAddress = false;

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
  void checkAndUpdateDescriptorSets() const;
  void querySurfaceCapabilities();
  void processDeferredTasks() const;
  void waitDeferredTasks();

 private:
  friend class igl::vulkan::Device;
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
  std::unique_ptr<igl::vulkan::VulkanDescriptorSetLayout> dslBindless_; // everything
  std::unique_ptr<igl::vulkan::VulkanDescriptorSetLayout> dslTextures_; // texture/sampler slots for
                                                                        // the current drawcall
  std::unique_ptr<igl::vulkan::VulkanDescriptorSetLayout> dslBuffersUniform_; // buffer binding
                                                                              // slots for the
                                                                              // current drawcall
  std::unique_ptr<igl::vulkan::VulkanDescriptorSetLayout> dslBuffersStorage_; // buffer binding
                                                                              // slots for the
                                                                              // current drawcall
  VkDescriptorPool dpBindless_ = VK_NULL_HANDLE;
  VkDescriptorPool dpTextures_ = VK_NULL_HANDLE;
  VkDescriptorPool dpBuffersUniform_ = VK_NULL_HANDLE;
  VkDescriptorPool dpBuffersStorage_ = VK_NULL_HANDLE;
  struct DescriptorSet {
    VkDescriptorSet ds = VK_NULL_HANDLE;
    SubmitHandle handle =
        SubmitHandle(); // a handle of the last submit this descriptor set was a part of
  };
  mutable DescriptorSet bindlessDSet_;
  mutable std::vector<DescriptorSet> textureDSets_;
  mutable std::vector<DescriptorSet> bufferUniformDSets_;
  mutable std::vector<DescriptorSet> bufferStorageDSets_;
  mutable uint32_t currentDSetIndex_ = 0;
  mutable uint32_t prevSubmitIndex_ = 0;
  std::unique_ptr<igl::vulkan::VulkanPipelineLayout> pipelineLayoutGraphics_;
  std::unique_ptr<igl::vulkan::VulkanPipelineLayout> pipelineLayoutCompute_;
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

  void updateBindings(VkCommandBuffer cmdBuf,
                      VkPipelineBindPoint bindPoint,
                      const Bindings& data) const;
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
