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

namespace igl {
namespace vulkan {

Texture::Texture(const igl::vulkan::Device& device, TextureFormat format) :
  ITexture(format), device_(device) {}

Result Texture::create(const TextureDesc& desc) {
  desc_ = desc;

  const VulkanContext& ctx = device_.getVulkanContext();

  const VkFormat vkFormat = getProperties().isDepthOrStencil()
                                ? ctx.getClosestDepthStencilFormat(desc_.format)
                                : textureFormatToVkFormat(desc_.format);

  const igl::TextureType type = desc_.type;
  if (!IGL_VERIFY(type == TextureType::TwoD || type == TextureType::TwoDArray ||
                  type == TextureType::Cube || type == TextureType::ThreeD)) {
    IGL_ASSERT_MSG(false, "Only 1D, 1D array, 2D, 2D array, 3D and cubemap textures are supported");
    return Result(Result::Code::Unimplemented);
  }

  if (desc_.numMipLevels == 0) {
    IGL_ASSERT_MSG(false, "The number of mip levels specified must be greater than 0");
    desc_.numMipLevels = 1;
  }

  if (desc.numSamples > 1 && desc_.numMipLevels != 1) {
    IGL_ASSERT_MSG(false, "The number of mip levels for multisampled images should be 1");
    return Result(Result::Code::ArgumentOutOfRange,
                  "The number of mip levels for multisampled images should be 1");
  }

  if (desc.numSamples > 1 && type == TextureType::ThreeD) {
    IGL_ASSERT_MSG(false, "Multisampled 3D images are not supported");
    return Result(Result::Code::ArgumentOutOfRange, "Multisampled 3D images are not supported");
  }

  if (desc.numLayers > 1 && desc.type != TextureType::TwoDArray) {
    return Result{Result::Code::Unsupported,
                  "Array textures are only supported when type is TwoDArray."};
  }

  if (!IGL_VERIFY(desc_.numMipLevels <=
                  TextureDesc::calcNumMipLevels(desc_.width, desc_.height, desc_.height))) {
    return Result(Result::Code::ArgumentOutOfRange,
                  "The number of specified mip levels is greater than the maximum possible "
                  "number of mip levels.");
  }

  if (desc_.usage == 0) {
    IGL_ASSERT_MSG(false, "Texture usage flags are not set");
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
  if (!ctx.useStagingForBuffers_ && desc_.storage == ResourceStorage::Private &&
      !getProperties().isDepthOrStencil()) {
    desc_.storage = ResourceStorage::Shared;
  }

  if (desc_.usage & TextureDesc::TextureUsageBits::Sampled) {
    usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if (desc_.usage & TextureDesc::TextureUsageBits::Storage) {
    IGL_ASSERT_MSG(desc_.numSamples <= 1, "Storage images cannot be multisampled");
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

  IGL_ASSERT_MSG(usageFlags != 0, "Invalid usage flags");

  const VkMemoryPropertyFlags memFlags = resourceStorageToVkMemoryPropertyFlags(desc_.storage);

  const std::string debugNameImage =
      !desc_.debugName.empty() ? IGL_FORMAT("Image: {}", desc_.debugName.c_str()) : "";
  const std::string debugNameImageView =
      !desc_.debugName.empty() ? IGL_FORMAT("Image View: {}", desc_.debugName.c_str()) : "";

  VkImageCreateFlags createFlags = 0;
  uint32_t arrayLayerCount = static_cast<uint32_t>(desc_.numLayers);
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
    IGL_ASSERT_NOT_REACHED();
    return Result(Result::Code::Unimplemented, "Unimplemented or unsupported texture type.");
  }

  Result result;
  auto image = ctx.createImage(
      imageType,
      VkExtent3D{(uint32_t)desc_.width, (uint32_t)desc_.height, (uint32_t)desc_.depth},
      vkFormat,
      (uint32_t)desc_.numMipLevels,
      arrayLayerCount,
      VK_IMAGE_TILING_OPTIMAL,
      usageFlags,
      memFlags,
      createFlags,
      samples,
      &result,
      debugNameImage.c_str());
  if (!IGL_VERIFY(result.isOk())) {
    return result;
  }
  if (!IGL_VERIFY(image.valid())) {
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

  if (!IGL_VERIFY(imageView.valid())) {
    return Result(Result::Code::InvalidOperation, "Cannot create VulkanImageView");
  }

  texture_ = ctx.createTexture(std::move(image), std::move(imageView), desc.debugName.c_str());

  if (aspect == VK_IMAGE_ASPECT_COLOR_BIT && samples == VK_SAMPLE_COUNT_1_BIT &&
      (usageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0) {
    // always clear color attachments by default
    clearColorTexture({0, 0, 0, 1});
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

  const VulkanContext& ctx = device_.getVulkanContext();
  ctx.stagingDevice_->imageData(
      texture_->getVulkanImage(), desc_.type, range, getProperties(), bytesPerRow, data);

  return Result();
}

Dimensions Texture::getDimensions() const {
  return Dimensions{desc_.width, desc_.height, desc_.depth};
}

VkFormat Texture::getVkFormat() const {
  IGL_ASSERT(texture_);
  return texture_ ? texture_->getVulkanImage().imageFormat_ : VK_FORMAT_UNDEFINED;
}

size_t Texture::getNumLayers() const {
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
  if (desc_.numMipLevels > 1) {
    const auto& ctx = device_.getVulkanContext();
    const auto& wrapper = ctx.immediate_->acquire();
    texture_->getVulkanImage().generateMipmap(wrapper.cmdBuf_, range ? *range : desc_.asRange());
    ctx.immediate_->submit(wrapper);
  }
}

void Texture::generateMipmap(ICommandBuffer& cmdBuffer, const TextureRangeDesc* range) const {
  auto& vkCmdBuffer = static_cast<vulkan::CommandBuffer&>(cmdBuffer);
  texture_->getVulkanImage().generateMipmap(vkCmdBuffer.getVkCommandBuffer(),
                                            range ? *range : desc_.asRange());
}

bool Texture::isRequiredGenerateMipmap() const {
  if (!texture_ || desc_.numMipLevels <= 1) {
    return false;
  }

  return texture_->getVulkanImage().imageLayout_ != VK_IMAGE_LAYOUT_UNDEFINED;
}

uint64_t Texture::getTextureId() const {
  const auto& config = device_.getVulkanContext().config_;
  IGL_ASSERT_MSG(config.enableDescriptorIndexing,
                 "Make sure config.enableDescriptorIndexing is enabled.");
  return texture_ && config.enableDescriptorIndexing ? texture_->getTextureId() : 0;
}

VkImageView Texture::getVkImageView() const {
  return texture_ ? texture_->getVulkanImageView().vkImageView_ : VK_NULL_HANDLE;
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

  const VkImageAspectFlags flags = texture_->getVulkanImage().getImageAspectFlags();
  imageViews[index] = texture_->getVulkanImage().createImageView(
      isStereo ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
      textureFormatToVkFormat(desc_.format),
      flags,
      mipLevel,
      1u,
      layer,
      isStereo ? VK_REMAINING_ARRAY_LAYERS : 1u,
      "Image View: igl/vulkan/Texture.cpp: Texture::getVkImageViewForFramebuffer()");

  return imageViews[index].vkImageView_;
}

VkImage Texture::getVkImage() const {
  return texture_ ? texture_->getVulkanImage().vkImage_ : VK_NULL_HANDLE;
}

bool Texture::isSwapchainTexture() const {
  return texture_ ? texture_->getVulkanImage().isExternallyManaged_ : false;
}

uint32_t Texture::getNumVkLayers() const {
  return desc_.type == TextureType::Cube ? 6u : static_cast<uint32_t>(desc_.numLayers);
}

void Texture::clearColorTexture(const igl::Color& rgba) {
  if (!texture_) {
    return;
  }

  const igl::vulkan::VulkanImage& img = texture_->getVulkanImage();
  IGL_ASSERT(img.valid());

  const auto& wrapper = img.ctx_->immediate_->acquire();

  img.clearColorImage(wrapper.cmdBuf_, rgba);

  img.ctx_->immediate_->submit(wrapper);
}

} // namespace vulkan
} // namespace igl
