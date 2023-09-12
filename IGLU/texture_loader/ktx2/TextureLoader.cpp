/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/ktx2/TextureLoader.h>

#include <IGLU/texture_loader/ktx2/Header.h>
#include <numeric>

namespace iglu::textureloader::ktx2 {
namespace {
template<typename T>
T align(T offset, T alignment) {
  return (offset + (alignment - 1)) & ~(alignment - 1);
}
} // namespace

bool TextureLoader::isHeaderValid(DataReader reader, igl::Result* IGL_NULLABLE outResult) noexcept {
  if (reader.data() == nullptr) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentInvalid, "Reader's data is nullptr.");
    return false;
  }
  if (reader.length() < kHeaderLength) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentOutOfRange, "Not enough data for header.");
    return false;
  }

  const Header* header = reader.as<Header>();
  if (!header->tagIsValid()) {
    igl::Result::setResult(outResult, igl::Result::Code::InvalidOperation, "Incorrect identifier.");
    return false;
  }

  if (header->vkFormat == 0u) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "Basis universal texures not supported.");
    return false;
  }

  if (header->formatProperties().format == igl::TextureFormat::Invalid) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "Unrecognized texture format.");
    return false;
  }

  if (header->faceCount != 1u && header->faceCount != 6u) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "faceCount must be 1 or 6.");
    return false;
  }

  if (header->faceCount == 6u && header->pixelDepth != 0) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "pixelDepth must be 0 for cube textures.");
    return false;
  }

  if (header->faceCount == 6u && header->pixelWidth != header->pixelHeight) {
    igl::Result::setResult(outResult,
                           igl::Result::Code::InvalidOperation,
                           "pixelWidth must match pixelHeight for cube textures.");
    return false;
  }

  if (header->faceCount == 6u && header->layerCount > 1u) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "Texture cube arrays not supported.");
    return false;
  }
  if (header->layerCount > 1 && header->pixelDepth > 1) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "3D texture arrays not supported.");
    return false;
  }

  if (header->supercompressionScheme != 0) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "Supercompression not supported.");
    return false;
  }

  if (header->sgdByteLength > std::numeric_limits<uint32_t>::max()) {
    igl::Result::setResult(outResult,
                           igl::Result::Code::InvalidOperation,
                           "Super compression global data is too large to fit in uint32_t.");
    return false;
  }

  const uint32_t width = std::max(header->pixelWidth, 1u);
  const uint32_t height = std::max(header->pixelHeight, 1u);
  const uint32_t depth = std::max(header->pixelDepth, 1u);
  const uint32_t maxMipLevels = igl::TextureDesc::calcNumMipLevels(width, height, depth);
  if (header->levelCount > maxMipLevels) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "Too many mipmap levels.");
    return false;
  }

  return true;
}

std::unique_ptr<TextureLoader> TextureLoader::tryCreate(DataReader reader,
                                                        igl::Result* IGL_NULLABLE
                                                            outResult) noexcept {
  if (!isHeaderValid(reader, outResult)) {
    return nullptr;
  }

  const Header* header = reader.as<Header>();
  const uint32_t length = reader.length();

  const uint32_t sgdByteLength = static_cast<uint32_t>(header->sgdByteLength);

  if (static_cast<uint64_t>(header->dfdByteLength) + static_cast<uint64_t>(header->kvdByteLength) +
          header->sgdByteLength >
      static_cast<uint64_t>(length)) {
    igl::Result::setResult(outResult, igl::Result::Code::InvalidOperation, "Length is too short.");
    return nullptr;
  }

  const auto properties = header->formatProperties();

  igl::TextureRangeDesc range;
  range.numMipLevels = std::max(header->levelCount, 1u);
  range.numLayers = std::max(header->layerCount, 1u);
  range.numFaces = header->faceCount;
  range.width = std::max(header->pixelWidth, 1u);
  range.height = std::max(header->pixelHeight, 1u);
  range.depth = std::max(header->pixelDepth, 1u);

  const uint32_t mipLevelAlignment = std::lcm(static_cast<uint32_t>(properties.bytesPerBlock), 4u);

  size_t rangeBytesAsSizeT = 0;
  for (size_t mipLevel = 0; mipLevel < range.numMipLevels; ++mipLevel) {
    rangeBytesAsSizeT += align(properties.getBytesPerRange(range.atMipLevel(mipLevel)),
                               static_cast<size_t>(mipLevelAlignment));
  }

  if (rangeBytesAsSizeT > static_cast<size_t>(length)) {
    igl::Result::setResult(outResult, igl::Result::Code::InvalidOperation, "Length is too short.");
    return nullptr;
  }
  const uint32_t rangeBytes = static_cast<uint32_t>(rangeBytesAsSizeT);

  // Mipmap metadata is:
  //   UInt64 byteOffset
  //   UInt64 byteLength
  //   UInt64 uncompressedByteLength
  const uint32_t mipmapMetadataLength = static_cast<uint32_t>(range.numMipLevels) * 24u;

  const uint32_t preSupercompressionMetadataLength =
      kHeaderLength + mipmapMetadataLength + header->dfdByteLength + header->kvdByteLength;

  const uint32_t metadataLength = sgdByteLength > 0
                                      ? align(preSupercompressionMetadataLength, 8u) + sgdByteLength
                                      : preSupercompressionMetadataLength;

  uint32_t expectedDataOffset = align(metadataLength, mipLevelAlignment);

  const uint32_t expectedLength = expectedDataOffset + rangeBytes;
  if (length < expectedLength) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "Length shorter than expected length.");
    return nullptr;
  }

  std::vector<const uint8_t*> mipData;
  mipData.resize(range.numMipLevels);

  for (size_t i = 0; i < range.numMipLevels; ++i) {
    // ktx2 stores actual mip data in 'reverse' order (smallest images to largest) but the metadata
    // in 'normal' order (largest to smallest).
    // We process the list in the same order the data is stored to simplify the bookkeeping
    // validation.
    const size_t mipLevel = range.numMipLevels - i - 1;

    const uint32_t offset = kHeaderLength + static_cast<uint32_t>(mipLevel) * 24u;
    const uint64_t byteOffset = reader.readAt<uint64_t>(offset);
    const uint64_t byteLength = reader.readAt<uint64_t>(offset + 8u);
    const uint64_t uncompressedByteLength = reader.readAt<uint64_t>(offset + 16u);

    if (byteLength != uncompressedByteLength) {
      igl::Result::setResult(
          outResult, igl::Result::Code::InvalidOperation, "Supercompression not supported.");
      return nullptr;
    }

    if (byteOffset != static_cast<uint64_t>(expectedDataOffset)) {
      igl::Result::setResult(
          outResult, igl::Result::Code::InvalidOperation, "Unexpected byteOffset.");
      return nullptr;
    }

    if (static_cast<size_t>(byteLength) !=
        properties.getBytesPerRange(range.atMipLevel(mipLevel))) {
      igl::Result::setResult(
          outResult, igl::Result::Code::InvalidOperation, "Unexpected byteLength.");
      return nullptr;
    }

    // @fb-only
    mipData[mipLevel] = reader.at(expectedDataOffset);
    expectedDataOffset =
        align(expectedDataOffset + static_cast<uint32_t>(byteLength), mipLevelAlignment);
  }

  // Explicitly call new to support private constructor
  return std::unique_ptr<TextureLoader>(
      new TextureLoader(reader, *header, properties.format, std::move(mipData)));
}

TextureLoader::TextureLoader(DataReader reader,
                             const Header& header,
                             igl::TextureFormat format,
                             std::vector<const uint8_t*> mipData) noexcept :
  Super(reader), mipData_(std::move(mipData)), shouldGenerateMipmaps_(header.levelCount == 0) {
  auto& desc = mutableDescriptor();
  desc.format = format;
  desc.numMipLevels = std::max(header.levelCount, 1u);
  desc.numLayers = std::max(header.layerCount, 1u);
  desc.width = std::max(header.pixelWidth, 1u);
  desc.height = std::max(header.pixelHeight, 1u);
  desc.depth = std::max(header.pixelDepth, 1u);

  if (header.faceCount == 6u) {
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

  for (size_t mipLevel = 0; mipLevel < desc.numMipLevels; ++mipLevel) {
    texture.upload(texture.getFullRange(mipLevel), mipData_[mipLevel]);
  }

  igl::Result::setOk(outResult);
}

} // namespace iglu::textureloader::ktx2
