/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/IGLSafeC.h>
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

  if (!IGL_VERIFY(desc_.options == 0)) {
    IGL_ASSERT_NOT_IMPLEMENTED();
    return Result(Result::Code::Unimplemented);
  }
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

  if (!IGL_VERIFY(desc_.numMipLevels <= TextureDesc::calcNumMipLevels(desc_.width, desc_.height))) {
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
  if (!ctx.useStaging_ && desc_.storage == ResourceStorage::Private &&
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
  }

  // For now, always set this flag so we can read it back
  usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

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
  if (!IGL_VERIFY(image)) {
    return Result(Result::Code::InvalidOperation, "Cannot create VulkanImage");
  }

  VkImageAspectFlags aspect = 0;
  if (image->isDepthOrStencilFormat_) {
    if (image->isDepthFormat_) {
      aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
    } else if (image->isStencilFormat_) {
      aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  } else {
    aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  std::shared_ptr<VulkanImageView> imageView = image->createImageView(imageViewType,
                                                                      vkFormat,
                                                                      aspect,
                                                                      0,
                                                                      VK_REMAINING_MIP_LEVELS,
                                                                      0,
                                                                      arrayLayerCount,
                                                                      debugNameImageView.c_str());

  if (!IGL_VERIFY(imageView)) {
    return Result(Result::Code::InvalidOperation, "Cannot create VulkanImageView");
  }

  texture_ = ctx.createTexture(std::move(image), std::move(imageView));

  return Result();
}

Result Texture::upload(const TextureRangeDesc& range, const void* data, size_t bytesPerRow) const {
  if (!data) {
    return igl::Result();
  }
  const auto [result, _] = validateRange(range);
  if (!result.isOk()) {
    return result;
  }

  const void* uploadData = data;
  const auto imageRowWidth = getProperties().getBytesPerRow(range);

  std::vector<uint8_t> linearData;

  const bool isAligned = getProperties().isCompressed() || bytesPerRow == 0 ||
                         imageRowWidth == bytesPerRow;

  if (!isAligned) {
    linearData.resize(getProperties().getBytesPerRange(range.atLayer(0)));
  }

  auto numLayers = std::max(range.numLayers, static_cast<size_t>(1));
  auto byteIncrement =
      numLayers > 1 ? getProperties().getBytesPerLayer(range.width, range.height, range.depth) : 0;
  if (range.numMipLevels > 1) {
    for (auto i = 1; i < range.numMipLevels; ++i) {
      byteIncrement += getProperties().getBytesPerRange(range.atMipLevel(i));
    }
  }

  for (auto i = 0; i < numLayers; ++i) {
    if (isAligned) {
      uploadData = data;
    } else {
      const auto rows = getProperties().getRows(range);
      for (uint32_t h = 0; h < rows; h++) {
        checked_memcpy(static_cast<uint8_t*>(linearData.data()) + h * imageRowWidth,
                       linearData.size() - h * imageRowWidth,
                       static_cast<const uint8_t*>(data) + h * bytesPerRow,
                       imageRowWidth);
      }

      uploadData = linearData.data();
    }

    const VulkanContext& ctx = device_.getVulkanContext();

    const VkImageType type = texture_->getVulkanImage().type_;

    if (type == VK_IMAGE_TYPE_3D) {
      ctx.stagingDevice_->imageData3D(
          texture_->getVulkanImage(),
          VkOffset3D{(int32_t)range.x, (int32_t)range.y, (int32_t)range.z},
          VkExtent3D{(uint32_t)range.width, (uint32_t)range.height, (uint32_t)range.depth},
          getProperties(),
          getVkFormat(),
          uploadData);
    } else {
      const VkRect2D imageRegion = ivkGetRect2D(
          (uint32_t)range.x, (uint32_t)range.y, (uint32_t)range.width, (uint32_t)range.height);
      ctx.stagingDevice_->imageData2D(texture_->getVulkanImage(),
                                      imageRegion,
                                      (uint32_t)range.mipLevel,
                                      (uint32_t)range.numMipLevels,
                                      (uint32_t)range.layer + i,
                                      getProperties(),
                                      getVkFormat(),
                                      uploadData);
    }

    data = static_cast<const uint8_t*>(data) + byteIncrement;
  }

  return Result();
}

Result Texture::uploadCube(const TextureRangeDesc& range,
                           TextureCubeFace face,
                           const void* data,
                           size_t bytesPerRow) const {
  const auto [result, _] = validateRange(range);
  if (!result.isOk()) {
    return result;
  }

  const VulkanContext& ctx = device_.getVulkanContext();
  const VkRect2D imageRegion = ivkGetRect2D(
      (uint32_t)range.x, (uint32_t)range.y, (uint32_t)range.width, (uint32_t)range.height);
  ctx.stagingDevice_->imageData2D(texture_->getVulkanImage(),
                                  imageRegion,
                                  (uint32_t)range.mipLevel,
                                  (uint32_t)range.numMipLevels,
                                  (uint32_t)face - (uint32_t)TextureCubeFace::PosX,
                                  getProperties(),
                                  getVkFormat(),
                                  data);
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

ulong_t Texture::getUsage() const {
  return desc_.usage;
}

size_t Texture::getSamples() const {
  return desc_.numSamples;
}

size_t Texture::getNumMipLevels() const {
  return desc_.numMipLevels;
}

void Texture::generateMipmap(ICommandQueue& /*cmdQueue*/) const {
  if (desc_.numMipLevels > 1) {
    const auto& ctx = device_.getVulkanContext();
    const auto& wrapper = ctx.immediate_->acquire();
    texture_->getVulkanImage().generateMipmap(wrapper.cmdBuf_);
    ctx.immediate_->submit(wrapper);
  }
}

size_t Texture::getEstimatedSizeInBytes() const {
  const auto range = getFullRange(0, getNumMipLevels());
  auto totalBytes = properties_.getBytesPerRange(range);

  if (getType() == igl::TextureType::Cube) {
    totalBytes *= 6;
  }

  return totalBytes;
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

VkImageView Texture::getVkImageViewForFramebuffer(uint32_t level, FramebufferMode mode) const {
  if (level < imageViewForFramebuffer_.size() && imageViewForFramebuffer_[level]) {
    return imageViewForFramebuffer_[level]->getVkImageView();
  }

  if (level >= imageViewForFramebuffer_.size()) {
    imageViewForFramebuffer_.resize(level + 1);
  }

  const VkImageAspectFlags flags = texture_->getVulkanImage().getImageAspectFlags();
  const bool isStereo = mode == FramebufferMode::Stereo;

  imageViewForFramebuffer_[level] = texture_->getVulkanImage().createImageView(
      isStereo ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
      textureFormatToVkFormat(desc_.format),
      flags,
      level,
      1u,
      0u,
      isStereo ? VK_REMAINING_ARRAY_LAYERS : 1u);

  return imageViewForFramebuffer_[level]->vkImageView_;
}

VkImage Texture::getVkImage() const {
  return texture_ ? texture_->getVulkanImage().vkImage_ : VK_NULL_HANDLE;
}

bool Texture::isSwapchainTexture() const {
  return texture_ ? texture_->getVulkanImage().isExternallyManaged_ : false;
}

} // namespace vulkan
} // namespace igl
