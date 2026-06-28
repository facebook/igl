/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/Texture.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// TextureType
// ---------------------------------------------------------------------------

TEST(TextureTypeTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(TextureType::Invalid), 0u);
  EXPECT_EQ(static_cast<uint8_t>(TextureType::TwoD), 1u);
  EXPECT_EQ(static_cast<uint8_t>(TextureType::TwoDArray), 2u);
  EXPECT_EQ(static_cast<uint8_t>(TextureType::ThreeD), 3u);
  EXPECT_EQ(static_cast<uint8_t>(TextureType::Cube), 4u);
  EXPECT_EQ(static_cast<uint8_t>(TextureType::ExternalImage), 5u);
}

// ---------------------------------------------------------------------------
// TextureCubeFace
// ---------------------------------------------------------------------------

TEST(TextureCubeFaceTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(TextureCubeFace::PosX), 0u);
  EXPECT_EQ(static_cast<uint8_t>(TextureCubeFace::NegX), 1u);
  EXPECT_EQ(static_cast<uint8_t>(TextureCubeFace::PosY), 2u);
  EXPECT_EQ(static_cast<uint8_t>(TextureCubeFace::NegY), 3u);
  EXPECT_EQ(static_cast<uint8_t>(TextureCubeFace::PosZ), 4u);
  EXPECT_EQ(static_cast<uint8_t>(TextureCubeFace::NegZ), 5u);
}

// ---------------------------------------------------------------------------
// TextureDesc::TextureUsageBits
// ---------------------------------------------------------------------------

TEST(TextureUsageBitsTest, BitValues) {
  EXPECT_EQ(TextureDesc::TextureUsageBits::Sampled, 1u);
  EXPECT_EQ(TextureDesc::TextureUsageBits::Storage, 2u);
  EXPECT_EQ(TextureDesc::TextureUsageBits::Attachment, 4u);
}

TEST(TextureUsageBitsTest, BitsAreCombineable) {
  const TextureDesc::TextureUsage combined =
      TextureDesc::TextureUsageBits::Sampled | TextureDesc::TextureUsageBits::Attachment;
  EXPECT_EQ(combined, 5u);
  EXPECT_TRUE(combined & TextureDesc::TextureUsageBits::Sampled);
  EXPECT_TRUE(combined & TextureDesc::TextureUsageBits::Attachment);
  EXPECT_FALSE(combined & TextureDesc::TextureUsageBits::Storage);
}

// ---------------------------------------------------------------------------
// TextureDesc::TextureTiling
// ---------------------------------------------------------------------------

TEST(TextureTilingTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(TextureDesc::TextureTiling::Optimal), 0u);
  EXPECT_EQ(static_cast<uint8_t>(TextureDesc::TextureTiling::Linear), 1u);
}

// ---------------------------------------------------------------------------
// TextureDesc::TextureExportability
// ---------------------------------------------------------------------------

TEST(TextureExportabilityTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(TextureDesc::TextureExportability::NoExport), 0u);
  EXPECT_EQ(static_cast<uint8_t>(TextureDesc::TextureExportability::Exportable), 1u);
}

// ---------------------------------------------------------------------------
// TextureDesc::TextureMipmapGeneration
// ---------------------------------------------------------------------------

TEST(TextureMipmapGenerationTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(TextureDesc::TextureMipmapGeneration::Manual), 0u);
  EXPECT_EQ(static_cast<uint8_t>(TextureDesc::TextureMipmapGeneration::AutoGenerateOnUpload), 1u);
}

// ---------------------------------------------------------------------------
// SurfaceTextures
// ---------------------------------------------------------------------------

TEST(SurfaceTexturesTest, DefaultConstruction) {
  const SurfaceTextures st;
  EXPECT_EQ(st.color, nullptr);
  EXPECT_EQ(st.depth, nullptr);
}

// ---------------------------------------------------------------------------
// TextureDesc factory — newCube
// ---------------------------------------------------------------------------

TEST(TextureDescTest, NewCubeFactory) {
  const auto desc = TextureDesc::newCube(
      TextureFormat::RGBA_UNorm8, 64, 64, TextureDesc::TextureUsageBits::Sampled, "cubemap");
  EXPECT_EQ(desc.type, TextureType::Cube);
  EXPECT_EQ(desc.format, TextureFormat::RGBA_UNorm8);
  EXPECT_EQ(desc.width, 64u);
  EXPECT_EQ(desc.height, 64u);
  EXPECT_EQ(desc.debugName, "cubemap");
}

// ---------------------------------------------------------------------------
// TextureDesc factory — new2DArray
// ---------------------------------------------------------------------------

TEST(TextureDescTest, New2DArrayFactory) {
  const auto desc = TextureDesc::new2DArray(
      TextureFormat::RGBA_F16, 32, 32, 6, TextureDesc::TextureUsageBits::Sampled, "texArray");
  EXPECT_EQ(desc.type, TextureType::TwoDArray);
  EXPECT_EQ(desc.format, TextureFormat::RGBA_F16);
  EXPECT_EQ(desc.width, 32u);
  EXPECT_EQ(desc.height, 32u);
  EXPECT_EQ(desc.numLayers, 6u);
  EXPECT_EQ(desc.debugName, "texArray");
}

// ---------------------------------------------------------------------------
// TextureDesc — exportability and mipmap defaults
// ---------------------------------------------------------------------------

TEST(TextureDescTest, ExportabilityDefault) {
  const auto desc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 16, 16, TextureDesc::TextureUsageBits::Sampled);
  EXPECT_EQ(desc.exportability, TextureDesc::TextureExportability::NoExport);
  EXPECT_EQ(desc.mipmapGeneration, TextureDesc::TextureMipmapGeneration::Manual);
}

} // namespace igl::tests
