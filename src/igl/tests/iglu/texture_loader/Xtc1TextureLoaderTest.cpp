/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Config.h>

#if !defined(IGL_CMAKE_BUILD)
// @fb-only

#include <gtest/gtest.h>

#include <IGLU/texture_loader/xtc1/Header.h>
#include <IGLU/texture_loader/xtc1/TextureLoaderFactory.h>
#include <cstring>
#include <memory>
#include <vector>
// @fb-only
// @fb-only
// @fb-only

namespace igl::tests::xtc1 {

namespace {

std::vector<uint8_t> createXtc1TextureData(uint32_t width, uint32_t height) {
  // Create a simple RGBA8 test pattern
  const uint32_t numPixels = width * height;
  const uint32_t rgbaDataSize = numPixels * 4;
  std::vector<uint8_t> rgbaData(rgbaDataSize);

  // Create a checkerboard pattern
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      const uint32_t offset = (y * width + x) * 4;
      const bool isWhite = ((x / 8) + (y / 8)) % 2 == 0;
      rgbaData[offset + 0] = isWhite ? 255 : 0; // R
      rgbaData[offset + 1] = isWhite ? 255 : 0; // G
      rgbaData[offset + 2] = isWhite ? 255 : 0; // B
      rgbaData[offset + 3] = 255; // A
    }
  }

  // @fb-only
  // @fb-only
  // @fb-only
                                           // @fb-only
                                           // @fb-only
                                           // @fb-only
                                           // @fb-only
                                           // @fb-only

  // @fb-only
    // @fb-only
  // @fb-only

  // @fb-only
  // @fb-only

  // Create the full texture data with header
  const uint32_t totalSize = sizeof(iglu::textureloader::xtc1::Header) + compressedSize;
  std::vector<uint8_t> textureData(totalSize);

  // Write header
  auto* header = reinterpret_cast<iglu::textureloader::xtc1::Header*>(textureData.data());
  header->magicTag = {0x49, 0x56, 0x41, 0x4e};
  header->width = width;
  header->height = height;
  header->numChannels = 4;
  header->lossless = 0;
  header->impasto = 1;
  header->numMips = 1;
  header->mipSizes[0] = static_cast<uint32_t>(compressedSize);

  // Write compressed data
  // @fb-only
              // @fb-only
              // @fb-only

  // @fb-only

  return textureData;
}

} // namespace

class Xtc1TextureLoaderTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);
  }

  void TearDown() override {}

 protected:
  iglu::textureloader::xtc1::TextureLoaderFactory factory_;
};

TEST_F(Xtc1TextureLoaderTest, HeaderTagValidation) {
  // Test valid tag
  iglu::textureloader::xtc1::Header validHeader;
  validHeader.magicTag = {0x49, 0x56, 0x41, 0x4e};
  EXPECT_TRUE(validHeader.tagIsValid());

  // Test invalid tag
  iglu::textureloader::xtc1::Header invalidHeader;
  invalidHeader.magicTag = {0x00, 0x00, 0x00, 0x00};
  EXPECT_FALSE(invalidHeader.tagIsValid());
}

TEST_F(Xtc1TextureLoaderTest, EmptyBuffer_Fails) {
  // Create a buffer that's too small (less than header size)
  std::vector<uint8_t> buffer(4); // Only 4 bytes, need at least 8 for header

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      buffer.data(), static_cast<uint32_t>(buffer.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_EQ(loader, nullptr);
  EXPECT_FALSE(ret.isOk());
}

TEST_F(Xtc1TextureLoaderTest, MinimalHeader_Succeeds) {
  const uint32_t width = 64u;
  const uint32_t height = 64u;

  auto textureData = createXtc1TextureData(width, height);
  ASSERT_FALSE(textureData.empty());

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      textureData.data(), static_cast<uint32_t>(textureData.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_NE(loader, nullptr);
  EXPECT_TRUE(ret.isOk()) << ret.message;

  if (loader) {
    const auto& desc = loader->descriptor();
    EXPECT_EQ(desc.width, width);
    EXPECT_EQ(desc.height, height);
    // XTC1 textures use compressed format, not uncompressed RGBA8
    // @fb-only
  }
}

TEST_F(Xtc1TextureLoaderTest, InvalidHeader_Fails) {
  std::vector<uint8_t> buffer(sizeof(iglu::textureloader::xtc1::Header));
  auto* header = reinterpret_cast<iglu::textureloader::xtc1::Header*>(buffer.data());
  header->width = 0; // Invalid width
  header->height = 64;

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      buffer.data(), static_cast<uint32_t>(buffer.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_EQ(loader, nullptr);
  EXPECT_FALSE(ret.isOk());
}

TEST_F(Xtc1TextureLoaderTest, ExcessiveWidth_Fails) {
  std::vector<uint8_t> buffer(sizeof(iglu::textureloader::xtc1::Header));
  auto* header = reinterpret_cast<iglu::textureloader::xtc1::Header*>(buffer.data());
  // @fb-only
  header->height = 64;

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      buffer.data(), static_cast<uint32_t>(buffer.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_EQ(loader, nullptr);
  EXPECT_FALSE(ret.isOk());
}

TEST_F(Xtc1TextureLoaderTest, LoadData_Succeeds) {
  const uint32_t width = 64u;
  const uint32_t height = 64u;

  auto textureData = createXtc1TextureData(width, height);
  ASSERT_FALSE(textureData.empty());

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      textureData.data(), static_cast<uint32_t>(textureData.size()), nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  ASSERT_NE(loader, nullptr);
  ASSERT_TRUE(ret.isOk());

  auto data = loader->load(&ret);
  EXPECT_NE(data, nullptr);
  EXPECT_TRUE(ret.isOk()) << ret.message;

  if (data) {
    // XTC1 textures return compressed data, not decompressed
    // The size should be the compressed size, which is less than uncompressed
    const uint32_t compressedSize =
        static_cast<uint32_t>(textureData.size()) - sizeof(iglu::textureloader::xtc1::Header);
    EXPECT_EQ(data->size(), compressedSize);
    // Verify compressed data is smaller than uncompressed (64x64 RGBA8 = 16384 bytes)
    EXPECT_LT(data->size(), width * height * 4);
  }
}

TEST_F(Xtc1TextureLoaderTest, MinHeaderLength) {
  // Test that minHeaderLength returns the correct value
  auto minLength = factory_.minHeaderLength();
  EXPECT_EQ(minLength, sizeof(iglu::textureloader::xtc1::Header));
}

TEST_F(Xtc1TextureLoaderTest, CanCreateWithValidHeader) {
  const uint32_t width = 64u;
  const uint32_t height = 64u;

  auto textureData = createXtc1TextureData(width, height);
  ASSERT_FALSE(textureData.empty());

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      textureData.data(), static_cast<uint32_t>(textureData.size()), nullptr);
  EXPECT_TRUE(factory_.canCreate(reader, &ret));
  EXPECT_TRUE(ret.isOk());
}

TEST_F(Xtc1TextureLoaderTest, CanCreateFailsWithInvalidTag) {
  std::vector<uint8_t> buffer(sizeof(iglu::textureloader::xtc1::Header));
  auto* header = reinterpret_cast<iglu::textureloader::xtc1::Header*>(buffer.data());
  header->magicTag = {0x00, 0x00, 0x00, 0x00}; // Invalid magic tag
  header->width = 64;
  header->height = 64;

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      buffer.data(), static_cast<uint32_t>(buffer.size()), nullptr);
  EXPECT_FALSE(factory_.canCreate(reader, &ret));
  EXPECT_FALSE(ret.isOk());
}

TEST_F(Xtc1TextureLoaderTest, TryCreateFailsWithSmallBuffer) {
  std::vector<uint8_t> buffer(4); // Too small

  Result ret;
  auto reader = *iglu::textureloader::DataReader::tryCreate(
      buffer.data(), static_cast<uint32_t>(buffer.size()), nullptr);
  auto loader = factory_.tryCreate(reader, TextureFormat::RGBA_UNorm8, &ret);
  EXPECT_EQ(loader, nullptr);
  EXPECT_FALSE(ret.isOk());
}

} // namespace igl::tests::xtc1

// @fb-only
#endif // IGL_CMAKE_BUILD
