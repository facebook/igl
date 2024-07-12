/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/ITextureLoader.h>

#include <IGLU/texture_loader/IData.h>
#include <igl/Device.h>
#include <igl/IGLSafeC.h>

namespace iglu::textureloader {

/// Interface for getting CPU access to GPU texture data
ITextureLoader::ITextureLoader(DataReader reader, igl::TextureDesc::TextureUsage usage) noexcept :
  reader_(reader) {
  IGL_ASSERT(reader.data() != nullptr && reader.length() > 0);
  desc_.usage = usage;
}

igl::TextureDesc& ITextureLoader::mutableDescriptor() noexcept {
  return desc_;
}

const igl::TextureDesc& ITextureLoader::descriptor() const noexcept {
  return desc_;
}

[[nodiscard]] uint32_t ITextureLoader::memorySizeInBytes() const noexcept {
  const auto properties = igl::TextureFormatProperties::fromTextureFormat(desc_.format);
  igl::TextureRangeDesc range;
  range.width = desc_.width;
  range.height = desc_.height;
  range.depth = desc_.depth;
  range.numFaces = desc_.type == igl::TextureType::Cube ? static_cast<size_t>(6)
                                                        : static_cast<size_t>(1);
  range.numLayers = desc_.numLayers;
  range.numMipLevels = desc_.numMipLevels;

  return static_cast<uint32_t>(properties.getBytesPerRange(range));
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
  return create(device, desc_.format, desc_.usage, outResult);
}

std::shared_ptr<igl::ITexture> ITextureLoader::create(const igl::IDevice& device,
                                                      igl::TextureFormat preferredFormat,
                                                      igl::Result* IGL_NULLABLE
                                                          outResult) const noexcept {
  return create(device, preferredFormat, desc_.usage, outResult);
}

std::shared_ptr<igl::ITexture> ITextureLoader::create(const igl::IDevice& device,
                                                      igl::TextureDesc::TextureUsage usage,
                                                      igl::Result* IGL_NULLABLE
                                                          outResult) const noexcept {
  return create(device, igl::TextureFormat::Invalid, usage, outResult);
}

std::shared_ptr<igl::ITexture> ITextureLoader::create(const igl::IDevice& device,
                                                      igl::TextureFormat preferredFormat,
                                                      igl::TextureDesc::TextureUsage usage,
                                                      igl::Result* IGL_NULLABLE
                                                          outResult) const noexcept {
  igl::TextureDesc desc = desc_;
  desc.format = preferredFormat == igl::TextureFormat::Invalid ? desc_.format : preferredFormat;
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

std::unique_ptr<IData> ITextureLoader::load(igl::Result* IGL_NULLABLE outResult) const noexcept {
  return loadInternal(outResult);
}

void ITextureLoader::loadToExternalMemory(uint8_t* IGL_NONNULL data,
                                          uint32_t length,
                                          igl::Result* IGL_NULLABLE outResult) const noexcept {
  if (data == nullptr) {
    igl::Result::setResult(outResult, igl::Result::Code::ArgumentNull, "data is nullptr.");
    return;
  }
  if (length < memorySizeInBytes()) {
    igl::Result::setResult(outResult, igl::Result::Code::ArgumentInvalid, "length is too short.");
    return;
  }

  return loadToExternalMemoryInternal(data, length, outResult);
}

DataReader& ITextureLoader::reader() noexcept {
  return reader_;
}

const DataReader& ITextureLoader::reader() const noexcept {
  return reader_;
}

void ITextureLoader::defaultUpload(igl::ITexture& texture,
                                   igl::Result* IGL_NULLABLE outResult) const noexcept {
  std::unique_ptr<IData> data;

  if (!canUploadSourceData()) {
    data = load(outResult);
    if (!data) {
      return;
    }
  }

  const auto range = shouldGenerateMipmaps() ? texture.getFullRange() : texture.getFullMipRange();
  auto result = texture.upload(range, data ? data->data() : reader_.data());
  igl::Result::setResult(outResult, std::move(result));
}

std::unique_ptr<IData> ITextureLoader::defaultLoad(
    igl::Result* IGL_NULLABLE outResult) const noexcept {
  const uint32_t length = memorySizeInBytes();
  auto data = std::make_unique<uint8_t[]>(length);
  if (!data) {
    igl::Result::setResult(outResult, igl::Result::Code::RuntimeError, "out of memory.");
    return nullptr;
  }

  loadToExternalMemory(data.get(), length, outResult);

  return IData::tryCreate(std::move(data), length, outResult);
}

void ITextureLoader::defaultLoadToExternalMemory(uint8_t* IGL_NONNULL data,
                                                 uint32_t length,
                                                 igl::Result* IGL_NULLABLE
                                                     outResult) const noexcept {
  if (reader_.length() != length) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentInvalid, "length doesn't match reader length.");
    return;
  }
  checked_memcpy(data, length, reader_.data(), length);
}

} // namespace iglu::textureloader
