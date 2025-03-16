/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/IGLSafeC.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanImageView.h>
#include <igl/vulkan/VulkanStagingDevice.h>
#include <igl/vulkan/VulkanTexture.h>

namespace igl::vulkan {

Texture::Texture(Device& device, TextureFormat format) : ITexture(format), device_(device) {}

Result Texture::create(const TextureDesc& desc) {
  desc_ = desc;

  const VulkanContext& ctx = device_.getVulkanContext();

  const VkFormat vkFormat = getProperties().isDepthOrStencil()
                                ? ctx.getClosestDepthStencilFormat(desc_.format)
                                : textureFormatToVkFormat(desc_.format);

  const igl::TextureType type = desc_.type;
  if (!IGL_DEBUG_VERIFY(type == TextureType::TwoD || type == TextureType::TwoDArray ||
                        type == TextureType::Cube || type == TextureType::ThreeD)) {
    IGL_DEBUG_ABORT("Only 1D, 1D array, 2D, 2D array, 3D and cubemap textures are supported");
    return Result(Result::Code::Unimplemented);
  }

  if (desc_.numMipLevels == 0) {
    IGL_DEBUG_ABORT("The number of mip levels specified must be greater than 0");
    desc_.numMipLevels = 1;
  }

  if (desc.numSamples > 1 && desc_.numMipLevels != 1) {
    IGL_DEBUG_ABORT("The number of mip levels for multisampled images should be 1");
    return Result(Result::Code::ArgumentOutOfRange,
                  "The number of mip levels for multisampled images should be 1");
  }

  if (desc.numSamples > 1 && type == TextureType::ThreeD) {
    IGL_DEBUG_ABORT("Multisampled 3D images are not supported");
    return Result(Result::Code::ArgumentOutOfRange, "Multisampled 3D images are not supported");
  }

  if (desc.numLayers > 1 && desc.type != TextureType::TwoDArray) {
    return Result{Result::Code::Unsupported,
                  "Array textures are only supported when type is TwoDArray."};
  }

  if (!IGL_DEBUG_VERIFY(desc_.numMipLevels <=
                        TextureDesc::calcNumMipLevels(desc_.width, desc_.height, desc_.height))) {
    return Result(Result::Code::ArgumentOutOfRange,
                  "The number of specified mip levels is greater than the maximum possible "
                  "number of mip levels.");
  }

  if (desc_.usage == 0) {
    IGL_DEBUG_ABORT("Texture usage flags are not set");
    desc_.usage = TextureDesc::TextureUsageBits::Sampled;
  }
  // a simple heuristic to determine proper storage as the storage type is almost never provided by
  // existing IGL clients
  if (desc_.storage == ResourceStorage::Invalid) {
    desc_.storage = ResourceStorage::Private;
  }

  /* Use staging device to transfer data into the image when the storage is private to the device */
  VkImageUsageFlags usageFlags =
      (desc_.storage == ResourceStorage::Private) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0;

  // On M1 Macs, depth texture has to be ResourceStorage::Private.
  // On Intel Macs, multisample does not work with shared or managed storage modes
  if (!ctx.useStagingForBuffers_ && desc_.storage == ResourceStorage::Private &&
      !getProperties().isDepthOrStencil() && desc.numSamples == 1) {
    desc_.storage = ResourceStorage::Shared;
  }

  if (desc_.usage & TextureDesc::TextureUsageBits::Sampled) {
    usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if (desc_.usage & TextureDesc::TextureUsageBits::Storage) {
    IGL_DEBUG_ASSERT(desc_.numSamples <= 1, "Storage images cannot be multisampled");
    usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  if (desc_.usage & TextureDesc::TextureUsageBits::Attachment) {
    usageFlags |= getProperties().isDepthOrStencil() ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                     : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (desc_.storage == igl::ResourceStorage::Memoryless) {
      usageFlags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    }
  }

  // For now, always set this flag so we can read it back
  if (desc_.storage != igl::ResourceStorage::Memoryless) {
    // not supported on transient attachments
    usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }

  IGL_DEBUG_ASSERT(usageFlags != 0, "Invalid usage flags");

  const VkMemoryPropertyFlags memFlags = resourceStorageToVkMemoryPropertyFlags(desc_.storage);

  const std::string debugNameImage =
      !desc_.debugName.empty() ? IGL_FORMAT("Image: {}", desc_.debugName.c_str()) : "";
  const std::string debugNameImageView =
      !desc_.debugName.empty() ? IGL_FORMAT("Image View: {}", desc_.debugName.c_str()) : "";

  VkImageCreateFlags createFlags = 0;
  uint32_t arrayLayerCount = desc_.numLayers;
  VkImageViewType imageViewType;
  VkImageType imageType;
  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
  switch (desc_.type) {
  case TextureType::TwoD:
    imageViewType = VK_IMAGE_VIEW_TYPE_2D;
    imageType = VK_IMAGE_TYPE_2D;
    samples = getVulkanSampleCountFlags(desc_.numSamples);
    break;
  case TextureType::ThreeD:
    imageViewType = VK_IMAGE_VIEW_TYPE_3D;
    imageType = VK_IMAGE_TYPE_3D;
    break;
  case TextureType::Cube:
    imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
    arrayLayerCount *= 6;
    imageType = VK_IMAGE_TYPE_2D;
    createFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    break;
  case TextureType::TwoDArray:
    imageType = VK_IMAGE_TYPE_2D;
    imageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    samples = getVulkanSampleCountFlags(desc_.numSamples);
    break;
  default:
    IGL_DEBUG_ASSERT_NOT_REACHED();
    return Result(Result::Code::Unimplemented, "Unimplemented or unsupported texture type.");
  }

  const VkImageTiling tiling = desc.tiling == TextureDesc::TextureTiling::Optimal
                                   ? VK_IMAGE_TILING_OPTIMAL
                                   : VK_IMAGE_TILING_LINEAR;

  if (getProperties().numPlanes > 1) {
    // some constraints for multiplanar image formats
    IGL_DEBUG_ASSERT(imageType == VK_IMAGE_TYPE_2D);
    IGL_DEBUG_ASSERT(samples == VK_SAMPLE_COUNT_1_BIT);
    IGL_DEBUG_ASSERT(tiling == VK_IMAGE_TILING_OPTIMAL);
    IGL_DEBUG_ASSERT(desc.numLayers == 1);
    IGL_DEBUG_ASSERT(desc.numMipLevels == 1);
    createFlags |= VK_IMAGE_CREATE_DISJOINT_BIT | VK_IMAGE_CREATE_ALIAS_BIT |
                   VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
  }

  Result result;
  VulkanImage image;

  if (desc_.exportability == TextureDesc::TextureExportability::NoExport) {
    image = ctx.createImage(
        imageType,
        VkExtent3D{(uint32_t)desc_.width, (uint32_t)desc_.height, (uint32_t)desc_.depth},
        vkFormat,
        (uint32_t)desc_.numMipLevels,
        arrayLayerCount,
        tiling,
        usageFlags,
        memFlags,
        createFlags,
        samples,
        &result,
        debugNameImage.c_str());
    if (!IGL_DEBUG_VERIFY(result.isOk())) {
      return result;
    }
  } else if (desc_.exportability == TextureDesc::TextureExportability::Exportable) {
#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_LINUX || IGL_PLATFORM_ANDROID

    image = igl::vulkan::VulkanImage::createWithExportMemory(
        ctx,
        ctx.getVkDevice(),
        VkExtent3D{.width = (uint32_t)desc_.width,
                   .height = (uint32_t)desc_.height,
                   .depth = (uint32_t)desc_.depth},
        imageType,
        vkFormat,
        (uint32_t)desc_.numMipLevels,
        arrayLayerCount,
        tiling,
        usageFlags,
        createFlags,
        samples,
        "vulkan export memory image");

#else // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_LINUX || IGL_PLATFORM_ANDROID
    // Currently only Mac is not supported.
    return Result(Result::Code::Unimplemented,
                  "Exportable textures are not supported on this platform.");
#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_LINUX || IGL_PLATFORM_ANDROID
  }

  if (!IGL_DEBUG_VERIFY(image.valid())) {
    return Result(Result::Code::InvalidOperation, "Cannot create VulkanImage");
  }

  VkImageAspectFlags aspect = 0;
  if (image.isDepthOrStencilFormat_) {
    if (image.isDepthFormat_) {
      aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
    } else if (image.isStencilFormat_) {
      aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  } else {
    aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  VulkanImageView imageView = image.createImageView(imageViewType,
                                                    vkFormat,
                                                    aspect,
                                                    0,
                                                    VK_REMAINING_MIP_LEVELS,
                                                    0,
                                                    arrayLayerCount,
                                                    debugNameImageView.c_str());

  if (!IGL_DEBUG_VERIFY(imageView.valid())) {
    return Result(Result::Code::InvalidOperation, "Cannot create VulkanImageView");
  }

  texture_ = ctx.createTexture(std::move(image), std::move(imageView), desc.debugName.c_str());

  if (aspect == VK_IMAGE_ASPECT_COLOR_BIT && samples == VK_SAMPLE_COUNT_1_BIT &&
      (usageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0) {
    // always clear color attachments by default
    clearColorTexture({0, 0, 0, 0});
  }

  return Result();
}

bool Texture::needsRepacking(const TextureRangeDesc& /*range*/, size_t bytesPerRow) const {
  // Vulkan textures MUST be aligned to a multiple of the texel size or, for compressed textures,
  // the texel block size.
  return bytesPerRow != 0 && bytesPerRow % getProperties().bytesPerBlock != 0;
}

Result Texture::uploadInternal(TextureType /*type*/,
                               const TextureRangeDesc& range,
                               const void* data,
                               size_t bytesPerRow) const {
  if (!data) {
    return Result{};
  }

  const igl::vulkan::VulkanImage& vulkanImage = texture_->image_;
  if (vulkanImage.isMappedPtrAccessible()) {
    checked_memcpy(
        vulkanImage.mappedPtr_, vulkanImage.allocatedSize, data, bytesPerRow * range.width);
    vulkanImage.flushMappedMemory();
    return Result();
  }

  const VulkanContext& ctx = device_.getVulkanContext();

  const VkImageAspectFlags imageAspectFlags = texture_->imageView_.getVkImageAspectFlags();
  ctx.stagingDevice_->imageData(
      vulkanImage, desc_.type, range, getProperties(), bytesPerRow, imageAspectFlags, data);

  return Result();
}

Dimensions Texture::getDimensions() const {
  return Dimensions{desc_.width, desc_.height, desc_.depth};
}

VkFormat Texture::getVkFormat() const {
  IGL_DEBUG_ASSERT(texture_);
  return texture_ ? texture_->image_.imageFormat_ : VK_FORMAT_UNDEFINED;
}

VkImageUsageFlags Texture::getVkUsageFlags() const {
  IGL_DEBUG_ASSERT(texture_);
  return texture_ ? texture_->image_.getVkImageUsageFlags() : 0;
}

uint32_t Texture::getVkExtendedFormat() const {
  IGL_DEBUG_ASSERT(texture_);
  return texture_ ? texture_->image_.extendedFormat_ : 0;
}

uint32_t Texture::getNumLayers() const {
  return desc_.numLayers;
}

TextureType Texture::getType() const {
  return desc_.type;
}

TextureDesc::TextureUsage Texture::getUsage() const {
  return desc_.usage;
}

uint32_t Texture::getSamples() const {
  return desc_.numSamples;
}

uint32_t Texture::getNumMipLevels() const {
  return desc_.numMipLevels;
}

void Texture::generateMipmap(ICommandQueue& /* unused */,
                             const TextureRangeDesc* IGL_NULLABLE range) const {
  IGL_DEBUG_ASSERT(texture_);

  if (texture_ && desc_.numMipLevels > 1) {
    const auto& ctx = device_.getVulkanContext();
    const auto& wrapper = ctx.immediate_->acquire();
    texture_->image_.generateMipmap(wrapper.cmdBuf_, range ? *range : desc_.asRange());
    ctx.immediate_->submit(wrapper);
  }
}

void Texture::generateMipmap(ICommandBuffer& cmdBuffer, const TextureRangeDesc* range) const {
  IGL_DEBUG_ASSERT(texture_);

  auto& vkCmdBuffer = static_cast<CommandBuffer&>(cmdBuffer);
  texture_->image_.generateMipmap(vkCmdBuffer.getVkCommandBuffer(),
                                  range ? *range : desc_.asRange());
}

bool Texture::isRequiredGenerateMipmap() const {
  if (!texture_ || desc_.numMipLevels <= 1) {
    return false;
  }

  return texture_->image_.imageLayout_ != VK_IMAGE_LAYOUT_UNDEFINED;
}

uint64_t Texture::getTextureId() const {
  const auto& config = device_.getVulkanContext().config_;
  IGL_DEBUG_ASSERT(config.enableDescriptorIndexing,
                   "Make sure config.enableDescriptorIndexing is enabled.");
  return texture_ && config.enableDescriptorIndexing ? texture_->textureId_ : 0;
}

VkImageView Texture::getVkImageView() const {
  return texture_ ? texture_->imageView_.vkImageView_ : VK_NULL_HANDLE;
}

VkImageView Texture::getVkImageViewForFramebuffer(uint32_t mipLevel,
                                                  uint32_t layer,
                                                  FramebufferMode mode) const {
  const bool isStereo = mode == FramebufferMode::Stereo;
  const auto index = mipLevel * getNumVkLayers() + layer;
  std::vector<VulkanImageView>& imageViews = isStereo ? imageViewsForFramebufferStereo_
                                                      : imageViewsForFramebufferMono_;

  if (index < imageViews.size() && imageViews[index].valid()) {
    return imageViews[index].getVkImageView();
  }

  if (index >= imageViews.size()) {
    imageViews.resize(index + 1);
  }

  const VkFormat vkFormat =
      getProperties().isDepthOrStencil()
          ? device_.getVulkanContext().getClosestDepthStencilFormat(desc_.format)
          : textureFormatToVkFormat(desc_.format);

  const VkImageAspectFlags flags = texture_->image_.getImageAspectFlags();
  imageViews[index] = texture_->image_.createImageView(
      isStereo ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
      vkFormat,
      flags,
      mipLevel,
      1u,
      layer,
      isStereo ? VK_REMAINING_ARRAY_LAYERS : 1u,
      "Image View: igl/vulkan/Texture.cpp: Texture::getVkImageViewForFramebuffer()");

  return imageViews[index].vkImageView_;
}

VkImage Texture::getVkImage() const {
  return texture_ ? texture_->image_.vkImage_ : VK_NULL_HANDLE;
}

VulkanTexture& Texture::getVulkanTexture() const {
  IGL_DEBUG_ASSERT(texture_);
  return *texture_;
}

bool Texture::isSwapchainTexture() const {
  return texture_ ? texture_->image_.isExternallyManaged_ : false;
}

uint32_t Texture::getNumVkLayers() const {
  return desc_.type == TextureType::Cube ? 6u : desc_.numLayers;
}

void Texture::clearColorTexture(const igl::Color& rgba) {
  if (!texture_) {
    return;
  }

  const igl::vulkan::VulkanImage& img = texture_->image_;
  IGL_DEBUG_ASSERT(img.valid());

  const auto& wrapper = img.ctx_->stagingDevice_->immediate_->acquire();

  // There is a memory barrier inserted in clearColorImage().
  // The memory barrier is necessary to ensure synchronized access.
  img.clearColorImage(wrapper.cmdBuf_, rgba);

  img.ctx_->stagingDevice_->immediate_->submit(wrapper);
}

} // namespace igl::vulkan
