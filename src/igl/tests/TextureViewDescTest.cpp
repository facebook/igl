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
// Swizzle
// ---------------------------------------------------------------------------

TEST(SwizzleTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_Default), 0u);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_0), 1u);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_1), 2u);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_R), 3u);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_G), 4u);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_B), 5u);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_A), 6u);
}

TEST(SwizzleTest, AllValuesDistinct) {
  EXPECT_NE(Swizzle_Default, Swizzle_0);
  EXPECT_NE(Swizzle_0, Swizzle_1);
  EXPECT_NE(Swizzle_1, Swizzle_R);
  EXPECT_NE(Swizzle_R, Swizzle_G);
  EXPECT_NE(Swizzle_G, Swizzle_B);
  EXPECT_NE(Swizzle_B, Swizzle_A);
  EXPECT_NE(Swizzle_Default, Swizzle_A);
}

// ---------------------------------------------------------------------------
// ImageAspectBits
// ---------------------------------------------------------------------------

TEST(ImageAspectBitsTest, BitValues) {
  EXPECT_EQ(ImageAspectBits_Invalid, 0xffffffffu);
  EXPECT_EQ(ImageAspectBits_None, 0u);
  EXPECT_EQ(ImageAspectBits_Color, 0x1u);
  EXPECT_EQ(ImageAspectBits_Depth, 0x2u);
  EXPECT_EQ(ImageAspectBits_Stencil, 0x4u);
  EXPECT_EQ(ImageAspectBits_Plane_0, 0x10u);
  EXPECT_EQ(ImageAspectBits_Plane_1, 0x20u);
  EXPECT_EQ(ImageAspectBits_Plane_2, 0x40u);
}

TEST(ImageAspectBitsTest, BitsCanBeCombined) {
  const ImageAspectFlags combined = ImageAspectBits_Color | ImageAspectBits_Depth;
  EXPECT_TRUE(combined & ImageAspectBits_Color);
  EXPECT_TRUE(combined & ImageAspectBits_Depth);
  EXPECT_FALSE(combined & ImageAspectBits_Stencil);
}

TEST(ImageAspectBitsTest, PlaneBitsAreSeparateFromCoreAspects) {
  const ImageAspectFlags planes =
      ImageAspectBits_Plane_0 | ImageAspectBits_Plane_1 | ImageAspectBits_Plane_2;
  EXPECT_FALSE(planes & ImageAspectBits_Color);
  EXPECT_FALSE(planes & ImageAspectBits_Depth);
  EXPECT_FALSE(planes & ImageAspectBits_Stencil);
}

// ---------------------------------------------------------------------------
// ComponentMapping
// ---------------------------------------------------------------------------

TEST(ComponentMappingTest, DefaultConstruction) {
  const ComponentMapping mapping;
  EXPECT_EQ(mapping.r, Swizzle_Default);
  EXPECT_EQ(mapping.g, Swizzle_Default);
  EXPECT_EQ(mapping.b, Swizzle_Default);
  EXPECT_EQ(mapping.a, Swizzle_Default);
}

TEST(ComponentMappingTest, IdentityTrueForDefaults) {
  const ComponentMapping mapping;
  EXPECT_TRUE(mapping.identity());
}

TEST(ComponentMappingTest, IdentityFalseWhenRedDiffers) {
  ComponentMapping mapping;
  mapping.r = Swizzle_0;
  EXPECT_FALSE(mapping.identity());
}

TEST(ComponentMappingTest, IdentityFalseWhenGreenDiffers) {
  ComponentMapping mapping;
  mapping.g = Swizzle_1;
  EXPECT_FALSE(mapping.identity());
}

TEST(ComponentMappingTest, IdentityFalseWhenBlueDiffers) {
  ComponentMapping mapping;
  mapping.b = Swizzle_A;
  EXPECT_FALSE(mapping.identity());
}

TEST(ComponentMappingTest, IdentityFalseWhenAlphaDiffers) {
  ComponentMapping mapping;
  mapping.a = Swizzle_R;
  EXPECT_FALSE(mapping.identity());
}

TEST(ComponentMappingTest, DesignatedInitializer) {
  const ComponentMapping mapping{
      .r = Swizzle_B,
      .g = Swizzle_G,
      .b = Swizzle_R,
      .a = Swizzle_A,
  };
  EXPECT_EQ(mapping.r, Swizzle_B);
  EXPECT_EQ(mapping.g, Swizzle_G);
  EXPECT_EQ(mapping.b, Swizzle_R);
  EXPECT_EQ(mapping.a, Swizzle_A);
  EXPECT_FALSE(mapping.identity());
}

// ---------------------------------------------------------------------------
// TextureViewDesc
// ---------------------------------------------------------------------------

TEST(TextureViewDescTest, DefaultConstructionValues) {
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

TEST(TextureViewDescTest, DesignatedInitializer) {
  const TextureViewDesc desc{
      .type = TextureType::Cube,
      .format = TextureFormat::RGBA_UNorm8,
      .aspect = ImageAspectBits_Color,
      .layer = 2,
      .numLayers = 6,
      .mipLevel = 1,
      .numMipLevels = 4,
      .debugName = "cubeView",
  };
  EXPECT_EQ(desc.type, TextureType::Cube);
  EXPECT_EQ(desc.format, TextureFormat::RGBA_UNorm8);
  EXPECT_EQ(desc.aspect, ImageAspectBits_Color);
  EXPECT_EQ(desc.layer, 2u);
  EXPECT_EQ(desc.numLayers, 6u);
  EXPECT_EQ(desc.mipLevel, 1u);
  EXPECT_EQ(desc.numMipLevels, 4u);
  EXPECT_TRUE(desc.swizzle.identity());
  EXPECT_EQ(desc.debugName, "cubeView");
}

TEST(TextureViewDescTest, CustomSwizzle) {
  const TextureViewDesc desc{
      .swizzle = {.r = Swizzle_A, .g = Swizzle_A, .b = Swizzle_A, .a = Swizzle_R},
  };
  EXPECT_FALSE(desc.swizzle.identity());
  EXPECT_EQ(desc.swizzle.r, Swizzle_A);
  EXPECT_EQ(desc.swizzle.a, Swizzle_R);
}

TEST(TextureViewDescTest, DefaultAspectIsInvalid) {
  const TextureViewDesc desc;
  EXPECT_EQ(desc.aspect, ImageAspectBits_Invalid);
}

} // namespace igl::tests
