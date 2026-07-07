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
// TextureDesc::new3D() factory
// ---------------------------------------------------------------------------

TEST(TextureDescFactoryTest, New3DSetsTypeAndDimensions) {
  const auto desc = TextureDesc::new3D(
      TextureFormat::RGBA_UNorm8, 16, 32, 8, TextureDesc::TextureUsageBits::Sampled, "vol");
  EXPECT_EQ(desc.type, TextureType::ThreeD);
  EXPECT_EQ(desc.format, TextureFormat::RGBA_UNorm8);
  EXPECT_EQ(desc.width, 16u);
  EXPECT_EQ(desc.height, 32u);
  EXPECT_EQ(desc.depth, 8u);
  EXPECT_EQ(desc.usage, TextureDesc::TextureUsageBits::Sampled);
  EXPECT_EQ(desc.debugName, "vol");
}

TEST(TextureDescFactoryTest, New3DWithNullDebugName) {
  const auto desc =
      TextureDesc::new3D(TextureFormat::R_UNorm8, 4, 4, 4, TextureDesc::TextureUsageBits::Storage);
  EXPECT_EQ(desc.type, TextureType::ThreeD);
  EXPECT_TRUE(desc.debugName.empty());
}

// ---------------------------------------------------------------------------
// TextureDesc::newExternalImage() factory
// ---------------------------------------------------------------------------

TEST(TextureDescFactoryTest, NewExternalImageSetsType) {
  const auto desc = TextureDesc::newExternalImage(
      TextureFormat::BGRA_UNorm8, 640, 480, TextureDesc::TextureUsageBits::Sampled, "cam");
  EXPECT_EQ(desc.type, TextureType::ExternalImage);
  EXPECT_EQ(desc.format, TextureFormat::BGRA_UNorm8);
  EXPECT_EQ(desc.width, 640u);
  EXPECT_EQ(desc.height, 480u);
  EXPECT_EQ(desc.usage, TextureDesc::TextureUsageBits::Sampled);
  EXPECT_EQ(desc.debugName, "cam");
}

TEST(TextureDescFactoryTest, NewExternalImageWithNullDebugName) {
  const auto desc = TextureDesc::newExternalImage(
      TextureFormat::RGBA_UNorm8, 100, 100, TextureDesc::TextureUsageBits::Attachment);
  EXPECT_EQ(desc.type, TextureType::ExternalImage);
  EXPECT_TRUE(desc.debugName.empty());
}

// ---------------------------------------------------------------------------
// TextureDesc::new2D() — default field values
// ---------------------------------------------------------------------------

TEST(TextureDescFactoryTest, New2DDefaultFieldValues) {
  const auto desc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 64, 64, TextureDesc::TextureUsageBits::Sampled, "def");
  EXPECT_EQ(desc.numMipLevels, 1u);
  EXPECT_EQ(desc.numLayers, 1u);
  EXPECT_EQ(desc.depth, 1u);
  EXPECT_EQ(desc.storage, ResourceStorage::Invalid);
}

// ---------------------------------------------------------------------------
// TextureDesc::newCube() — type and layer count
// ---------------------------------------------------------------------------

TEST(TextureDescFactoryTest, NewCubeSetsTypeAndDefaultLayers) {
  const auto desc = TextureDesc::newCube(
      TextureFormat::RGBA_UNorm8, 64, 64, TextureDesc::TextureUsageBits::Sampled, "cube");
  EXPECT_EQ(desc.type, TextureType::Cube);
  EXPECT_EQ(desc.numLayers, 1u);
  EXPECT_EQ(desc.width, 64u);
  EXPECT_EQ(desc.height, 64u);
  EXPECT_EQ(desc.debugName, "cube");
}

// ---------------------------------------------------------------------------
// TextureDesc::calcNumMipLevels() — standalone edge cases
// ---------------------------------------------------------------------------

TEST(TextureDescCalcMipLevelsTest, PowerOfTwo) {
  EXPECT_EQ(TextureDesc::calcNumMipLevels(1, 1), 1u);
  EXPECT_EQ(TextureDesc::calcNumMipLevels(2, 2), 2u);
  EXPECT_EQ(TextureDesc::calcNumMipLevels(4, 4), 3u);
  EXPECT_EQ(TextureDesc::calcNumMipLevels(256, 256), 9u);
  EXPECT_EQ(TextureDesc::calcNumMipLevels(1024, 1024), 11u);
}

TEST(TextureDescCalcMipLevelsTest, NonSquare) {
  EXPECT_EQ(TextureDesc::calcNumMipLevels(1, 256), 9u);
  EXPECT_EQ(TextureDesc::calcNumMipLevels(256, 1), 9u);
  EXPECT_EQ(TextureDesc::calcNumMipLevels(16, 64), 7u);
}

TEST(TextureDescCalcMipLevelsTest, NonPowerOfTwo) {
  EXPECT_EQ(TextureDesc::calcNumMipLevels(5, 5), 3u);
  EXPECT_EQ(TextureDesc::calcNumMipLevels(100, 100), 7u);
}

TEST(TextureDescCalcMipLevelsTest, ThreeDimensions) {
  EXPECT_EQ(TextureDesc::calcNumMipLevels(4, 4, 4), 3u);
  EXPECT_EQ(TextureDesc::calcNumMipLevels(4, 4, 8), 4u);
  EXPECT_EQ(TextureDesc::calcNumMipLevels(1, 1, 16), 5u);
}

TEST(TextureDescCalcMipLevelsTest, ZeroDimensionReturnsZero) {
  EXPECT_EQ(TextureDesc::calcNumMipLevels(0, 64), 0u);
  EXPECT_EQ(TextureDesc::calcNumMipLevels(64, 0), 0u);
  EXPECT_EQ(TextureDesc::calcNumMipLevels(0, 0), 0u);
  EXPECT_EQ(TextureDesc::calcNumMipLevels(0, 0, 0), 0u);
  EXPECT_EQ(TextureDesc::calcNumMipLevels(64, 64, 0), 0u);
}

// ---------------------------------------------------------------------------
// TextureDesc::asRange() for 3D textures
// ---------------------------------------------------------------------------

TEST(TextureDescAsRangeTest, ThreeDPropagatesDepth) {
  const auto desc = TextureDesc::new3D(
      TextureFormat::RGBA_UNorm8, 8, 16, 4, TextureDesc::TextureUsageBits::Sampled);
  const auto range = desc.asRange();
  EXPECT_EQ(range.width, 8u);
  EXPECT_EQ(range.height, 16u);
  EXPECT_EQ(range.depth, 4u);
  EXPECT_EQ(range.numFaces, 1u);
  EXPECT_EQ(range.numLayers, 1u);
}

// ---------------------------------------------------------------------------
// TextureDesc per-field equality — fields NOT covered by existing tests
// ---------------------------------------------------------------------------

TEST(TextureDescPerFieldEqualityTest, HeightDifference) {
  auto a = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 16, 16, TextureDesc::TextureUsageBits::Sampled, "tex");
  auto b = a;
  b.height = 32;
  EXPECT_NE(a, b);
}

TEST(TextureDescPerFieldEqualityTest, DepthDifference) {
  auto a = TextureDesc::new3D(
      TextureFormat::RGBA_UNorm8, 8, 8, 8, TextureDesc::TextureUsageBits::Sampled, "tex");
  auto b = a;
  b.depth = 4;
  EXPECT_NE(a, b);
}

TEST(TextureDescPerFieldEqualityTest, UsageDifference) {
  auto a = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 16, 16, TextureDesc::TextureUsageBits::Sampled, "tex");
  auto b = a;
  b.usage = TextureDesc::TextureUsageBits::Storage;
  EXPECT_NE(a, b);
}

TEST(TextureDescPerFieldEqualityTest, TypeDifference) {
  auto a = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 16, 16, TextureDesc::TextureUsageBits::Sampled, "tex");
  auto b = a;
  b.type = TextureType::Cube;
  EXPECT_NE(a, b);
}

TEST(TextureDescPerFieldEqualityTest, NumMipLevelsDifference) {
  auto a = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 16, 16, TextureDesc::TextureUsageBits::Sampled, "tex");
  auto b = a;
  b.numMipLevels = 4;
  EXPECT_NE(a, b);
}

TEST(TextureDescPerFieldEqualityTest, NumLayersDifference) {
  auto a = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 16, 16, TextureDesc::TextureUsageBits::Sampled, "tex");
  auto b = a;
  b.numLayers = 3;
  EXPECT_NE(a, b);
}

TEST(TextureDescPerFieldEqualityTest, StorageDifference) {
  auto a = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 16, 16, TextureDesc::TextureUsageBits::Sampled, "tex");
  auto b = a;
  b.storage = ResourceStorage::Private;
  EXPECT_NE(a, b);
}

TEST(TextureDescPerFieldEqualityTest, NumSamplesDifference) {
  auto a = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 16, 16, TextureDesc::TextureUsageBits::Sampled, "tex");
  auto b = a;
  b.numSamples = 4;
  EXPECT_NE(a, b);
}

} // namespace igl::tests
