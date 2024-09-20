/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/ktx2/TextureLoaderFactory.h>

#include <IGLU/texture_loader/ktx2/Header.h>
#include <igl/IGLSafeC.h>
#include <igl/vulkan/util/TextureFormat.h>
#include <ktx.h>
#include <numeric>
#include <vector>

namespace iglu::textureloader::ktx2 {
namespace {
template<typename T>
T align(T offset, T alignment) {
  return (offset + (alignment - 1)) & ~(alignment - 1);
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

  // vkFormat = 0 means basis universal or some non-Vulkan format.
  // In either case, we need to process the DFD to understand whether we can really handle the
  // format or not.
  if (header->vkFormat != 0 &&
      igl::vulkan::util::vkTextureFormatToTextureFormat(static_cast<int32_t>(header->vkFormat)) ==
          igl::TextureFormat::Invalid) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "Unrecognized texture format.");
    return false;
  }

  return true;
}

igl::TextureRangeDesc TextureLoaderFactory::textureRange(DataReader reader) const noexcept {
  const Header* header = reader.as<Header>();
  igl::TextureRangeDesc range;
  range.numMipLevels = std::max(header->levelCount, 1u);
  range.numLayers = std::max(header->layerCount, 1u);
  range.numFaces = header->faceCount;
  range.width = std::max(header->pixelWidth, 1u);
  range.height = std::max(header->pixelHeight, 1u);
  range.depth = std::max(header->pixelDepth, 1u);
  return range;
}

bool TextureLoaderFactory::validate(DataReader reader,
                                    const igl::TextureRangeDesc& range,
                                    igl::Result* IGL_NULLABLE outResult) const noexcept {
  const Header* header = reader.as<Header>();
  const uint32_t length = reader.length();

  if (header->sgdByteLength > std::numeric_limits<uint32_t>::max()) {
    igl::Result::setResult(outResult,
                           igl::Result::Code::InvalidOperation,
                           "Super compression global data is too large to fit in uint32_t.");
    return false;
  }
  const uint32_t sgdByteLength = static_cast<uint32_t>(header->sgdByteLength);

  if (static_cast<uint64_t>(header->dfdByteLength) + static_cast<uint64_t>(header->kvdByteLength) +
          header->sgdByteLength >
      static_cast<uint64_t>(length)) {
    igl::Result::setResult(outResult, igl::Result::Code::InvalidOperation, "Length is too short.");
    return false;
  }

  if (header->vkFormat != 0u) {
    const auto format =
        igl::vulkan::util::vkTextureFormatToTextureFormat(static_cast<int32_t>(header->vkFormat));
    const auto properties = igl::TextureFormatProperties::fromTextureFormat(format);

    const uint32_t mipLevelAlignment =
        std::lcm(static_cast<uint32_t>(properties.bytesPerBlock), 4u);

    size_t rangeBytesAsSizeT = 0;
    for (uint32_t mipLevel = 0; mipLevel < range.numMipLevels; ++mipLevel) {
      rangeBytesAsSizeT += align(properties.getBytesPerRange(range.atMipLevel(mipLevel)),
                                 static_cast<size_t>(mipLevelAlignment));
    }

    if (rangeBytesAsSizeT > length) {
      igl::Result::setResult(
          outResult, igl::Result::Code::InvalidOperation, "Length is too short.");
      return false;
    }
    const uint32_t rangeBytes = static_cast<uint32_t>(rangeBytesAsSizeT);

    // Mipmap metadata is:
    //   UInt64 byteOffset
    //   UInt64 byteLength
    //   UInt64 uncompressedByteLength
    const uint32_t mipmapMetadataLength = range.numMipLevels * 24u;

    const uint32_t preSupercompressionMetadataLength =
        kHeaderLength + mipmapMetadataLength + header->dfdByteLength + header->kvdByteLength;

    const uint32_t metadataLength =
        sgdByteLength > 0 ? align(preSupercompressionMetadataLength, 8u) + sgdByteLength
                          : preSupercompressionMetadataLength;

    uint32_t expectedDataOffset = align(metadataLength, mipLevelAlignment);

    const uint32_t expectedLength = expectedDataOffset + rangeBytes;
    if (length < expectedLength) {
      igl::Result::setResult(
          outResult, igl::Result::Code::InvalidOperation, "Length shorter than expected length.");
      return false;
    }

    for (uint32_t i = 0; i < range.numMipLevels; ++i) {
      // ktx2 stores actual mip data in 'reverse' order (smallest images to largest) but the
      // metadata in 'normal' order (largest to smallest). We process the list in the same order the
      // data is stored to simplify the bookkeeping validation.
      const uint32_t mipLevel = range.numMipLevels - i - 1;

      const uint32_t offset = kHeaderLength + static_cast<uint32_t>(mipLevel) * 24u;
      const uint64_t byteOffset = reader.readAt<uint64_t>(offset);
      const uint64_t byteLength = reader.readAt<uint64_t>(offset + 8u);
      const uint64_t uncompressedByteLength = reader.readAt<uint64_t>(offset + 16u);

      if (byteLength != uncompressedByteLength && header->supercompressionScheme == 0) {
        igl::Result::setResult(
            outResult,
            igl::Result::Code::InvalidOperation,
            "Unexpected difference between byteLength and uncompressedByteLength.");
        return false;
      }

      if (byteOffset != static_cast<uint64_t>(expectedDataOffset)) {
        igl::Result::setResult(
            outResult, igl::Result::Code::InvalidOperation, "Unexpected byteOffset.");
        return false;
      }

      if (static_cast<size_t>(uncompressedByteLength) !=
          properties.getBytesPerRange(range.atMipLevel(mipLevel))) {
        igl::Result::setResult(
            outResult, igl::Result::Code::InvalidOperation, "Unexpected byteLength.");
        return false;
      }

      expectedDataOffset =
          align(expectedDataOffset + static_cast<uint32_t>(byteLength), mipLevelAlignment);
    }
  }

  return true;
}

igl::TextureFormat TextureLoaderFactory::textureFormat(const ktxTexture* texture) const noexcept {
  if (texture->classId == ktxTexture2_c) {
    const auto* texture2 = reinterpret_cast<const ktxTexture2*>(texture);
    return igl::vulkan::util::vkTextureFormatToTextureFormat(
        static_cast<int32_t>(texture2->vkFormat));
  }

  return igl::TextureFormat::Invalid;
}
} // namespace iglu::textureloader::ktx2
