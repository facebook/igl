/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <set>
#include <igl/Texture.h>

namespace igl::tests {

// --- Swizzle enum ---

TEST(TextureSwizzleAspectSwizzleTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_Default), 0);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_0), 1);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_1), 2);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_R), 3);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_G), 4);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_B), 5);
  EXPECT_EQ(static_cast<uint8_t>(Swizzle_A), 6);
}

TEST(TextureSwizzleAspectSwizzleTest, AllValuesDistinct) {
  const std::set<uint8_t> values = {
      static_cast<uint8_t>(Swizzle_Default),
      static_cast<uint8_t>(Swizzle_0),
      static_cast<uint8_t>(Swizzle_1),
      static_cast<uint8_t>(Swizzle_R),
      static_cast<uint8_t>(Swizzle_G),
      static_cast<uint8_t>(Swizzle_B),
      static_cast<uint8_t>(Swizzle_A),
  };
  EXPECT_EQ(values.size(), 7u);
}

// --- ComponentMapping ---

TEST(TextureSwizzleAspectComponentMappingTest, DefaultConstruction) {
  const ComponentMapping cm;
  EXPECT_EQ(cm.r, Swizzle_Default);
  EXPECT_EQ(cm.g, Swizzle_Default);
  EXPECT_EQ(cm.b, Swizzle_Default);
  EXPECT_EQ(cm.a, Swizzle_Default);
}

TEST(TextureSwizzleAspectComponentMappingTest, IdentityWithDefaults) {
  const ComponentMapping cm;
  EXPECT_TRUE(cm.identity());
}

TEST(TextureSwizzleAspectComponentMappingTest, NonIdentityR) {
  ComponentMapping cm;
  cm.r = Swizzle_R;
  EXPECT_FALSE(cm.identity());
}

TEST(TextureSwizzleAspectComponentMappingTest, NonIdentityG) {
  ComponentMapping cm;
  cm.g = Swizzle_G;
  EXPECT_FALSE(cm.identity());
}

TEST(TextureSwizzleAspectComponentMappingTest, NonIdentityB) {
  ComponentMapping cm;
  cm.b = Swizzle_B;
  EXPECT_FALSE(cm.identity());
}

TEST(TextureSwizzleAspectComponentMappingTest, NonIdentityA) {
  ComponentMapping cm;
  cm.a = Swizzle_A;
  EXPECT_FALSE(cm.identity());
}

TEST(TextureSwizzleAspectComponentMappingTest, AllChannelsCustom) {
  const ComponentMapping cm{.r = Swizzle_A, .g = Swizzle_B, .b = Swizzle_G, .a = Swizzle_R};
  EXPECT_FALSE(cm.identity());
  EXPECT_EQ(cm.r, Swizzle_A);
  EXPECT_EQ(cm.g, Swizzle_B);
  EXPECT_EQ(cm.b, Swizzle_G);
  EXPECT_EQ(cm.a, Swizzle_R);
}

TEST(TextureSwizzleAspectComponentMappingTest, DesignatedInitializerIdentity) {
  const ComponentMapping cm{
      .r = Swizzle_Default, .g = Swizzle_Default, .b = Swizzle_Default, .a = Swizzle_Default};
  EXPECT_TRUE(cm.identity());
}

TEST(TextureSwizzleAspectComponentMappingTest, ZeroAndOneSwizzle) {
  const ComponentMapping cm{.r = Swizzle_0, .g = Swizzle_1, .b = Swizzle_0, .a = Swizzle_1};
  EXPECT_FALSE(cm.identity());
  EXPECT_EQ(cm.r, Swizzle_0);
  EXPECT_EQ(cm.g, Swizzle_1);
}

// --- ImageAspectBits ---

TEST(TextureSwizzleAspectImageAspectBitsTest, InvalidIsMaxUint32) {
  EXPECT_EQ(ImageAspectBits_Invalid, 0xffffffff);
}

TEST(TextureSwizzleAspectImageAspectBitsTest, NoneIsZero) {
  EXPECT_EQ(ImageAspectBits_None, 0u);
}

TEST(TextureSwizzleAspectImageAspectBitsTest, ColorDepthStencilValues) {
  EXPECT_EQ(ImageAspectBits_Color, 0x1u);
  EXPECT_EQ(ImageAspectBits_Depth, 0x2u);
  EXPECT_EQ(ImageAspectBits_Stencil, 0x4u);
}

TEST(TextureSwizzleAspectImageAspectBitsTest, PlaneFlagValues) {
  EXPECT_EQ(ImageAspectBits_Plane_0, 0x10u);
  EXPECT_EQ(ImageAspectBits_Plane_1, 0x20u);
  EXPECT_EQ(ImageAspectBits_Plane_2, 0x40u);
}

TEST(TextureSwizzleAspectImageAspectBitsTest, BitwiseCombination) {
  const ImageAspectFlags flags = ImageAspectBits_Color | ImageAspectBits_Depth;
  EXPECT_EQ(flags, 0x3u);
  EXPECT_NE(flags & ImageAspectBits_Color, 0u);
  EXPECT_NE(flags & ImageAspectBits_Depth, 0u);
  EXPECT_EQ(flags & ImageAspectBits_Stencil, 0u);
}

TEST(TextureSwizzleAspectImageAspectBitsTest, DepthStencilCombination) {
  const ImageAspectFlags depthStencil = ImageAspectBits_Depth | ImageAspectBits_Stencil;
  EXPECT_EQ(depthStencil, 0x6u);
  EXPECT_EQ(depthStencil & ImageAspectBits_Color, 0u);
  EXPECT_NE(depthStencil & ImageAspectBits_Depth, 0u);
  EXPECT_NE(depthStencil & ImageAspectBits_Stencil, 0u);
}

TEST(TextureSwizzleAspectImageAspectBitsTest, AllPlaneCombination) {
  const ImageAspectFlags planes =
      ImageAspectBits_Plane_0 | ImageAspectBits_Plane_1 | ImageAspectBits_Plane_2;
  EXPECT_EQ(planes, 0x70u);
  EXPECT_EQ(planes & ImageAspectBits_Color, 0u);
}

TEST(TextureSwizzleAspectImageAspectBitsTest, IndividualBitsDistinct) {
  const std::set<uint32_t> values = {
      ImageAspectBits_None,
      ImageAspectBits_Color,
      ImageAspectBits_Depth,
      ImageAspectBits_Stencil,
      ImageAspectBits_Plane_0,
      ImageAspectBits_Plane_1,
      ImageAspectBits_Plane_2,
  };
  EXPECT_EQ(values.size(), 7u);
}

TEST(TextureSwizzleAspectImageAspectBitsTest, ImageAspectFlagsTypeAlias) {
  const ImageAspectFlags flags = ImageAspectBits_Color;
  const uint32_t raw = flags;
  EXPECT_EQ(raw, 0x1u);
}

} // namespace igl::tests
