/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Common.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanImageView.h>
#include <igl/vulkan/VulkanStagingDevice.h>
#include <igl/vulkan/VulkanTexture.h>

#include <format>

namespace igl::vulkan {

Texture::Texture(const igl::vulkan::Device& device, TextureFormat format) :
  ITexture(format), device_(device) {}

Result Texture::create(const TextureDesc& desc) {
  desc_ = desc;

  const VulkanContext& ctx = device_.getVulkanContext();

  const VkFormat vkFormat = isDepthOrStencilFormat(desc_.format)
                                ? ctx.getClosestDepthStencilFormat(desc_.format)
                                : textureFormatToVkFormat(desc_.format);

  const igl::TextureType type = desc_.type;
  if (!IGL_VERIFY(type == TextureType::TwoD || type == TextureType::Cube ||
                  type == TextureType::ThreeD)) {
    IGL_ASSERT_MSG(false, "Only 1D, 2D, 3D and Cube textures are supported");
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

  if (!IGL_VERIFY(desc_.numMipLevels <= TextureDesc::calcNumMipLevels(desc_.width, desc_.height))) {
    return Result(Result::Code::ArgumentOutOfRange,
                  "The number of specified mip levels is greater than the maximum possible "
                  "number of mip levels.");
  }

  if (desc_.usage == 0) {
    IGL_ASSERT_MSG(false, "Texture usage flags are not set");
    desc_.usage = TextureDesc::TextureUsageBits::Sampled;
  }

  /* Use staging device to transfer data into the image when the storage is private to the device */
  VkImageUsageFlags usageFlags =
      (desc_.storage == ResourceStorage::Private) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0;

  if (desc_.usage & TextureDesc::TextureUsageBits::Sampled) {
    usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if (desc_.usage & TextureDesc::TextureUsageBits::Storage) {
    IGL_ASSERT_MSG(desc_.numSamples <= 1, "Storage images cannot be multisampled");
    usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  if (desc_.usage & TextureDesc::TextureUsageBits::Attachment) {
    usageFlags |= isDepthOrStencilFormat(desc_.format) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                       : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }

  // For now, always set this flag so we can read it back
  usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  IGL_ASSERT_MSG(usageFlags != 0, "Invalid usage flags");

  const VkMemoryPropertyFlags memFlags = resourceStorageToVkMemoryPropertyFlags(desc_.storage);

  const bool hasDebugName = desc_.debugName && *desc_.debugName;

  const std::string debugNameImage = hasDebugName ? std::format("Image: {}", desc_.debugName) : "";
  const std::string debugNameImageView =
      hasDebugName ? std::format("Image View: {}", desc_.debugName) : "";

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
  if (!IGL_VERIFY(image.get())) {
    return Result(Result::Code::InvalidOperation, "Cannot create VulkanImage");
  }

  // TODO: use multiple image views to allow sampling from the STENCIL buffer
  const VkImageAspectFlags aspect = (usageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                                        ? VK_IMAGE_ASPECT_DEPTH_BIT
                                        : VK_IMAGE_ASPECT_COLOR_BIT;

  std::shared_ptr<VulkanImageView> imageView = image->createImageView(imageViewType,
                                                                      vkFormat,
                                                                      aspect,
                                                                      0,
                                                                      VK_REMAINING_MIP_LEVELS,
                                                                      0,
                                                                      arrayLayerCount,
                                                                      debugNameImageView.c_str());

  if (!IGL_VERIFY(imageView.get())) {
    return Result(Result::Code::InvalidOperation, "Cannot create VulkanImageView");
  }

  texture_ = ctx.createTexture(std::move(image), std::move(imageView));

  return Result();
}

Result Texture::upload(const TextureRangeDesc& range, const void* data[]) const {
  if (!data) {
    return igl::Result();
  }
  const auto result = validateRange(range);
  if (!IGL_VERIFY(result.isOk())) {
    return result;
  }

  const uint32_t numLayers = std::max(range.numLayers, 1u);

  const VulkanContext& ctx = device_.getVulkanContext();
  const VkImageType type = texture_->getVulkanImage().type_;

  if (type == VK_IMAGE_TYPE_3D) {
    const void* uploadData = data[0];
    ctx.stagingDevice_->imageData3D(
        texture_->getVulkanImage(),
        VkOffset3D{(int32_t)range.x, (int32_t)range.y, (int32_t)range.z},
        VkExtent3D{(uint32_t)range.width, (uint32_t)range.height, (uint32_t)range.depth},
        getVkFormat(),
        uploadData);
  } else {
    const VkRect2D imageRegion = ivkGetRect2D(
        (uint32_t)range.x, (uint32_t)range.y, (uint32_t)range.width, (uint32_t)range.height);
    ctx.stagingDevice_->imageData2D(texture_->getVulkanImage(),
                                    imageRegion,
                                    range.mipLevel,
                                    range.numMipLevels,
                                    range.layer,
                                    range.numLayers,
                                    getVkFormat(),
                                    data);
  }

  return Result();
}

Dimensions Texture::getDimensions() const {
  return Dimensions{desc_.width, desc_.height, desc_.depth};
}

VkFormat Texture::getVkFormat() const {
  IGL_ASSERT(texture_.get());
  return texture_ ? texture_->getVulkanImage().imageFormat_ : VK_FORMAT_UNDEFINED;
}

void Texture::generateMipmap() const {
  if (desc_.numMipLevels > 1) {
    IGL_ASSERT(texture_.get());
    IGL_ASSERT(texture_->getVulkanImage().imageLayout_ != VK_IMAGE_LAYOUT_UNDEFINED);
    const auto& ctx = device_.getVulkanContext();
    const auto& wrapper = ctx.immediate_->acquire();
    texture_->getVulkanImage().generateMipmap(wrapper.cmdBuf_);
    ctx.immediate_->submit(wrapper);
  }
}

uint32_t Texture::getTextureId() const {
  return texture_ ? texture_->getTextureId() : 0;
}

VkImageView Texture::getVkImageView() const {
  return texture_ ? texture_->getVulkanImageView().vkImageView_ : VK_NULL_HANDLE;
}

VkImageView Texture::getVkImageViewForFramebuffer(uint32_t level) const {
  if (level < imageViewForFramebuffer_.size() && imageViewForFramebuffer_[level]) {
    return imageViewForFramebuffer_[level]->getVkImageView();
  }

  if (level >= imageViewForFramebuffer_.size()) {
    imageViewForFramebuffer_.resize(level + 1);
  }

  const VkImageAspectFlags flags = texture_->getVulkanImage().getImageAspectFlags();

  imageViewForFramebuffer_[level] = texture_->getVulkanImage().createImageView(
      VK_IMAGE_VIEW_TYPE_2D, textureFormatToVkFormat(desc_.format), flags, level, 1u, 0u, 1u);

  return imageViewForFramebuffer_[level]->vkImageView_;
}

VkImage Texture::getVkImage() const {
  return texture_ ? texture_->getVulkanImage().vkImage_ : VK_NULL_HANDLE;
}

bool Texture::isSwapchainTexture() const {
  return texture_ ? texture_->getVulkanImage().isExternallyManaged_ : false;
}

Result Texture::validateRange(const igl::TextureRangeDesc& range) const {
  if (IGL_UNEXPECTED(range.width == 0 || range.height == 0 || range.depth == 0 ||
                     range.numLayers == 0 || range.numMipLevels == 0)) {
    return Result{Result::Code::ArgumentInvalid,
                  "width, height, depth numLayers, and numMipLevels must be at least 1."};
  }
  if (range.mipLevel > desc_.numMipLevels) {
    return Result{Result::Code::ArgumentOutOfRange, "range mip-level exceeds texture mip-levels"};
  }

  const auto dimensions = getDimensions();
  const auto texWidth = std::max(dimensions.width >> range.mipLevel, 1u);
  const auto texHeight = std::max(dimensions.height >> range.mipLevel, 1u);
  const auto texDepth = std::max(dimensions.depth >> range.mipLevel, 1u);
  const auto texSlices = desc_.numLayers;

  if (range.width > texWidth || range.height > texHeight || range.depth > texDepth) {
    return Result{Result::Code::ArgumentOutOfRange, "range dimensions exceed texture dimensions"};
  }
  if (range.x > texWidth - range.width || range.y > texHeight - range.height ||
      range.z > texDepth - range.depth) {
    return Result{Result::Code::ArgumentOutOfRange, "range dimensions exceed texture dimensions"};
  }

  return Result{};
}

} // namespace igl::vulkan
