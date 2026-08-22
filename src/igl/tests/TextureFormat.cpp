/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "util/TextureFormatTestBase.h"

#if defined(IGL_D3D12_TEST)
#include <igl/d3d12/Common.h>
#endif

namespace igl::tests {

class TextureFormatTest : public util::TextureFormatTestBase {
 private:
 public:
  TextureFormatTest() = default;
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
  EXPECT_EQ(linearTosRGB(igl::TextureFormat::RGBA_UNorm8), igl::TextureFormat::RGBA_SRGB);
  EXPECT_EQ(linearTosRGB(igl::TextureFormat::BGRA_UNorm8), igl::TextureFormat::BGRA_SRGB);

  EXPECT_EQ(sRGBToLinear(igl::TextureFormat::RGBA_SRGB), igl::TextureFormat::RGBA_UNorm8);
  EXPECT_EQ(sRGBToLinear(igl::TextureFormat::BGRA_SRGB), igl::TextureFormat::BGRA_UNorm8);
}

TEST(TextureFormatUtilsTest, RgbaToBgra) {
  EXPECT_EQ(RgbaToBgra(igl::TextureFormat::RGBA_UNorm8), igl::TextureFormat::BGRA_UNorm8);
  EXPECT_EQ(RgbaToBgra(igl::TextureFormat::RGBA_SRGB), igl::TextureFormat::BGRA_SRGB);

  EXPECT_EQ(BgraToRgba(igl::TextureFormat::BGRA_UNorm8), igl::TextureFormat::RGBA_UNorm8);
  EXPECT_EQ(BgraToRgba(igl::TextureFormat::BGRA_SRGB), igl::TextureFormat::RGBA_SRGB);
}

TEST(TextureFormatUtilsTest, BgraToRgbaPassthrough) {
  EXPECT_EQ(BgraToRgba(igl::TextureFormat::RGBA_UNorm8), igl::TextureFormat::RGBA_UNorm8);
  EXPECT_EQ(BgraToRgba(igl::TextureFormat::R_UNorm8), igl::TextureFormat::R_UNorm8);
  EXPECT_EQ(BgraToRgba(igl::TextureFormat::RGBA_SRGB), igl::TextureFormat::RGBA_SRGB);
}

TEST(TextureFormatUtilsTest, RgbaToBgraPassthrough) {
  EXPECT_EQ(RgbaToBgra(igl::TextureFormat::BGRA_UNorm8), igl::TextureFormat::BGRA_UNorm8);
  EXPECT_EQ(RgbaToBgra(igl::TextureFormat::R_UNorm8), igl::TextureFormat::R_UNorm8);
  EXPECT_EQ(RgbaToBgra(igl::TextureFormat::BGRA_SRGB), igl::TextureFormat::BGRA_SRGB);
}

#if defined(IGL_D3D12_TEST)
TEST(TextureFormatD3D12Test, B10G11R11UFloatRoundTrip) {
  constexpr auto kIglFormat = TextureFormat::B10G11R11_UFloat;
  constexpr auto kDxgiFormat = DXGI_FORMAT_R11G11B10_FLOAT;

  EXPECT_EQ(d3d12::textureFormatToDXGIFormat(kIglFormat), kDxgiFormat);
  EXPECT_EQ(d3d12::dxgiFormatToTextureFormat(kDxgiFormat), kIglFormat);
  EXPECT_EQ(d3d12::dxgiFormatToTextureFormat(d3d12::textureFormatToDXGIFormat(kIglFormat)),
            kIglFormat);
}
#endif

} // namespace igl::tests
