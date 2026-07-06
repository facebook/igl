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
// Swizzle enum
// ---------------------------------------------------------------------------

TEST(SwizzleEnumTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_Default), 0u);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_0), 1u);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_1), 2u);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_R), 3u);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_G), 4u);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_B), 5u);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_A), 6u);
}

TEST(SwizzleEnumTest, AllValuesDistinct) {
  const Swizzle values[] = {
      Swizzle_Default,
      Swizzle_0,
      Swizzle_1,
      Swizzle_R,
      Swizzle_G,
      Swizzle_B,
      Swizzle_A,
  };
  for (size_t i = 0; i < std::size(values); ++i) {
    for (size_t j = i + 1; j < std::size(values); ++j) {
      EXPECT_NE(values[i], values[j]);
    }
  }
}

// ---------------------------------------------------------------------------
// ComponentMapping
// ---------------------------------------------------------------------------

TEST(ComponentMappingTest, DefaultIsIdentity) {
  const ComponentMapping mapping;
  EXPECT_EQ(mapping.r, Swizzle_Default);
  EXPECT_EQ(mapping.g, Swizzle_Default);
  EXPECT_EQ(mapping.b, Swizzle_Default);
  EXPECT_EQ(mapping.a, Swizzle_Default);
  EXPECT_TRUE(mapping.identity());
}

TEST(ComponentMappingTest, IdentityWithExplicitDefaults) {
  const ComponentMapping mapping{
      .r = Swizzle_Default, .g = Swizzle_Default, .b = Swizzle_Default, .a = Swizzle_Default};
  EXPECT_TRUE(mapping.identity());
}

TEST(ComponentMappingTest, NonIdentityR) {
  const ComponentMapping mapping{.r = Swizzle_0};
  EXPECT_FALSE(mapping.identity());
}

TEST(ComponentMappingTest, NonIdentityG) {
  const ComponentMapping mapping{.g = Swizzle_1};
  EXPECT_FALSE(mapping.identity());
}

TEST(ComponentMappingTest, NonIdentityB) {
  const ComponentMapping mapping{.b = Swizzle_A};
  EXPECT_FALSE(mapping.identity());
}

TEST(ComponentMappingTest, NonIdentityA) {
  const ComponentMapping mapping{.a = Swizzle_R};
  EXPECT_FALSE(mapping.identity());
}

TEST(ComponentMappingTest, AllChannelsSwizzled) {
  const ComponentMapping mapping{.r = Swizzle_A, .g = Swizzle_B, .b = Swizzle_G, .a = Swizzle_R};
  EXPECT_FALSE(mapping.identity());
}

// ---------------------------------------------------------------------------
// ImageAspectBits
// ---------------------------------------------------------------------------

TEST(ImageAspectBitsTest, FlagValues) {
  EXPECT_EQ(ImageAspectBits_None, 0u);
  EXPECT_EQ(ImageAspectBits_Color, 0x1u);
  EXPECT_EQ(ImageAspectBits_Depth, 0x2u);
  EXPECT_EQ(ImageAspectBits_Stencil, 0x4u);
  EXPECT_EQ(ImageAspectBits_Plane_0, 0x10u);
  EXPECT_EQ(ImageAspectBits_Plane_1, 0x20u);
  EXPECT_EQ(ImageAspectBits_Plane_2, 0x40u);
  EXPECT_EQ(ImageAspectBits_Invalid, 0xffffffffu);
}

TEST(ImageAspectBitsTest, FlagCombinations) {
  const ImageAspectFlags depthStencil = ImageAspectBits_Depth | ImageAspectBits_Stencil;
  EXPECT_EQ(depthStencil, 0x6u);
  EXPECT_NE(depthStencil & ImageAspectBits_Depth, 0u);
  EXPECT_NE(depthStencil & ImageAspectBits_Stencil, 0u);
  EXPECT_EQ(depthStencil & ImageAspectBits_Color, 0u);
}

TEST(ImageAspectBitsTest, PlaneFlagsCombine) {
  const ImageAspectFlags allPlanes =
      ImageAspectBits_Plane_0 | ImageAspectBits_Plane_1 | ImageAspectBits_Plane_2;
  EXPECT_EQ(allPlanes, 0x70u);
}

// ---------------------------------------------------------------------------
// TextureViewDesc
// ---------------------------------------------------------------------------

TEST(TextureViewDescTest, DefaultValues) {
  const TextureViewDesc desc;
  EXPECT_EQ(desc.type, TextureType::TwoD);
  EXPECT_EQ(desc.format, TextureFormat::Invalid);
  EXPECT_EQ(desc.aspect, ImageAspectBits_Invalid);
  EXPECT_EQ(desc.layer, 0u);
  EXPECT_EQ(desc.numLayers, 1u);
  EXPECT_EQ(desc.mipLevel, 0u);
  EXPECT_EQ(desc.numMipLevels, 1u);
  EXPECT_TRUE(desc.swizzle.identity());
  EXPECT_TRUE(desc.debugName.empty());
}

} // namespace igl::tests
