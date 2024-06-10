/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <vector>

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanHelpers.h>
#include <igl/vulkan/VulkanImageView.h>

namespace igl {
namespace vulkan {

class VulkanContext;
class VulkanImageView;
struct VulkanImageViewCreateInfo;

struct VulkanImageCreateInfo {
  VkImageUsageFlags usageFlags = 0;
  bool isExternallyManaged = true;
  VkExtent3D extent = VkExtent3D{0, 0, 0};
  VkImageType type = VK_IMAGE_TYPE_MAX_ENUM;
  VkFormat imageFormat = VK_FORMAT_UNDEFINED;
  uint32_t mipLevels = 1;
  uint32_t arrayLayers = 1;
  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
  bool isImported = false;
};

/**
 * @brief Encapsulates a Vulkan Image object (`VkImage`) along with some of its properties
 */
class VulkanImage final {
 public:
  explicit VulkanImage() = default;
  /**
   * @brief Constructs a `VulkanImage` object from a `VkImage` object. If a debug name is provided,
   * the constructor will assign it to the `VkImage` object. No other Vulkan functions are called
   */
  VulkanImage(const VulkanContext& ctx,
              VkDevice device,
              VkImage image,
              const char* debugName = nullptr,
              VkImageUsageFlags usageFlags = 0,
              bool isExternallyManaged = true,
              VkExtent3D extent = VkExtent3D{0, 0, 0},
              VkImageType type = VK_IMAGE_TYPE_MAX_ENUM,
              VkFormat imageFormat = VK_FORMAT_UNDEFINED,
              uint32_t mipLevels = 1,
              uint32_t arrayLayers = 1,
              VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
              bool isImported = false);

  /**
   * @brief Constructs a `VulkanImage` object from a `VkImage` object. If a debug name is provided,
   * the constructor will assign it to the `VkImage` object. No other Vulkan functions are called
   */
  VulkanImage(const VulkanContext& ctx,
              VkDevice device,
              VkImage image,
              const VulkanImageCreateInfo& createInfo,
              const char* debugName = nullptr);

  /**
   * @brief Constructs a `VulkanImage` object and a `VkImage` object. Except for the debug name, all
   * other parameters are required. The debug name, if provided, is associated with the newly
   * created `VkImage` object.
   *
   * The image must contain at least one mip level, one array layer and one sample
   * (`VK_SAMPLE_COUNT_1_BIT`). The format cannot be undefined (`VK_FORMAT_UNDEFINED`).
   *
   * If the image is host-visible (`memFlags` contains `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`), then
   * it is memory mapped until the object's destruction.
   */
  VulkanImage(const VulkanContext& ctx,
              VkDevice device,
              VkExtent3D extent,
              VkImageType type,
              VkFormat format,
              uint32_t mipLevels,
              uint32_t arrayLayers,
              VkImageTiling tiling,
              VkImageUsageFlags usageFlags,
              VkMemoryPropertyFlags memFlags,
              VkImageCreateFlags createFlags,
              VkSampleCountFlagBits samples,
              const char* debugName = nullptr);

  /**
   * @brief Constructs a `VulkanImage` object and a `VkImage` object from a file descriptor. The
   * `VkImage` object is backed by external memory. The handle type of the external memory used is
   * `VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR`.
   *
   * This constructor does not support VMA.
   *
   * Except for the debug name, all other parameters are required. The debug name, if provided, is
   * associated with the newly created VkImage object.
   *
   * The image must contain at least one mip level, one array layer and one sample
   * (`VK_SAMPLE_COUNT_1_BIT`). The format cannot be undefined (`VK_FORMAT_UNDEFINED`).
   *
   * If the image is host-visible (`memFlags` contains `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`), then
   * it is memory mapped until the object's destruction.
   *
   * This constructor is only supported on non-Windows environments.
   *
   * NOTE: Importing memory from a file descriptor transfers ownership of the descriptor from the
   * application to the Vulkan implementation. The application must not perform any operations on
   * the file descriptor after a successful import. The file descriptors are closed on object's
   * destruction automatically.
   */
  VulkanImage(const VulkanContext& ctx,
              int32_t undupedFileDescriptor,
              uint64_t memoryAllocationSize,
              VkDevice device,
              VkExtent3D extent,
              VkImageType type,
              VkFormat format,
              uint32_t mipLevels,
              uint32_t arrayLayers,
              VkImageTiling tiling,
              VkImageUsageFlags usageFlags,
              VkImageCreateFlags createFlags,
              VkSampleCountFlagBits samples,
              const char* debugName = nullptr);

#if IGL_PLATFORM_WIN
  /**
   * @brief Creates a `VulkanImage` with memory imported from a Windows handle.
   * NOTE:
   * 1. This should only be called on Windows and will crash on other platforms.
   * 2. The implementation currently only supports handleType
   * `VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT`, which means the handle must be created
   * by Vulkan API, not other graphics API.
   */
  VulkanImage(const VulkanContext& ctx,
              void* windowsHandle,
              VkDevice device,
              VkExtent3D extent,
              VkImageType type,
              VkFormat format,
              uint32_t mipLevels,
              uint32_t arrayLayers,
              VkImageTiling tiling,
              VkImageUsageFlags usageFlags,
              VkImageCreateFlags createFlags,
              VkSampleCountFlagBits samples,
              const char* debugName = nullptr);
#endif // IGL_PLATFORM_WIN

#if IGL_PLATFORM_WIN || IGL_PLATFORM_LINUX || IGL_PLATFORM_ANDROID
  /**
   * @brief Creates a `VulkanImage` object whose memory can be exported externally.
   * On Windows, the exported `HANDLE` will be stored in `exportedMemoryHandle_`.
   * On Linux/Android, the exported file descriptor will be stored in `exportedFd_`.
   */
  static VulkanImage createWithExportMemory(const VulkanContext& ctx,
                                            VkDevice device,
                                            VkExtent3D extent,
                                            VkImageType type,
                                            VkFormat format,
                                            uint32_t mipLevels,
                                            uint32_t arrayLayers,
                                            VkImageTiling tiling,
                                            VkImageUsageFlags usageFlags,
                                            VkImageCreateFlags createFlags,
                                            VkSampleCountFlagBits samples,
                                            const char* debugName = nullptr);
#endif // IGL_PLATFORM_WIN || IGL_PLATFORM_LINUX || IGL_PLATFORM_ANDROID

  ~VulkanImage();

  VulkanImage(const VulkanImage&) = delete;
  VulkanImage& operator=(const VulkanImage&) = delete;

  VulkanImage(VulkanImage&& other) {
    *this = std::move(other);
  }
  VulkanImage& operator=(VulkanImage&& other);

  VkImage getVkImage() const {
    return vkImage_;
  }

  /**
   * @brief Returns true if the object is valid
   */
  bool valid() const;

  VkImageUsageFlags getVkImageUsageFlags() const {
    return usageFlags_;
  }

  bool isSampledImage() const {
    return (usageFlags_ & VK_IMAGE_USAGE_SAMPLED_BIT) > 0;
  }

  bool isStorageImage() const {
    return (usageFlags_ & VK_IMAGE_USAGE_STORAGE_BIT) > 0;
  }

  /**
   * @brief Creates a `VkImageView` object from the `VkImage` stored in the object.
   *
   * Setting `numLevels` to a non-zero value will override `mipLevels_` value from the original
   * vulkan image, and can be used to create image views with different number of levels
   */
  VulkanImageView createImageView(VkImageViewType type,
                                  VkFormat format,
                                  VkImageAspectFlags aspectMask,
                                  uint32_t baseLevel,
                                  uint32_t numLevels = VK_REMAINING_MIP_LEVELS,
                                  uint32_t baseLayer = 0,
                                  uint32_t numLayers = 1,
                                  const char* debugName = nullptr) const;
  VulkanImageView createImageView(VulkanImageViewCreateInfo createInfo,
                                  const char* debugName = nullptr) const;
  void generateMipmap(VkCommandBuffer commandBuffer, const TextureRangeDesc& range) const;

  /**
   * @brief Transitions the `VkImage`'s layout from the current layout (stored in the object) to the
   * `newImageLayout` by recording an Image Memory Barrier into the commandBuffer.
   *
   * The source and destination access masks for the transition are automatically deduced based on
   * the `srcStageMask` and the `dstStageMask` parameters. Not not all `VkPipelineStageFlags` are
   * supported.
   */
  void transitionLayout(VkCommandBuffer commandBuffer,
                        VkImageLayout newImageLayout,
                        VkPipelineStageFlags srcStageMask,
                        VkPipelineStageFlags dstStageMask,
                        const VkImageSubresourceRange& subresourceRange) const;
  void clearColorImage(VkCommandBuffer commandBuffer,
                       const igl::Color& rgba,
                       const VkImageSubresourceRange* subresourceRange = nullptr) const;

  VkImageAspectFlags getImageAspectFlags() const;

  static bool isDepthFormat(VkFormat format);
  static bool isStencilFormat(VkFormat format);

  bool isMappedPtrAccessible() const {
    return (mappedPtr_ != nullptr) && ((tiling_ & VK_IMAGE_TILING_LINEAR) != 0);
  }

  bool isCoherentMemory() const {
    return isCoherentMemory_;
  }

 public:
  const VulkanContext* ctx_ = nullptr;
  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkImage vkImage_ = VK_NULL_HANDLE;
  VkImageUsageFlags usageFlags_ = 0;
  VkDeviceMemory vkMemory_ = VK_NULL_HANDLE;
  VmaAllocation vmaAllocation_ = VK_NULL_HANDLE;
  VkFormatProperties formatProperties_{};
  void* mappedPtr_ = nullptr;
  bool isExternallyManaged_ = false;
  VkExtent3D extent_ = {0, 0, 0};
  VkImageType type_ = VK_IMAGE_TYPE_MAX_ENUM;
  VkFormat imageFormat_ = VK_FORMAT_UNDEFINED;
  uint32_t mipLevels_ = 1;
  uint32_t arrayLayers_ = 1;
  VkSampleCountFlagBits samples_ = VK_SAMPLE_COUNT_1_BIT;
  bool isDepthFormat_ = false;
  bool isStencilFormat_ = false;
  bool isDepthOrStencilFormat_ = false;
  VkDeviceSize allocatedSize = 0;
  mutable VkImageLayout imageLayout_ = VK_IMAGE_LAYOUT_UNDEFINED; // current image layout
  bool isImported_ = false;
  bool isExported_ = false;
  bool isCubemap_ = false;
  void* exportedMemoryHandle_ = nullptr; // windows handle
  int exportedFd_ = -1; // linux fd
#if defined(IGL_DEBUG)
  std::string name_;
#endif

 private:
  VkImageTiling tiling_ = VK_IMAGE_TILING_OPTIMAL;
  bool isCoherentMemory_ = false;

#if IGL_PLATFORM_WIN || IGL_PLATFORM_LINUX || IGL_PLATFORM_ANDROID
  /**
   * @brief Constructs a `VulkanImage` object and a `VkImage` object. Except for the debug name, all
   * other parameters are required. The debug name, if provided, is associated with the newly
   * created `VkImage` object.
   *
   * This version is only supported on Windows, Linux and Android environments and accepts both
   * `VkExternalMemoryImageCreateInfoKHR` and `VkExportMemoryAllocateInfoKHR` pre-filled structures.
   *
   * On Windows, the external memory handle is stored in `exportedMemoryHandle_`. On Linux and
   * Android the external memory handle is stored in `exportedFd_`.
   *
   * The image must contain at least one mip level, one array layer and one sample
   * (`VK_SAMPLE_COUNT_1_BIT`). The format cannot be undefined (`VK_FORMAT_UNDEFINED`).
   *
   * If the image is host-visible (`memFlags` contains `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`), then
   * it is memory mapped until the object's destruction.
   */
  VulkanImage(const VulkanContext& ctx,
              VkDevice device,
              VkExtent3D extent,
              VkImageType type,
              VkFormat format,
              uint32_t mipLevels,
              uint32_t arrayLayers,
              VkImageTiling tiling,
              VkImageUsageFlags usageFlags,
              VkMemoryPropertyFlags memFlags,
              VkImageCreateFlags createFlags,
              VkSampleCountFlagBits samples,
              VkExternalMemoryHandleTypeFlags compatibleHandleTypes,
              const char* debugName);
#endif // IGL_PLATFORM_WIN || IGL_PLATFORM_LINUX || IGL_PLATFORM_ANDROID

  // No-op in all builds except DEBUG
  void setName(const std::string& name) noexcept;

  void destroy();
};

} // namespace vulkan
} // namespace igl
