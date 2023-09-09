/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/ITextureLoader.h>

#include <igl/Device.h>

namespace iglu::textureloader {

/// Interface for getting CPU access to GPU texture data
ITextureLoader::ITextureLoader(DataReader reader) noexcept : reader_(reader) {
  IGL_ASSERT(reader.data() != nullptr && reader.length() > 0);
  desc_.usage =
      igl::TextureDesc::TextureUsageBits::Sampled | igl::TextureDesc::TextureUsageBits::Attachment;
}

igl::TextureDesc& ITextureLoader::mutableDescriptor() noexcept {
  return desc_;
}

const igl::TextureDesc& ITextureLoader::descriptor() const noexcept {
  return desc_;
}

bool ITextureLoader::isSupported(const igl::ICapabilities& capabilities) const noexcept {
  return isSupported(capabilities, desc_.usage);
}

bool ITextureLoader::isSupported(const igl::ICapabilities& capabilities,
                                 igl::TextureDesc::TextureUsage usage) const noexcept {
  const auto caps = capabilities.getTextureFormatCapabilities(desc_.format);

  const bool isSampled = (usage & igl::TextureDesc::TextureUsageBits::Sampled) != 0;
  const bool isAttachment = (usage & igl::TextureDesc::TextureUsageBits::Attachment) != 0;
  const bool isStorage = (usage & igl::TextureDesc::TextureUsageBits::Storage) != 0;

  if (isSampled && isAttachment &&
      !igl::contains(caps, igl::ICapabilities::TextureFormatCapabilityBits::SampledAttachment)) {
    return false;
  } else if (isSampled &&
             !igl::contains(caps, igl::ICapabilities::TextureFormatCapabilityBits::Sampled)) {
    return false;
  } else if (isAttachment &&
             !igl::contains(caps, igl::ICapabilities::TextureFormatCapabilityBits::Attachment)) {
    return false;
  } else if (isStorage &&
             !igl::contains(caps, igl::ICapabilities::TextureFormatCapabilityBits::Storage)) {
    return false;
  }

  return true;
}

std::shared_ptr<igl::ITexture> ITextureLoader::create(const igl::IDevice& device,
                                                      igl::Result* IGL_NULLABLE
                                                          outResult) const noexcept {
  return create(device, desc_.usage, outResult);
}

std::shared_ptr<igl::ITexture> ITextureLoader::create(const igl::IDevice& device,
                                                      igl::TextureDesc::TextureUsage usage,
                                                      igl::Result* IGL_NULLABLE
                                                          outResult) const noexcept {
  igl::TextureDesc desc = desc_;
  desc.usage = usage;
  IGL_ASSERT(isSupported(device, usage));
  return device.createTexture(desc, outResult);
}

void ITextureLoader::upload(igl::ITexture& texture,
                            igl::Result* IGL_NULLABLE outResult) const noexcept {
  const auto dimensions = texture.getDimensions();
  if (texture.getType() != desc_.type ||
      (desc_.numMipLevels > 1 && texture.getNumMipLevels() != desc_.numMipLevels) ||
      texture.getNumLayers() != desc_.numLayers || dimensions.width != desc_.width ||
      dimensions.height != desc_.height || dimensions.depth != desc_.depth ||
      texture.getFormat() != desc_.format) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "Texture descriptor mismatch.");
    return;
  }

  uploadInternal(texture, outResult);
}

DataReader& ITextureLoader::reader() noexcept {
  return reader_;
}

const DataReader& ITextureLoader::reader() const noexcept {
  return reader_;
}

} // namespace iglu::textureloader
