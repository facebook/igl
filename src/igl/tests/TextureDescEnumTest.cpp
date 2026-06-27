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
