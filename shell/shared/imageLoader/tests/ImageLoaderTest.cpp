/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <shell/shared/imageLoader/ImageLoader.h>

#include <cstring>

namespace igl::shell::tests {

class ImageLoaderTest : public ::testing::Test {
 public:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(ImageLoaderTest, CheckerboardReturnsValidData) {
  auto imageData = ImageLoader::checkerboard();

  EXPECT_NE(imageData.data, nullptr);
  EXPECT_EQ(imageData.data->size(), 8u * 8u * 4u);
  EXPECT_NE(imageData.data->data(), nullptr);
  EXPECT_EQ(imageData.desc.width, 8u);
  EXPECT_EQ(imageData.desc.height, 8u);
}

TEST_F(ImageLoaderTest, WhiteReturnsValidData) {
  auto imageData = ImageLoader::white();

  EXPECT_NE(imageData.data, nullptr);
  EXPECT_EQ(imageData.data->size(), 8u * 8u * 4u);
  EXPECT_NE(imageData.data->data(), nullptr);
  EXPECT_EQ(imageData.desc.width, 8u);
  EXPECT_EQ(imageData.desc.height, 8u);
}

TEST_F(ImageLoaderTest, CheckerboardDataIsReadable) {
  auto imageData = ImageLoader::checkerboard();

  ASSERT_NE(imageData.data, nullptr);
  const uint8_t* dataPtr = imageData.data->data();
  ASSERT_NE(dataPtr, nullptr);

  uint32_t firstPixel = 0;
  std::memcpy(&firstPixel, dataPtr, sizeof(uint32_t));
  EXPECT_EQ(firstPixel, 0xFF000000u);
}

TEST_F(ImageLoaderTest, WhiteDataIsAllWhite) {
  auto imageData = ImageLoader::white();

  ASSERT_NE(imageData.data, nullptr);
  const uint8_t* dataPtr = imageData.data->data();
  ASSERT_NE(dataPtr, nullptr);

  uint32_t firstPixel = 0;
  std::memcpy(&firstPixel, dataPtr, sizeof(uint32_t));
  EXPECT_EQ(firstPixel, 0xFFFFFFFFu);
}

TEST_F(ImageLoaderTest, CheckerboardDescriptorFields) {
  auto imageData = ImageLoader::checkerboard();

  EXPECT_EQ(imageData.desc.format, TextureFormat::RGBA_UNorm8);
  EXPECT_EQ(imageData.desc.numMipLevels, TextureDesc::calcNumMipLevels(8u, 8u));
  EXPECT_NE(imageData.desc.usage & TextureDesc::TextureUsageBits::Sampled, 0);
}

TEST_F(ImageLoaderTest, WhiteDescriptorFields) {
  auto imageData = ImageLoader::white();

  EXPECT_EQ(imageData.desc.format, TextureFormat::RGBA_UNorm8);
  EXPECT_EQ(imageData.desc.numMipLevels, TextureDesc::calcNumMipLevels(8u, 8u));
  EXPECT_NE(imageData.desc.usage & TextureDesc::TextureUsageBits::Sampled, 0);
}

TEST_F(ImageLoaderTest, CheckerboardHasAlternatingPattern) {
  auto imageData = ImageLoader::checkerboard();

  ASSERT_NE(imageData.data, nullptr);
  ASSERT_GE(imageData.data->size(), 3u * sizeof(uint32_t));
  const uint8_t* dataPtr = imageData.data->data();
  ASSERT_NE(dataPtr, nullptr);

  // The checkerboard alternates black/white in 2-pixel runs, so the third pixel
  // is white while the first is black.
  uint32_t firstPixel = 0;
  uint32_t thirdPixel = 0;
  std::memcpy(&firstPixel, dataPtr, sizeof(uint32_t));
  std::memcpy(&thirdPixel, dataPtr + 2 * sizeof(uint32_t), sizeof(uint32_t));
  EXPECT_EQ(firstPixel, 0xFF000000u);
  EXPECT_EQ(thirdPixel, 0xFFFFFFFFu);
}

} // namespace igl::shell::tests
