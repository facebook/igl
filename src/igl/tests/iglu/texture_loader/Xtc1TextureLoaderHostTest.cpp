/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#if !defined(IGL_CMAKE_BUILD)

#include <IGLU/texture_loader/DataReader.h>
#include <IGLU/texture_loader/IData.h>
#include <IGLU/texture_loader/xtc1/Header.h>
#include <IGLU/texture_loader/xtc1/TextureLoaderFactory.h>
#include <cstdint>
#include <vector>
#include <igl/Common.h>

// These tests exercise the host-coverable CPU header-parsing and passthrough
// paths of the XTC1 TextureLoaderFactory using hand-built headers and byte
// buffers. They do not compress real data and do not require a GPU device, so
// they run on every host unit-test backend.
namespace igl::tests::xtc1 {

using Header = iglu::textureloader::xtc1::Header;
using DataReader = iglu::textureloader::DataReader;
using TextureLoaderFactory = iglu::textureloader::xtc1::TextureLoaderFactory;

namespace {

constexpr uint32_t kPayloadSize = 16u;

// Builds an XTC1 blob: a populated Header followed by `payloadSize` zeroed
// payload bytes. The buffer is zero-initialized and the Header is written over
// the raw bytes, so every field the loader inspects must be set explicitly here
// (the struct's default member initializers do not run for a placed-over cast).
std::vector<uint8_t> buildXtc1Blob(uint32_t width,
                                   uint32_t height,
                                   uint32_t numChannels,
                                   uint32_t numMips,
                                   const std::vector<uint32_t>& mipSizes,
                                   uint32_t payloadSize) {
  std::vector<uint8_t> blob(sizeof(Header) + payloadSize);
  auto* header = reinterpret_cast<Header*>(blob.data());
  header->magicTag = {0x49, 0x56, 0x41, 0x4e};
  header->width = width;
  header->height = height;
  header->numChannels = numChannels;
  header->numMips = numMips;
  for (uint32_t i = 0; i < mipSizes.size() && i < Header::kMaxMips; ++i) {
    header->mipSizes[i] = mipSizes[i];
  }
  return blob;
}

} // namespace

class Xtc1TextureLoaderHostTest : public ::testing::Test {
 protected:
  TextureLoaderFactory factory_;
};

TEST_F(Xtc1TextureLoaderHostTest, MinHeaderLengthMatchesHeaderSize) {
  EXPECT_EQ(factory_.minHeaderLength(), iglu::textureloader::xtc1::kHeaderLength);
}

TEST_F(Xtc1TextureLoaderHostTest, CanCreateSucceedsWithValidHeader) {
  const auto blob = buildXtc1Blob(64u, 64u, 4u, 1u, {kPayloadSize}, kPayloadSize);

  Result ret;
  auto reader = *DataReader::tryCreate(blob.data(), static_cast<uint32_t>(blob.size()), nullptr);
  EXPECT_TRUE(factory_.canCreate(reader, &ret)) << ret.message;
  EXPECT_TRUE(ret.isOk()) << ret.message;
}

TEST_F(Xtc1TextureLoaderHostTest, TryCreateSucceedsWithValidHeader) {
  const uint32_t width = 64u;
  const uint32_t height = 64u;
  const auto blob = buildXtc1Blob(width, height, 4u, 1u, {kPayloadSize}, kPayloadSize);

  Result ret;
  auto reader = *DataReader::tryCreate(blob.data(), static_cast<uint32_t>(blob.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  ASSERT_NE(loader, nullptr) << ret.message;
  EXPECT_TRUE(ret.isOk()) << ret.message;

  const auto& desc = loader->descriptor();
  EXPECT_EQ(desc.width, width);
  EXPECT_EQ(desc.height, height);
  EXPECT_EQ(desc.numMipLevels, 1u);
}

TEST_F(Xtc1TextureLoaderHostTest, LoadReturnsCompressedPayload) {
  const auto blob = buildXtc1Blob(64u, 64u, 4u, 1u, {kPayloadSize}, kPayloadSize);

  Result ret;
  auto reader = *DataReader::tryCreate(blob.data(), static_cast<uint32_t>(blob.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  ASSERT_NE(loader, nullptr) << ret.message;

  auto data = loader->load(&ret);
  ASSERT_NE(data, nullptr) << ret.message;
  EXPECT_TRUE(ret.isOk()) << ret.message;
  EXPECT_EQ(data->size(), static_cast<uint64_t>(kPayloadSize));
}

TEST_F(Xtc1TextureLoaderHostTest, TryCreateFailsWhenNumMipsExceedsMax) {
  const uint32_t tooManyMips = Header::kMaxMips + 1u;
  const auto blob = buildXtc1Blob(64u, 64u, 4u, tooManyMips, {}, kPayloadSize);

  Result ret;
  auto reader = *DataReader::tryCreate(blob.data(), static_cast<uint32_t>(blob.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_EQ(loader, nullptr);
  EXPECT_FALSE(ret.isOk());
}

TEST_F(Xtc1TextureLoaderHostTest, TryCreateFailsWhenMipSizesExceedPayload) {
  const std::vector<uint32_t> oversizedMips = {kPayloadSize * 100u, kPayloadSize * 100u};
  const auto blob = buildXtc1Blob(64u, 64u, 4u, 2u, oversizedMips, kPayloadSize);

  Result ret;
  auto reader = *DataReader::tryCreate(blob.data(), static_cast<uint32_t>(blob.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_EQ(loader, nullptr);
  EXPECT_FALSE(ret.isOk());
}

} // namespace igl::tests::xtc1

#endif // IGL_CMAKE_BUILD
