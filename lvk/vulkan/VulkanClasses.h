/*
 * LightweightVK
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <lvk/vulkan/VulkanUtils.h>
#include <lvk/Pool.h>

#include <future>
#include <memory>
#include <vector>

namespace lvk {

class VulkanContext;

struct DeviceQueues {
  const static uint32_t INVALID = 0xFFFFFFFF;
  uint32_t graphicsQueueFamilyIndex = INVALID;
  uint32_t computeQueueFamilyIndex = INVALID;

  VkQueue graphicsQueue = VK_NULL_HANDLE;
  VkQueue computeQueue = VK_NULL_HANDLE;
};

class VulkanBuffer final {
 public:
  VulkanBuffer() = default;
  VulkanBuffer(lvk::VulkanContext* ctx,
               VkDevice device,
               VkDeviceSize bufferSize,
               VkBufferUsageFlags usageFlags,
               VkMemoryPropertyFlags memFlags,
               const char* debugName = nullptr);
  ~VulkanBuffer();

  VulkanBuffer(const VulkanBuffer&) = delete;
  VulkanBuffer& operator=(const VulkanBuffer&) = delete;

  VulkanBuffer(VulkanBuffer&& other);
  VulkanBuffer& operator=(VulkanBuffer&& other);

  void bufferSubData(size_t offset, size_t size, const void* data);
  void getBufferSubData(size_t offset, size_t size, void* data);
  [[nodiscard]] uint8_t* getMappedPtr() const {
    return static_cast<uint8_t*>(mappedPtr_);
  }
  bool isMapped() const {
    return mappedPtr_ != nullptr;
  }
  void flushMappedMemory(VkDeviceSize offset, VkDeviceSize size) const;
  void invalidateMappedMemory(VkDeviceSize offset, VkDeviceSize size) const;

 public:
  lvk::VulkanContext* ctx_ = nullptr;
  VkDevice device_ = VK_NULL_HANDLE;
  VkBuffer vkBuffer_ = VK_NULL_HANDLE;
  VkDeviceMemory vkMemory_ = VK_NULL_HANDLE;
  VmaAllocationCreateInfo vmaAllocInfo_ = {};
  VmaAllocation vmaAllocation_ = VK_NULL_HANDLE;
  VkDeviceAddress vkDeviceAddress_ = 0;
  VkDeviceSize bufferSize_ = 0;
  VkBufferUsageFlags vkUsageFlags_ = 0;
  VkMemoryPropertyFlags vkMemFlags_ = 0;
  void* mappedPtr_ = nullptr;
  bool isCoherentMemory_ = false;
};

class VulkanImage final {
 public:
  VulkanImage(lvk::VulkanContext& ctx,
              VkDevice device,
              VkExtent3D extent,
              VkImageType type,
              VkFormat format,
              uint32_t numLevels,
              uint32_t numLayers,
              VkImageTiling tiling,
              VkImageUsageFlags usageFlags,
              VkMemoryPropertyFlags memFlags,
              VkImageCreateFlags createFlags,
              VkSampleCountFlagBits samples,
              const char* debugName);
  VulkanImage(lvk::VulkanContext& ctx,
              VkDevice device,
              VkImage image,
              VkImageUsageFlags usageFlags,
              VkFormat imageFormat,
              VkExtent3D extent,
              const char* debugName);
  ~VulkanImage();

  VulkanImage(const VulkanImage&) = delete;
  VulkanImage& operator=(const VulkanImage&) = delete;

  // clang-format off
  bool isSampledImage() const { return (vkUsageFlags_ & VK_IMAGE_USAGE_SAMPLED_BIT) > 0; }
  bool isStorageImage() const { return (vkUsageFlags_ & VK_IMAGE_USAGE_STORAGE_BIT) > 0; }
  // clang-format on

  /*
   * Setting `numLevels` to a non-zero value will override `mipLevels_` value from the original Vulkan image, and can be used to create
   * image views with different number of levels.
   */
  VkImageView createImageView(VkImageViewType type,
                              VkFormat format,
                              VkImageAspectFlags aspectMask,
                              uint32_t baseLevel,
                              uint32_t numLevels = VK_REMAINING_MIP_LEVELS,
                              uint32_t baseLayer = 0,
                              uint32_t numLayers = 1,
                              const char* debugName = nullptr) const;

  void generateMipmap(VkCommandBuffer commandBuffer) const;

  void transitionLayout(VkCommandBuffer commandBuffer,
                        VkImageLayout newImageLayout,
                        VkPipelineStageFlags srcStageMask,
                        VkPipelineStageFlags dstStageMask,
                        const VkImageSubresourceRange& subresourceRange) const;

  VkImageAspectFlags getImageAspectFlags() const;

  static bool isDepthFormat(VkFormat format);
  static bool isStencilFormat(VkFormat format);

 public:
  lvk::VulkanContext& ctx_;
  VkDevice vkDevice_ = VK_NULL_HANDLE;
  VkImage vkImage_ = VK_NULL_HANDLE;
  VkImageUsageFlags vkUsageFlags_ = 0;
  VkDeviceMemory vkMemory_ = VK_NULL_HANDLE;
  VmaAllocationCreateInfo vmaAllocInfo_ = {};
  VmaAllocation vmaAllocation_ = VK_NULL_HANDLE;
  VkFormatProperties vkFormatProperties_ = {};
  VkExtent3D vkExtent_ = {0, 0, 0};
  VkImageType vkType_ = VK_IMAGE_TYPE_MAX_ENUM;
  VkFormat vkImageFormat_ = VK_FORMAT_UNDEFINED;
  VkSampleCountFlagBits vkSamples_ = VK_SAMPLE_COUNT_1_BIT;
  void* mappedPtr_ = nullptr;
  bool isSwapchainImage_ = false;
  uint32_t numLevels_ = 1u;
  uint32_t numLayers_ = 1u;
  bool isDepthFormat_ = false;
  bool isStencilFormat_ = false;
  // current image layout
  mutable VkImageLayout vkImageLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct VulkanTexture final {
  VulkanTexture() = default;
  VulkanTexture(std::shared_ptr<lvk::VulkanImage> image, VkImageView imageView);
  ~VulkanTexture();

  VulkanTexture(const VulkanTexture&) = delete;
  VulkanTexture& operator=(const VulkanTexture&) = delete;

  VulkanTexture(VulkanTexture&& other);
  VulkanTexture& operator=(VulkanTexture&& other);

  VkExtent3D getExtent() const {
    LVK_ASSERT(image_.get());
    return image_->vkExtent_;
  }

  // framebuffers can render only into one level/layer
  VkImageView getOrCreateVkImageViewForFramebuffer(uint8_t level, uint16_t layer);

  std::shared_ptr<lvk::VulkanImage> image_;
  VkImageView imageView_ = VK_NULL_HANDLE; // all mip-levels
  VkImageView imageViewForFramebuffer_[LVK_MAX_MIP_LEVELS][6] = {}; // max 6 faces for cubemap rendering
};

class VulkanSwapchain final {
  enum { LVK_MAX_SWAPCHAIN_IMAGES = 16 };

 public:
  VulkanSwapchain(VulkanContext& ctx, uint32_t width, uint32_t height);
  ~VulkanSwapchain();

  Result present(VkSemaphore waitSemaphore);
  VkImage getCurrentVkImage() const;
  VkImageView getCurrentVkImageView() const;
  TextureHandle getCurrentTexture();
  const VkSurfaceFormatKHR& getSurfaceFormat() const;
  uint32_t getNumSwapchainImages() const;

 public:
  VkSemaphore acquireSemaphore_ = VK_NULL_HANDLE;

 private:
  VulkanContext& ctx_;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue graphicsQueue_ = VK_NULL_HANDLE;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint32_t numSwapchainImages_ = 0;
  uint32_t currentImageIndex_ = 0;
  bool getNextImage_ = true;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkSurfaceFormatKHR surfaceFormat_ = {.format = VK_FORMAT_UNDEFINED};
  TextureHandle swapchainTextures_[LVK_MAX_SWAPCHAIN_IMAGES] = {};
};

class VulkanImmediateCommands final {
 public:
  // the maximum number of command buffers which can similtaneously exist in the system; when we run out of buffers, we stall and wait until
  // an existing buffer becomes available
  static constexpr uint32_t kMaxCommandBuffers = 64;

  VulkanImmediateCommands(VkDevice device, uint32_t queueFamilyIndex, const char* debugName);
  ~VulkanImmediateCommands();
  VulkanImmediateCommands(const VulkanImmediateCommands&) = delete;
  VulkanImmediateCommands& operator=(const VulkanImmediateCommands&) = delete;

  struct CommandBufferWrapper {
    VkCommandBuffer cmdBuf_ = VK_NULL_HANDLE;
    VkCommandBuffer cmdBufAllocated_ = VK_NULL_HANDLE;
    SubmitHandle handle_ = {};
    VkFence fence_ = VK_NULL_HANDLE;
    VkSemaphore semaphore_ = VK_NULL_HANDLE;
    bool isEncoding_ = false;
  };

  // returns the current command buffer (creates one if it does not exist)
  const CommandBufferWrapper& acquire();
  SubmitHandle submit(const CommandBufferWrapper& wrapper);
  void waitSemaphore(VkSemaphore semaphore);
  VkSemaphore acquireLastSubmitSemaphore();
  SubmitHandle getLastSubmitHandle() const;
  bool isReady(SubmitHandle handle, bool fastCheckNoVulkan = false) const;
  void wait(SubmitHandle handle);
  void waitAll();

 private:
  void purge();

 private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  VkCommandPool commandPool_ = VK_NULL_HANDLE;
  uint32_t queueFamilyIndex_ = 0;
  const char* debugName_ = "";
  CommandBufferWrapper buffers_[kMaxCommandBuffers];
  SubmitHandle lastSubmitHandle_ = SubmitHandle();
  VkSemaphore lastSubmitSemaphore_ = VK_NULL_HANDLE;
  VkSemaphore waitSemaphore_ = VK_NULL_HANDLE;
  uint32_t numAvailableCommandBuffers_ = kMaxCommandBuffers;
  uint32_t submitCounter_ = 1;
};

struct RenderPipelineDynamicState final {
#if defined(__APPLE__)
  VkCompareOp depthCompareOp_ : 3 = VK_COMPARE_OP_ALWAYS;
  VkBool32 depthWriteEnable_ : 1 = VK_FALSE;
#endif
  VkBool32 depthBiasEnable_ : 1 = VK_FALSE;
};

static_assert(sizeof(RenderPipelineDynamicState) == sizeof(uint32_t));

struct RenderPipelineState final {
  void destroyPipelines(lvk::VulkanContext* ctx);

  RenderPipelineDesc desc_;

  uint32_t numBindings_ = 0;
  uint32_t numAttributes_ = 0;
  VkVertexInputBindingDescription vkBindings_[VertexInput::LVK_VERTEX_BUFFER_MAX] = {};
  VkVertexInputAttributeDescription vkAttributes_[VertexInput::LVK_VERTEX_ATTRIBUTES_MAX] = {};

  // non-owning, cached the last pipeline layout from the context (if the context has a new layout, invalidate all VkPipeline objects)
  VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

#if !defined(__APPLE__)
  // [depthBiasEnable]
  VkPipeline pipelines_[2] = {};
#else
  // [depthCompareOp][depthWriteEnable][depthBiasEnable]
  VkPipeline pipelines_[8][2][2] = {};
#endif // __APPLE__
};

class VulkanPipelineBuilder final {
 public:
  VulkanPipelineBuilder();
  ~VulkanPipelineBuilder() = default;

  VulkanPipelineBuilder& depthBiasEnable(bool enable);
#if defined(__APPLE__)
  VulkanPipelineBuilder& depthWriteEnable(bool enable);
  VulkanPipelineBuilder& depthCompareOp(VkCompareOp compareOp);
#endif
  VulkanPipelineBuilder& dynamicState(VkDynamicState state);
  VulkanPipelineBuilder& primitiveTopology(VkPrimitiveTopology topology);
  VulkanPipelineBuilder& rasterizationSamples(VkSampleCountFlagBits samples);
  VulkanPipelineBuilder& shaderStage(VkPipelineShaderStageCreateInfo stage);
  VulkanPipelineBuilder& stencilStateOps(VkStencilFaceFlags faceMask,
                                         VkStencilOp failOp,
                                         VkStencilOp passOp,
                                         VkStencilOp depthFailOp,
                                         VkCompareOp compareOp);
  VulkanPipelineBuilder& stencilMasks(VkStencilFaceFlags faceMask, uint32_t compareMask, uint32_t writeMask, uint32_t reference);
  VulkanPipelineBuilder& cullMode(VkCullModeFlags mode);
  VulkanPipelineBuilder& frontFace(VkFrontFace mode);
  VulkanPipelineBuilder& polygonMode(VkPolygonMode mode);
  VulkanPipelineBuilder& vertexInputState(const VkPipelineVertexInputStateCreateInfo& state);
  VulkanPipelineBuilder& colorAttachments(const VkPipelineColorBlendAttachmentState* states,
                                          const VkFormat* formats,
                                          uint32_t numColorAttachments);
  VulkanPipelineBuilder& depthAttachmentFormat(VkFormat format);
  VulkanPipelineBuilder& stencilAttachmentFormat(VkFormat format);

  VkResult build(VkDevice device,
                 VkPipelineCache pipelineCache,
                 VkPipelineLayout pipelineLayout,
                 VkPipeline* outPipeline,
                 const char* debugName = nullptr) noexcept;

  static uint32_t getNumPipelinesCreated() {
    return numPipelinesCreated_;
  }

 private:
  enum { LVK_MAX_DYNAMIC_STATES = 128 };
  uint32_t numDynamicStates_ = 0;
  VkDynamicState dynamicStates_[LVK_MAX_DYNAMIC_STATES] = {};

  uint32_t numShaderStages_ = 0;
  VkPipelineShaderStageCreateInfo shaderStages_[Stage_Frag + 1] = {};

  VkPipelineVertexInputStateCreateInfo vertexInputState_;
  VkPipelineInputAssemblyStateCreateInfo inputAssembly_;
  VkPipelineRasterizationStateCreateInfo rasterizationState_;
  VkPipelineMultisampleStateCreateInfo multisampleState_;
  VkPipelineDepthStencilStateCreateInfo depthStencilState_;

  uint32_t numColorAttachments_ = 0;
  VkPipelineColorBlendAttachmentState colorBlendAttachmentStates_[LVK_MAX_COLOR_ATTACHMENTS] = {};
  VkFormat colorAttachmentFormats_[LVK_MAX_COLOR_ATTACHMENTS] = {};

  VkFormat depthAttachmentFormat_ = VK_FORMAT_UNDEFINED;
  VkFormat stencilAttachmentFormat_ = VK_FORMAT_UNDEFINED;

  static uint32_t numPipelinesCreated_;
};

struct ComputePipelineState final {
  ComputePipelineDesc desc_;
  // non-owning, cached the last pipeline layout from the context
  VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
};

class CommandBuffer final : public ICommandBuffer {
 public:
  CommandBuffer() = default;
  explicit CommandBuffer(VulkanContext* ctx);
  ~CommandBuffer() override;

  CommandBuffer& operator=(CommandBuffer&& other) = default;

  void transitionToShaderReadOnly(TextureHandle surface) const override;

  void cmdBindComputePipeline(lvk::ComputePipelineHandle handle) override;
  void cmdDispatchThreadGroups(const Dimensions& threadgroupCount, const Dependencies& deps) override;

  void cmdPushDebugGroupLabel(const char* label, uint32_t colorRGBA) const override;
  void cmdInsertDebugEventLabel(const char* label, uint32_t colorRGBA) const override;
  void cmdPopDebugGroupLabel() const override;

  void cmdBeginRendering(const lvk::RenderPass& renderPass, const lvk::Framebuffer& desc) override;
  void cmdEndRendering() override;

  void cmdBindViewport(const Viewport& viewport) override;
  void cmdBindScissorRect(const ScissorRect& rect) override;

  void cmdBindRenderPipeline(lvk::RenderPipelineHandle handle) override;
  void cmdBindDepthState(const DepthState& state) override;

  void cmdBindVertexBuffer(uint32_t index, BufferHandle buffer, uint64_t bufferOffset) override;
  void cmdBindIndexBuffer(BufferHandle indexBuffer, IndexFormat indexFormat, uint64_t indexBufferOffset) override;
  void cmdPushConstants(const void* data, size_t size, size_t offset) override;

  void cmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t baseInstance) override;
  void cmdDrawIndexed(uint32_t indexCount,
                      uint32_t instanceCount,
                      uint32_t firstIndex,
                      int32_t vertexOffset,
                      uint32_t baseInstance) override;
  void cmdDrawIndirect(BufferHandle indirectBuffer, size_t indirectBufferOffset, uint32_t drawCount, uint32_t stride = 0) override;
  void cmdDrawIndexedIndirect(BufferHandle indirectBuffer, size_t indirectBufferOffset, uint32_t drawCount, uint32_t stride = 0) override;

  void cmdSetBlendColor(const float color[4]) override;
  void cmdSetDepthBias(float depthBias, float slopeScale, float clamp) override;

 private:
  void useComputeTexture(TextureHandle texture);
  void bindGraphicsPipeline();

 private:
  friend class VulkanContext;

  VulkanContext* ctx_ = nullptr;
  const VulkanImmediateCommands::CommandBufferWrapper* wrapper_ = nullptr;

  lvk::Framebuffer framebuffer_ = {};
  lvk::SubmitHandle lastSubmitHandle_ = {};

  VkPipeline lastPipelineBound_ = VK_NULL_HANDLE;

  bool isRendering_ = false;

  lvk::RenderPipelineHandle currentPipeline_ = {};
  lvk::RenderPipelineDynamicState dynamicState_ = {};
};

class VulkanStagingDevice final {
 public:
  explicit VulkanStagingDevice(VulkanContext& ctx);
  ~VulkanStagingDevice();

  VulkanStagingDevice(const VulkanStagingDevice&) = delete;
  VulkanStagingDevice& operator=(const VulkanStagingDevice&) = delete;

  void bufferSubData(VulkanBuffer& buffer, size_t dstOffset, size_t size, const void* data);
  void imageData2D(VulkanImage& image,
                   const VkRect2D& imageRegion,
                   uint32_t baseMipLevel,
                   uint32_t numMipLevels,
                   uint32_t layer,
                   uint32_t numLayers,
                   VkFormat format,
                   const void* data[]);
  void imageData3D(VulkanImage& image, const VkOffset3D& offset, const VkExtent3D& extent, VkFormat format, const void* data);
  void getImageData(VulkanImage& image,
                    const VkOffset3D& offset,
                    const VkExtent3D& extent,
                    VkImageSubresourceRange range,
                    VkFormat format,
                    void* outData);

 private:
  struct MemoryRegionDesc {
    uint32_t srcOffset_ = 0;
    uint32_t alignedSize_ = 0;
    SubmitHandle handle_ = {};
  };

  MemoryRegionDesc getNextFreeOffset(uint32_t size);
  void waitAndReset();

 private:
  VulkanContext& ctx_;
  BufferHandle stagingBuffer_;
  std::unique_ptr<lvk::VulkanImmediateCommands> immediate_;
  uint32_t stagingBufferFrontOffset_ = 0;
  uint32_t stagingBufferSize_ = 0;
  uint32_t bufferCapacity_ = 0;
  std::vector<MemoryRegionDesc> regions_;
};

class VulkanContext final : public IContext {
 public:
  VulkanContext(const lvk::ContextConfig& config, void* window, void* display = nullptr);
  ~VulkanContext();

  ICommandBuffer& acquireCommandBuffer() override;

  SubmitHandle submit(lvk::ICommandBuffer& commandBuffer, TextureHandle present) override;
  void wait(SubmitHandle handle) override;

  Holder<BufferHandle> createBuffer(const BufferDesc& desc, Result* outResult) override;
  Holder<SamplerHandle> createSampler(const SamplerStateDesc& desc, Result* outResult) override;
  Holder<TextureHandle> createTexture(const TextureDesc& desc, const char* debugName, Result* outResult) override;

  Holder<ComputePipelineHandle> createComputePipeline(const ComputePipelineDesc& desc, Result* outResult) override;
  Holder<RenderPipelineHandle> createRenderPipeline(const RenderPipelineDesc& desc, Result* outResult) override;
  Holder<ShaderModuleHandle> createShaderModule(const ShaderModuleDesc& desc, Result* outResult) override;

  void destroy(ComputePipelineHandle handle) override;
  void destroy(RenderPipelineHandle handle) override;
  void destroy(ShaderModuleHandle handle) override;
  void destroy(SamplerHandle handle) override;
  void destroy(BufferHandle handle) override;
  void destroy(TextureHandle handle) override;
  void destroy(Framebuffer& fb) override;

  Result upload(BufferHandle handle, const void* data, size_t size, size_t offset) override;
  uint8_t* getMappedPtr(BufferHandle handle) const override;
  uint64_t gpuAddress(BufferHandle handle, size_t offset) const override;
  void flushMappedMemory(BufferHandle handle, size_t offset, size_t size) const override;

  Result upload(TextureHandle handle, const TextureRangeDesc& range, const void* data[]) override;
  Result download(TextureHandle handle, const TextureRangeDesc& range, void* outData) override;
  Dimensions getDimensions(TextureHandle handle) const override;
  void generateMipmap(TextureHandle handle) const override;
  Format getFormat(TextureHandle handle) const override;

  TextureHandle getCurrentSwapchainTexture() override;
  Format getSwapchainFormat() const override;
  ColorSpace getSwapChainColorSpace() const override;
  uint32_t getNumSwapchainImages() const override;
  void recreateSwapchain(int newWidth, int newHeight) override;
  
  uint32_t getFramebufferMSAABitMask() const override;

  ///////////////

  VkPipeline getVkPipeline(ComputePipelineHandle handle);
  VkPipeline getVkPipeline(RenderPipelineHandle handle, const RenderPipelineDynamicState& dynamicState);

  uint32_t queryDevices(HWDeviceType deviceType, HWDeviceDesc* outDevices, uint32_t maxOutDevices = 1);
  lvk::Result initContext(const HWDeviceDesc& desc);
  lvk::Result initSwapchain(uint32_t width, uint32_t height);

  std::shared_ptr<VulkanImage> createImage(VkImageType imageType,
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
                                           const char* debugName = nullptr);
  BufferHandle createBuffer(VkDeviceSize bufferSize,
                            VkBufferUsageFlags usageFlags,
                            VkMemoryPropertyFlags memFlags,
                            lvk::Result* outResult,
                            const char* debugName = nullptr);
  SamplerHandle createSampler(const VkSamplerCreateInfo& ci, lvk::Result* outResult, const char* debugName = nullptr);

  bool hasSwapchain() const noexcept {
    return swapchain_ != nullptr;
  }

  const VkPhysicalDeviceProperties& getVkPhysicalDeviceProperties() const {
    return vkPhysicalDeviceProperties2_.properties;
  }

  VkFormat getClosestDepthStencilFormat(lvk::Format desiredFormat) const;

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

  // execute a task some time in the future after the submit handle finished processing
  void deferredTask(std::packaged_task<void()>&& task, SubmitHandle handle = SubmitHandle()) const;

  void* getVmaAllocator() const;

  void checkAndUpdateDescriptorSets();
  void bindDefaultDescriptorSets(VkCommandBuffer cmdBuf, VkPipelineBindPoint bindPoint) const;

 private:
  void createInstance();
  void createSurface(void* window, void* display);
  void querySurfaceCapabilities();
  void processDeferredTasks() const;
  void waitDeferredTasks();
  lvk::Result growDescriptorPool(uint32_t maxTextures, uint32_t maxSamplers);
  VkShaderModule createShaderModule(const void* data, size_t length, const char* debugName, Result* outResult) const;
  VkShaderModule createShaderModule(ShaderStage stage, const char* source, const char* debugName, Result* outResult) const;

 private:
  friend class lvk::VulkanSwapchain;

  VkInstance vkInstance_ = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT vkDebugUtilsMessenger_ = VK_NULL_HANDLE;
  VkSurfaceKHR vkSurface_ = VK_NULL_HANDLE;
  VkPhysicalDevice vkPhysicalDevice_ = VK_NULL_HANDLE;
  VkDevice vkDevice_ = VK_NULL_HANDLE;

  VkPhysicalDeviceVulkan13Features vkFeatures13_ = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  VkPhysicalDeviceVulkan12Features vkFeatures12_ = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                                                    .pNext = &vkFeatures13_};
  VkPhysicalDeviceVulkan11Features vkFeatures11_ = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
                                                    .pNext = &vkFeatures12_};
  VkPhysicalDeviceFeatures2 vkFeatures10_ = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &vkFeatures11_};

  // provided by Vulkan 1.2
  VkPhysicalDeviceDriverProperties vkPhysicalDeviceDriverProperties_ = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES, nullptr};
  VkPhysicalDeviceVulkan12Properties vkPhysicalDeviceVulkan12Properties_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES,
      &vkPhysicalDeviceDriverProperties_,
  };
  // provided by Vulkan 1.1
  VkPhysicalDeviceProperties2 vkPhysicalDeviceProperties2_ = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                                                              &vkPhysicalDeviceVulkan12Properties_,
                                                              VkPhysicalDeviceProperties{}};

  std::vector<VkFormat> deviceDepthFormats_;
  std::vector<VkSurfaceFormatKHR> deviceSurfaceFormats_;
  VkSurfaceCapabilitiesKHR deviceSurfaceCaps_;
  std::vector<VkPresentModeKHR> devicePresentModes_;

 public:
  DeviceQueues deviceQueues_;
  std::unique_ptr<lvk::VulkanSwapchain> swapchain_;
  std::unique_ptr<lvk::VulkanImmediateCommands> immediate_;
  std::unique_ptr<lvk::VulkanStagingDevice> stagingDevice_;
  uint32_t currentMaxTextures_ = 16;
  uint32_t currentMaxSamplers_ = 16;
  VkPipelineLayout vkPipelineLayout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout vkDSL_ = VK_NULL_HANDLE;
  VkDescriptorPool vkDPool_ = VK_NULL_HANDLE;
  VkDescriptorSet vkDSet_ = VK_NULL_HANDLE;
  SubmitHandle lastSubmitHandle = SubmitHandle(); // a handle of the last submit this descriptor set was a part of
  // don't use staging on devices with shared host-visible memory
  bool useStaging_ = true;

  std::unique_ptr<struct VulkanContextImpl> pimpl_;

  VkPipelineCache pipelineCache_ = VK_NULL_HANDLE;

  // a texture/sampler was created since the last descriptor set update
  mutable bool awaitingCreation_ = false;

  lvk::ContextConfig config_;

  lvk::Pool<lvk::ShaderModule, VkShaderModule> shaderModulesPool_;
  lvk::Pool<lvk::RenderPipeline, lvk::RenderPipelineState> renderPipelinesPool_;
  lvk::Pool<lvk::ComputePipeline, lvk::ComputePipelineState> computePipelinesPool_;
  lvk::Pool<lvk::Sampler, VkSampler> samplersPool_;
  lvk::Pool<lvk::Buffer, lvk::VulkanBuffer> buffersPool_;
  lvk::Pool<lvk::Texture, lvk::VulkanTexture> texturesPool_;
};

} // namespace lvk
