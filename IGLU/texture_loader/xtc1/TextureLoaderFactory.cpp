/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/xtc1/TextureLoaderFactory.h>

#include <IGLU/texture_loader/xtc1/Header.h>
#include <cstring>
#include <memory>
#if defined(IGL_CMAKE_BUILD)
#include <igl/IGLSafeC.h>
#else
#include <secure_lib/secure_string.h>
#endif

namespace iglu::textureloader::xtc1 {

#if !defined(IGL_CMAKE_BUILD)
namespace {

// Helper function to determine XTC1 texture format based on number of channels
igl::TextureFormat getXTC1Format(uint32_t numChannels) {
  switch (numChannels) {
  case 1:
    // @fb-only
  case 3:
    // @fb-only
  case 4:
    // @fb-only
  default:
    return igl::TextureFormat::Invalid;
  }
}

class TextureLoader final : public ITextureLoader {
 public:
  explicit TextureLoader(DataReader reader) noexcept : ITextureLoader(reader) {
    const Header* header = this->reader().as<Header>();

    // Determine the appropriate XTC1 compressed format based on number of channels
    igl::TextureFormat format = getXTC1Format(header->numChannels);

    igl::TextureDesc desc = igl::TextureDesc::new2D(
        format, header->width, header->height, igl::TextureDesc::TextureUsageBits::Sampled);

    mutableDescriptor() = desc;
  }

 protected:
  [[nodiscard]] std::unique_ptr<IData> loadInternal(
      igl::Result* IGL_NULLABLE outResult) const noexcept override {
    const uint8_t* compressedData = reader().data() + sizeof(Header);

    // Calculate the size of compressed data
    // For XTC1, each 16x16 block is 128 bytes, but the actual size varies due to variable length
    // encoding. We use the total file size minus the header size.
    const uint32_t compressedSize = reader().size() - sizeof(Header);

    // Return the compressed data as-is without decompression
    auto data = std::make_unique<uint8_t[]>(compressedSize);
    auto err =
        try_checked_memcpy((uint8_t*)data.get(), compressedSize, compressedData, compressedSize);
    if (err != 0) {
      IGL_LOG_ERROR_ONCE("[IGL][Error] Failed to update texture buffer\n");
    }

    return IData::tryCreate(std::move(data), compressedSize, outResult);
  }
};

} // namespace

uint32_t TextureLoaderFactory::minHeaderLength() const noexcept {
  return kHeaderLength;
}

bool TextureLoaderFactory::canCreateInternal(DataReader headerReader,
                                             igl::Result* IGL_NULLABLE outResult) const noexcept {
  if (headerReader.size() < kHeaderLength) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentInvalid, "Header too small for XTC1 texture");
    return false;
  }

  const Header* header = headerReader.as<Header>();
  const bool isValid = header->tagIsValid();

  if (!isValid) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentInvalid, "Invalid XTC1 texture header");
  }

  return isValid;
}

std::unique_ptr<ITextureLoader> TextureLoaderFactory::tryCreateInternal(
    DataReader reader,
    igl::TextureFormat preferredFormat,
    igl::Result* IGL_NULLABLE outResult) const noexcept {
  if (reader.size() < kHeaderLength) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentInvalid, "Data too small for XTC1 texture");
    return nullptr;
  }

  return std::make_unique<TextureLoader>(reader);
}

#else
// Stub implementations for open source builds
uint32_t TextureLoaderFactory::minHeaderLength() const noexcept {
  return 0;
}

bool TextureLoaderFactory::canCreateInternal(DataReader headerReader,
                                             igl::Result* IGL_NULLABLE outResult) const noexcept {
  igl::Result::setResult(
      outResult, igl::Result::Code::Unsupported, "XTC1 texture format not supported in this build");
  return false;
}

std::unique_ptr<ITextureLoader> TextureLoaderFactory::tryCreateInternal(
    DataReader reader,
    igl::TextureFormat preferredFormat,
    igl::Result* IGL_NULLABLE outResult) const noexcept {
  igl::Result::setResult(
      outResult, igl::Result::Code::Unsupported, "XTC1 texture format not supported in this build");
  return nullptr;
}
#endif

} // namespace iglu::textureloader::xtc1
