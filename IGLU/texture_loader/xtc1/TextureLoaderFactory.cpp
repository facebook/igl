/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/xtc1/TextureLoaderFactory.h>

#include <IGLU/texture_loader/xtc1/Header.h>
#include <algorithm>
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

// Header::numMips is a 4-bit field (max value 15) but Header::mipSizes is
// fixed at kMaxMips=12 entries. A malformed/truncated header that encodes
// 13..15 mips would index past the array. Returns 0 for legacy headers
// that did not populate numMips (so callers can fall back to mip-0-only
// behavior), and otherwise clamps to kMaxMips.
[[nodiscard]] constexpr uint32_t clampedNumMips(uint32_t rawNumMips) noexcept {
  return std::min(rawNumMips, Header::kMaxMips);
}

class TextureLoader final : public ITextureLoader {
 public:
  // NOLINTNEXTLINE(bugprone-exception-escape)
  explicit TextureLoader(DataReader reader) noexcept : ITextureLoader(reader) {
    const Header* header = this->reader().as<Header>();

    // Determine the appropriate XTC1 compressed format based on number of channels
    igl::TextureFormat format = getXTC1Format(header->numChannels);

    igl::TextureDesc desc = igl::TextureDesc::new2D(
        format, header->width, header->height, igl::TextureDesc::TextureUsageBits::Sampled);
    // Honor the pre-baked mip chain so downstream texture creation uploads
    // each mip directly instead of regenerating from the base mip at runtime.
    // Legacy headers leave numMips==0; keep desc.numMipLevels at its default
    // of 1 so the mip chain is generated at runtime as it was previously.
    const uint32_t numMips = clampedNumMips(header->numMips);
    if (numMips > 0) {
      desc.numMipLevels = numMips;
    }

    mutableDescriptor() = desc;
  }

  // NOLINTNEXTLINE(bugprone-exception-escape)
  [[nodiscard]] std::vector<uint32_t> mipLevelBytes() const noexcept override {
    const Header* header = this->reader().as<Header>();
    const uint32_t numMips = clampedNumMips(header->numMips);
    // Legacy headers without a pre-baked chain match the base class behavior
    // (empty result) so consumers using mipLevelBytes().empty() as a signal
    // to generate mips at runtime keep working.
    std::vector<uint32_t> result;
    result.reserve(numMips);
    for (uint32_t i = 0; i < numMips; ++i) {
      result.push_back(header->mipSizes[i]);
    }
    return result;
  }

  // Override the base class heuristic. The base returns true whenever
  // numMipLevels > 1, which would tell the renderer to regenerate the chain
  // from mip 0 and skip the pre-baked uploads. We only want runtime
  // generation when the source file omits the chain (legacy numMips==0).
  [[nodiscard]] bool shouldGenerateMipmaps() const noexcept override {
    return clampedNumMips(this->reader().as<Header>()->numMips) == 0;
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
    auto err = try_checked_memcpy(
        static_cast<uint8_t*>(data.get()), compressedSize, compressedData, compressedSize);
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

// NOLINTNEXTLINE(bugprone-exception-escape)
bool TextureLoaderFactory::canCreateInternal(DataReader headerReader,
                                             igl::Result* IGL_NULLABLE outResult) const noexcept {
  if (headerReader.size() < kHeaderLength) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentInvalid, "Header too small for XTC1 texture");
    return false;
  }

  const Header* header = headerReader.as<Header>();
  if (!header->tagIsValid()) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentInvalid, "Invalid XTC1 texture header");
    return false;
  }
  // numMips is 4-bit (max 15) but mipSizes[] has only kMaxMips=12 slots.
  // Reject malformed headers that claim more mips than the array can hold.
  if (header->numMips > Header::kMaxMips) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentInvalid, "XTC1 header numMips exceeds kMaxMips");
    return false;
  }
  return true;
}

// NOLINTNEXTLINE(bugprone-exception-escape)
std::unique_ptr<ITextureLoader> TextureLoaderFactory::tryCreateInternal(
    DataReader reader,
    igl::TextureFormat preferredFormat,
    igl::Result* IGL_NULLABLE outResult) const noexcept {
  if (reader.size() < kHeaderLength) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentInvalid, "Data too small for XTC1 texture");
    return nullptr;
  }

  // Reject corrupted files where the declared per-mip byte sizes do not fit
  // in the payload that follows the header. Without this check, downstream
  // consumers walk past the buffer when they treat mipSizes as sequential
  // offsets into the data blob.
  const Header* header = reader.as<Header>();
  const uint32_t numMips = clampedNumMips(header->numMips);
  uint64_t totalMipBytes = 0;
  for (uint32_t i = 0; i < numMips; ++i) {
    totalMipBytes += header->mipSizes[i];
  }
  const uint64_t payloadSize = static_cast<uint64_t>(reader.size()) - kHeaderLength;
  if (totalMipBytes > payloadSize) {
    igl::Result::setResult(
        outResult, igl::Result::Code::ArgumentInvalid, "XTC1 mipSizes sum exceeds payload size");
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
