/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/ktx1/TextureLoaderFactory.h>

#include <IGLU/texture_loader/ktx1/Header.h>
#include <ktx.h>
#include <igl/opengl/util/TextureFormat.h>

namespace iglu::textureloader::ktx1 {

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

  if (igl::opengl::util::glTextureFormatToTextureFormat(
          header->glInternalFormat, header->glFormat, header->glType) ==
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
  range.numMipLevels = std::max(header->numberOfMipmapLevels, 1u);
  range.numLayers = std::max(header->numberOfArrayElements, 1u);
  range.numFaces = header->numberOfFaces;
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

  const auto format = igl::opengl::util::glTextureFormatToTextureFormat(
      header->glInternalFormat, header->glFormat, header->glType);
  const auto properties = igl::TextureFormatProperties::fromTextureFormat(format);

  const size_t rangeBytesAsSizeT = properties.getBytesPerRange(range);
  if (rangeBytesAsSizeT > static_cast<size_t>(length)) {
    igl::Result::setResult(outResult, igl::Result::Code::InvalidOperation, "Length is too short.");
    return false;
  }
  const uint32_t rangeBytes = static_cast<uint32_t>(rangeBytesAsSizeT);

  const uint32_t expectedLength =
      kHeaderLength + header->bytesOfKeyValueData +
      header->numberOfMipmapLevels * static_cast<uint32_t>(sizeof(uint32_t)) + rangeBytes;

  if (length < expectedLength) {
    igl::Result::setResult(
        outResult, igl::Result::Code::InvalidOperation, "Length shorter than expected length.");
    return false;
  }

  const bool isCubeTexture = header->numberOfFaces == 6u;

  uint32_t offset = kHeaderLength + header->bytesOfKeyValueData;
  for (size_t mipLevel = 0; mipLevel < range.numMipLevels; ++mipLevel) {
    uint32_t imageSize = 0;
    if (!reader.tryReadAt<uint32_t>(offset, imageSize, outResult)) {
      return false;
    }
    const size_t expectedBytes = properties.getBytesPerRange(range.atMipLevel(mipLevel).atFace(0));
    const size_t expectedCubeBytes = expectedBytes * static_cast<size_t>(6);

    if (imageSize != expectedBytes) {
      igl::Result::setResult(
          outResult, igl::Result::Code::InvalidOperation, "Unexpected image size.");
      return false;
    }
    offset += 4u;
    offset += static_cast<uint32_t>(isCubeTexture ? expectedCubeBytes : expectedBytes);
  }

  return true;
}

igl::TextureFormat TextureLoaderFactory::textureFormat(const ktxTexture* texture) const noexcept {
  if (texture->classId == ktxTexture1_c) {
    const auto* texture1 = reinterpret_cast<const ktxTexture1*>(texture);
    return igl::opengl::util::glTextureFormatToTextureFormat(
        texture1->glInternalformat, texture1->glFormat, texture1->glType);
  }

  return igl::TextureFormat::Invalid;
}

} // namespace iglu::textureloader::ktx1
