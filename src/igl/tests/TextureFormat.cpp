/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "util/TextureFormatTestBase.h"

namespace igl::tests {

class TextureFormatTest : public util::TextureFormatTestBase {
 private:
 public:
  TextureFormatTest() = default;
  ~TextureFormatTest() override = default;
};

TEST_F(TextureFormatTest, Sampled) {
  testUsage(TextureDesc::TextureUsageBits::Sampled, "Sampled");
}

TEST_F(TextureFormatTest, SampledAttachment) {
  testUsage(TextureDesc::TextureUsageBits::Sampled | TextureDesc::TextureUsageBits::Attachment,
            "SampledAttachment");
}

TEST_F(TextureFormatTest, Attachment) {
  testUsage(TextureDesc::TextureUsageBits::Attachment, "Attachment");
}

TEST_F(TextureFormatTest, Storage) {
  testUsage(TextureDesc::TextureUsageBits::Storage, "Storage");
}

TEST(TextureFormatUtilsTest, UnormTosRGB) {
  EXPECT_EQ(UNormTosRGB(igl::TextureFormat::RGBA_UNorm8), igl::TextureFormat::RGBA_SRGB);
  EXPECT_EQ(UNormTosRGB(igl::TextureFormat::BGRA_UNorm8), igl::TextureFormat::BGRA_SRGB);

  EXPECT_EQ(sRGBToUNorm(igl::TextureFormat::RGBA_SRGB), igl::TextureFormat::RGBA_UNorm8);
  EXPECT_EQ(sRGBToUNorm(igl::TextureFormat::BGRA_SRGB), igl::TextureFormat::BGRA_UNorm8);
}

TEST(TextureFormatUtilsTest, RgbaToBgra) {
  EXPECT_EQ(RgbaToBgra(igl::TextureFormat::RGBA_UNorm8), igl::TextureFormat::BGRA_UNorm8);
  EXPECT_EQ(RgbaToBgra(igl::TextureFormat::RGBA_SRGB), igl::TextureFormat::BGRA_SRGB);

  EXPECT_EQ(BgraToRgba(igl::TextureFormat::BGRA_UNorm8), igl::TextureFormat::RGBA_UNorm8);
  EXPECT_EQ(BgraToRgba(igl::TextureFormat::BGRA_SRGB), igl::TextureFormat::RGBA_SRGB);
}

} // namespace igl::tests
