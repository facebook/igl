/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <IGLU/texture_loader/stb_hdr/Header.h>
#include <IGLU/texture_loader/stb_hdr/TextureLoaderFactory.h>
#include <cstring>

namespace igl::tests::stb::hdr {

namespace {

std::string populateMinimalValidFile(bool radiance, uint32_t width, uint32_t height) {
  std::stringstream ss;
  ss << "#?" << (radiance ? "RADIANCE" : "RGBE") << "\n";
  ss << "FORMAT=32-bit_rle_rgbe\n";
  ss << "\n";
  ss << "-Y " << width << " +X " << height << "\n";

  return ss.str();
}

} // namespace

class StbHdrTextureLoaderTest : public ::testing::Test {
 public:
  void SetUp() override {}

  void TearDown() override {}

  iglu::textureloader::stb::hdr::TextureLoaderFactory factory_;
};

TEST_F(StbHdrTextureLoaderTest, EmptyBuffer_Fails) {
  std::string buffer;
  buffer.resize(iglu::textureloader::stb::hdr::kHeaderLength);

  Result ret;
  auto reader =
      *iglu::textureloader::DataReader::tryCreate(reinterpret_cast<const uint8_t*>(buffer.data()),
                                                  static_cast<uint32_t>(buffer.size()),
                                                  nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_EQ(loader, nullptr);
  EXPECT_FALSE(ret.isOk());
}

TEST_F(StbHdrTextureLoaderTest, MinimumValidRadianceHeader_Succeeds) {
  const uint32_t width = 64u;
  const uint32_t height = 32u;
  const bool radiance = true;

  auto buffer = populateMinimalValidFile(radiance, width, height);

  Result ret;
  auto reader =
      *iglu::textureloader::DataReader::tryCreate(reinterpret_cast<const uint8_t*>(buffer.data()),
                                                  static_cast<uint32_t>(buffer.size()),
                                                  nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_NE(loader, nullptr);
  EXPECT_TRUE(ret.isOk()) << ret.message;
}

TEST_F(StbHdrTextureLoaderTest, MinimumValidRgbeHeader_Succeeds) {
  const uint32_t width = 64u;
  const uint32_t height = 32u;
  const bool radiance = false;

  auto buffer = populateMinimalValidFile(radiance, width, height);

  Result ret;
  auto reader =
      *iglu::textureloader::DataReader::tryCreate(reinterpret_cast<const uint8_t*>(buffer.data()),
                                                  static_cast<uint32_t>(buffer.size()),
                                                  nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_NE(loader, nullptr);
  EXPECT_TRUE(ret.isOk()) << ret.message;
}

TEST_F(StbHdrTextureLoaderTest, ValidHeaderWithExtraData_Succeeds) {
  const uint32_t width = 64u;
  const uint32_t height = 32u;
  const bool radiance = true;

  auto buffer = populateMinimalValidFile(radiance, width, height);
  buffer.resize(buffer.size() + 1);

  Result ret;
  auto reader =
      *iglu::textureloader::DataReader::tryCreate(reinterpret_cast<const uint8_t*>(buffer.data()),
                                                  static_cast<uint32_t>(buffer.size()),
                                                  nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_NE(loader, nullptr);
  EXPECT_TRUE(ret.isOk()) << ret.message;
}

TEST_F(StbHdrTextureLoaderTest, InsufficientData_Fails) {
  std::string buffer = "?RADIANCE\n";

  Result ret;
  auto reader =
      *iglu::textureloader::DataReader::tryCreate(reinterpret_cast<const uint8_t*>(buffer.data()),
                                                  static_cast<uint32_t>(buffer.size()),
                                                  nullptr);
  auto loader = factory_.tryCreate(reader, &ret);
  EXPECT_EQ(loader, nullptr);
  EXPECT_FALSE(ret.isOk());
}

} // namespace igl::tests::stb::hdr
