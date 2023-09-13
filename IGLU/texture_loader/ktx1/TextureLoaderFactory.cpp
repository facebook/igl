/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/ktx1/TextureLoaderFactory.h>

#include <IGLU/texture_loader/ktx1/Header.h>
#include <vector>

namespace iglu::textureloader::ktx1 {
namespace {

class TextureLoader : public ITextureLoader {
  using Super = ITextureLoader;

 public:
  TextureLoader(DataReader reader,
                const igl::TextureRangeDesc& range,
                igl::TextureFormat format,
                std::vector<const uint8_t*> mipData) noexcept;

  [[nodiscard]] bool shouldGenerateMipmaps() const noexcept final;

 private:
  void uploadInternal(igl::ITexture& texture,
                      igl::Result* IGL_NULLABLE outResult) const noexcept final;

  std::vector<const uint8_t*> mipData_;
  bool shouldGenerateMipmaps_ = false;
};

TextureLoader::TextureLoader(DataReader reader,
                             const igl::TextureRangeDesc& range,
                             igl::TextureFormat format,
                             std::vector<const uint8_t*> mipData) noexcept :
  Super(reader), mipData_(std::move(mipData)), shouldGenerateMipmaps_(range.numMipLevels == 0) {
  auto& desc = mutableDescriptor();
  desc.format = format;
  desc.numMipLevels = range.numMipLevels;
  desc.numLayers = range.numLayers;
  desc.width = range.width;
  desc.height = range.height;
  desc.depth = range.depth;

  if (range.numFaces == 6u) {
    desc.type = igl::TextureType::Cube;
  } else if (desc.depth > 1) {
    desc.type = igl::TextureType::ThreeD;
  } else if (desc.numLayers > 1) {
    desc.type = igl::TextureType::TwoDArray;
  } else {
    desc.type = igl::TextureType::TwoD;
  }
}

bool TextureLoader::shouldGenerateMipmaps() const noexcept {
  return shouldGenerateMipmaps_;
}

void TextureLoader::uploadInternal(igl::ITexture& texture,
                                   igl::Result* IGL_NULLABLE outResult) const noexcept {
  const auto& desc = descriptor();

  for (size_t mipLevel = 0; mipLevel < desc.numMipLevels && mipLevel < mipData_.size();
       ++mipLevel) {
    texture.upload(texture.getFullRange(mipLevel), mipData_[mipLevel]);
  }

  igl::Result::setOk(outResult);
}
} // namespace

uint32_t TextureLoaderFactory::headerLength() const noexcept {
  return kHeaderLength;
}

bool TextureLoaderFactory::canCreateInternal(DataReader headerReader,
                                             igl::Result* IGL_NULLABLE outResult) const noexcept {
  if (headerReader.data() == nullptr) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentInvalid, "Reader's data is nullptr.");
    return false;
  }
  if (headerReader.length() < kHeaderLength) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentOutOfRange, "Not enough data for header.");
    return false;
  }

  const Header* header = headerReader.as<Header>();
  if (!header->tagIsValid()) {
    igl::Result::setResult(outResult, igl::Result::Code::InvalidOperation, "Incorrect identifier.");
    return false;
  }

  if (header->endianness != 0x04030201) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "Big endian not supported.");
    return false;
  }

  if (header->formatProperties().format == igl::TextureFormat::Invalid) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "Unrecognized texture format.");
    return false;
  }

  if (header->numberOfFaces == 6u && header->numberOfArrayElements > 1u) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "Texture cube arrays not supported.");
    return false;
  }
  if (header->numberOfArrayElements > 1 && header->pixelDepth > 1) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "3D texture arrays not supported.");
    return false;
  }

  return true;
}

std::unique_ptr<ITextureLoader> TextureLoaderFactory::tryCreateInternal(
    DataReader reader,
    igl::Result* IGL_NULLABLE outResult) const noexcept {
  const Header* header = reader.as<Header>();
  const uint32_t length = reader.length();

  if (header->bytesOfKeyValueData > length) {
    igl::Result::setResult(outResult, igl::Result::Code::InvalidOperation, "Length is too short.");
    return nullptr;
  }

  if (header->numberOfFaces != 1u && header->numberOfFaces != 6u) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "numberOfFaces must be 1 or 6.");
    return nullptr;
  }

  if (header->numberOfFaces == 6u && header->pixelDepth != 0) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "pixelDepth must be 0 for cube textures.");
    return nullptr;
  }

  if (header->numberOfFaces == 6u && header->pixelWidth != header->pixelHeight) {
    igl::Result::setResult(outResult,
                           igl::Result::Code::InvalidOperation,
                           "pixelWidth must match pixelHeight for cube textures.");
    return nullptr;
  }

  const auto properties = header->formatProperties();

  igl::TextureRangeDesc range;
  range.numMipLevels = std::max(header->numberOfMipmapLevels, 1u);
  range.numLayers = std::max(header->numberOfArrayElements, 1u);
  range.numFaces = header->numberOfFaces;
  range.width = std::max(header->pixelWidth, 1u);
  range.height = std::max(header->pixelHeight, 1u);
  range.depth = std::max(header->pixelDepth, 1u);

  auto result = range.validate();
  if (!result.isOk()) {
    igl::Result::setResult(outResult, std::move(result));
    return nullptr;
  }

  const size_t rangeBytesAsSizeT = properties.getBytesPerRange(range);
  if (rangeBytesAsSizeT > static_cast<size_t>(length)) {
    igl::Result::setResult(outResult, igl::Result::Code::InvalidOperation, "Length is too short.");
    return nullptr;
  }
  const uint32_t rangeBytes = static_cast<uint32_t>(rangeBytesAsSizeT);

  const uint32_t expectedLength =
      kHeaderLength + header->bytesOfKeyValueData +
      header->numberOfMipmapLevels * static_cast<uint32_t>(sizeof(uint32_t)) + rangeBytes;

  if (length < expectedLength) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "Length shorter than expected length.");
    return nullptr;
  }

  std::vector<const uint8_t*> mipData;
  mipData.reserve(range.numMipLevels);

  const bool isCubeTexture = header->numberOfFaces == 6u;

  uint32_t offset = kHeaderLength + header->bytesOfKeyValueData;
  for (size_t mipLevel = 0; mipLevel < range.numMipLevels; ++mipLevel) {
    const size_t imageSize = static_cast<size_t>(reader.readAt<uint32_t>(offset));
    const size_t expectedBytes = properties.getBytesPerRange(range.atMipLevel(mipLevel).atFace(0));
    const size_t expectedCubeBytes = expectedBytes * static_cast<size_t>(6);

    if (imageSize != expectedBytes && !(isCubeTexture && imageSize == expectedCubeBytes)) {
      igl::Result::setResult(
          outResult, igl::Result::Code::InvalidOperation, "Unexpected image size.");
      return nullptr;
    }
    offset += 4u;
    mipData.emplace_back(reader.at(offset));
    offset += static_cast<uint32_t>(isCubeTexture ? expectedCubeBytes : expectedBytes);
  }

  return std::make_unique<TextureLoader>(reader, range, properties.format, std::move(mipData));
}

} // namespace iglu::textureloader::ktx1
