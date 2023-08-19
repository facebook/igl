/*
 * LightweightVK
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <lvk/vulkan/VulkanUtils.h>

namespace lvk {

namespace vulkan {

class VulkanContext;

} // namespace vulkan

class VulkanBuffer final {
 public:
  VulkanBuffer() = default;
  VulkanBuffer(lvk::vulkan::VulkanContext* ctx,
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

 public:
  lvk::vulkan::VulkanContext* ctx_ = nullptr;
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
};

class VulkanImage final {
 public:
  VulkanImage(lvk::vulkan::VulkanContext& ctx,
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
  VulkanImage(lvk::vulkan::VulkanContext& ctx,
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
  lvk::vulkan::VulkanContext& ctx_;
  VkPhysicalDevice vkPhysicalDevice_ = VK_NULL_HANDLE;
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
  VkImageView getOrCreateVkImageViewForFramebuffer(uint8_t level);

  std::shared_ptr<lvk::VulkanImage> image_;
  VkImageView imageView_ = VK_NULL_HANDLE; // all mip-levels
  VkImageView imageViewForFramebuffer_[LVK_MAX_MIP_LEVELS] = {};
};

class VulkanSwapchain final {
  enum { LVK_MAX_SWAPCHAIN_IMAGES = 16 };
 public:
  VulkanSwapchain(vulkan::VulkanContext& ctx, uint32_t width, uint32_t height);
  ~VulkanSwapchain();

  Result present(VkSemaphore waitSemaphore);
  VkImage getCurrentVkImage() const;
  VkImageView getCurrentVkImageView() const;
  TextureHandle getCurrentTexture();

 public:
  VkSemaphore acquireSemaphore_ = VK_NULL_HANDLE;

 private:
  vulkan::VulkanContext& ctx_;
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
  // the maximum number of command buffers which can similtaneously exist in the system; when we run out of buffers, we stall and wait until an existing buffer becomes available
  static constexpr uint32_t kMaxCommandBuffers = 64;

  VulkanImmediateCommands(VkDevice device, uint32_t queueFamilyIndex, const char* debugName);
  ~VulkanImmediateCommands();
  VulkanImmediateCommands(const VulkanImmediateCommands&) = delete;
  VulkanImmediateCommands& operator=(const VulkanImmediateCommands&) = delete;

  struct SubmitHandle {
    uint32_t bufferIndex_ = 0;
    uint32_t submitId_ = 0;
    SubmitHandle() = default;
    explicit SubmitHandle(uint64_t handle) : bufferIndex_(uint32_t(handle & 0xffffffff)), submitId_(uint32_t(handle >> 32)) {
      LVK_ASSERT(submitId_);
    }
    bool empty() const {
      return submitId_ == 0;
    }
    uint64_t handle() const {
      return (uint64_t(submitId_) << 32) + bufferIndex_;
    }
  };

  static_assert(sizeof(SubmitHandle) == sizeof(uint64_t));

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

} // namespace lvk
