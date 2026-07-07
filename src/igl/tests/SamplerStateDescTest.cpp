/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <functional>
#include <igl/SamplerState.h>

namespace igl::tests {

TEST(SamplerStateDescTest, DefaultConstruction) {
  const SamplerStateDesc desc;
  EXPECT_EQ(desc.minFilter, SamplerMinMagFilter::Nearest);
  EXPECT_EQ(desc.magFilter, SamplerMinMagFilter::Nearest);
  EXPECT_EQ(desc.mipFilter, SamplerMipFilter::Disabled);
  EXPECT_EQ(desc.addressModeU, SamplerAddressMode::Repeat);
  EXPECT_EQ(desc.addressModeV, SamplerAddressMode::Repeat);
  EXPECT_EQ(desc.addressModeW, SamplerAddressMode::Repeat);
  EXPECT_EQ(desc.depthCompareFunction, CompareFunction::LessEqual);
  EXPECT_EQ(desc.mipLodMin, 0);
  EXPECT_EQ(desc.mipLodMax, 15);
  EXPECT_EQ(desc.maxAnisotropic, 1);
  EXPECT_FALSE(desc.depthCompareEnabled);
  EXPECT_TRUE(desc.debugName.empty());
  EXPECT_EQ(desc.yuvFormat, TextureFormat::Invalid);
}

TEST(SamplerStateDescTest, EqualityOpReflexive) {
  const SamplerStateDesc desc;
  EXPECT_EQ(desc, desc);
}

TEST(SamplerStateDescTest, EqualityOpSameValues) {
  const SamplerStateDesc a;
  const SamplerStateDesc b;
  EXPECT_EQ(a, b);
}

TEST(SamplerStateDescTest, InequalityOpDifferentMinFilter) {
  SamplerStateDesc a;
  SamplerStateDesc b;
  b.minFilter = SamplerMinMagFilter::Linear;
  EXPECT_NE(a, b);
}

TEST(SamplerStateDescTest, InequalityOpDifferentMipLodMax) {
  SamplerStateDesc a;
  SamplerStateDesc b;
  b.mipLodMax = 7;
  EXPECT_NE(a, b);
}

TEST(SamplerStateDescTest, InequalityOpDifferentAddressMode) {
  SamplerStateDesc a;
  SamplerStateDesc b;
  b.addressModeU = SamplerAddressMode::Clamp;
  EXPECT_NE(a, b);
}

TEST(SamplerStateDescTest, InequalityOpDifferentDepthCompareEnabled) {
  SamplerStateDesc a;
  SamplerStateDesc b;
  b.depthCompareEnabled = true;
  EXPECT_NE(a, b);
}

TEST(SamplerStateDescTest, InequalityOpDifferentMagFilter) {
  SamplerStateDesc a;
  SamplerStateDesc b;
  b.magFilter = SamplerMinMagFilter::Linear;
  EXPECT_NE(a, b);
}

TEST(SamplerStateDescTest, InequalityOpDifferentMaxAnisotropic) {
  SamplerStateDesc a;
  SamplerStateDesc b;
  b.maxAnisotropic = 8;
  EXPECT_NE(a, b);
}

TEST(SamplerStateDescTest, InequalityOpDifferentYuvFormat) {
  SamplerStateDesc a;
  SamplerStateDesc b;
  b.yuvFormat = TextureFormat::YUV_NV12;
  EXPECT_NE(a, b);
}

TEST(SamplerStateDescTest, InequalityOpDifferentMipFilter) {
  SamplerStateDesc a;
  SamplerStateDesc b;
  b.mipFilter = SamplerMipFilter::Linear;
  EXPECT_NE(a, b);
}

TEST(SamplerStateDescTest, InequalityOpDifferentAddressModeV) {
  SamplerStateDesc a;
  SamplerStateDesc b;
  b.addressModeV = SamplerAddressMode::Clamp;
  EXPECT_NE(a, b);
}

TEST(SamplerStateDescTest, InequalityOpDifferentAddressModeW) {
  SamplerStateDesc a;
  SamplerStateDesc b;
  b.addressModeW = SamplerAddressMode::MirrorRepeat;
  EXPECT_NE(a, b);
}

TEST(SamplerStateDescTest, InequalityOpDifferentDepthCompareFunction) {
  SamplerStateDesc a;
  SamplerStateDesc b;
  b.depthCompareFunction = CompareFunction::Never;
  EXPECT_NE(a, b);
}

TEST(SamplerStateDescTest, InequalityOpDifferentMipLodMin) {
  SamplerStateDesc a;
  SamplerStateDesc b;
  b.mipLodMin = 5;
  EXPECT_NE(a, b);
}

TEST(SamplerStateDescTest, DebugNameIgnoredInEquality) {
  SamplerStateDesc a;
  SamplerStateDesc b;
  a.debugName = "alpha";
  b.debugName = "beta";
  EXPECT_EQ(a, b);
}

TEST(SamplerStateDescTest, HashConsistency) {
  const SamplerStateDesc desc;
  const std::hash<SamplerStateDesc> hasher;
  EXPECT_EQ(hasher(desc), hasher(desc));
}

TEST(SamplerStateDescTest, HashEqualObjectsHaveSameHash) {
  const SamplerStateDesc a;
  const SamplerStateDesc b;
  const std::hash<SamplerStateDesc> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

TEST(SamplerStateDescTest, HashDifferentObjectsHaveDifferentHash) {
  const SamplerStateDesc def;
  const SamplerStateDesc linear = SamplerStateDesc::newLinear();
  const std::hash<SamplerStateDesc> hasher;
  EXPECT_NE(def, linear);
  EXPECT_NE(hasher(def), hasher(linear));
}

TEST(SamplerStateDescTest, FactoryNewLinear) {
  const SamplerStateDesc desc = SamplerStateDesc::newLinear();
  EXPECT_EQ(desc.minFilter, SamplerMinMagFilter::Linear);
  EXPECT_EQ(desc.magFilter, SamplerMinMagFilter::Linear);
  EXPECT_EQ(desc.mipFilter, SamplerMipFilter::Disabled);
}

TEST(SamplerStateDescTest, FactoryNewLinearMipmapped) {
  const SamplerStateDesc desc = SamplerStateDesc::newLinearMipmapped();
  EXPECT_EQ(desc.minFilter, SamplerMinMagFilter::Linear);
  EXPECT_EQ(desc.magFilter, SamplerMinMagFilter::Linear);
  EXPECT_EQ(desc.mipFilter, SamplerMipFilter::Linear);
}

TEST(SamplerStateDescTest, FactoryNewYUV) {
  const SamplerStateDesc desc = SamplerStateDesc::newYUV(TextureFormat::YUV_NV12, "testYUV");
  EXPECT_EQ(desc.minFilter, SamplerMinMagFilter::Linear);
  EXPECT_EQ(desc.magFilter, SamplerMinMagFilter::Linear);
  EXPECT_EQ(desc.mipFilter, SamplerMipFilter::Disabled);
  EXPECT_EQ(desc.addressModeU, SamplerAddressMode::Clamp);
  EXPECT_EQ(desc.addressModeV, SamplerAddressMode::Clamp);
  EXPECT_EQ(desc.addressModeW, SamplerAddressMode::Clamp);
  EXPECT_EQ(desc.yuvFormat, TextureFormat::YUV_NV12);
}

// ---------------------------------------------------------------------------
// SamplerMinMagFilter
// ---------------------------------------------------------------------------

TEST(SamplerMinMagFilterTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(SamplerMinMagFilter::Nearest), 0u);
  EXPECT_EQ(static_cast<uint8_t>(SamplerMinMagFilter::Linear), 1u);
}

TEST(SamplerMinMagFilterTest, ValuesAreDistinct) {
  EXPECT_NE(SamplerMinMagFilter::Nearest, SamplerMinMagFilter::Linear);
}

// ---------------------------------------------------------------------------
// SamplerMipFilter
// ---------------------------------------------------------------------------

TEST(SamplerMipFilterTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(SamplerMipFilter::Disabled), 0u);
  EXPECT_EQ(static_cast<uint8_t>(SamplerMipFilter::Nearest), 1u);
  EXPECT_EQ(static_cast<uint8_t>(SamplerMipFilter::Linear), 2u);
}

TEST(SamplerMipFilterTest, AllValuesDistinct) {
  EXPECT_NE(SamplerMipFilter::Disabled, SamplerMipFilter::Nearest);
  EXPECT_NE(SamplerMipFilter::Nearest, SamplerMipFilter::Linear);
  EXPECT_NE(SamplerMipFilter::Disabled, SamplerMipFilter::Linear);
}

// ---------------------------------------------------------------------------
// SamplerAddressMode
// ---------------------------------------------------------------------------

TEST(SamplerAddressModeTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(SamplerAddressMode::Repeat), 0u);
  EXPECT_EQ(static_cast<uint8_t>(SamplerAddressMode::Clamp), 1u);
  EXPECT_EQ(static_cast<uint8_t>(SamplerAddressMode::MirrorRepeat), 2u);
  EXPECT_EQ(static_cast<uint8_t>(SamplerAddressMode::ClampToBorder), 3u);
}

TEST(SamplerAddressModeTest, AllValuesDistinct) {
  EXPECT_NE(SamplerAddressMode::Repeat, SamplerAddressMode::Clamp);
  EXPECT_NE(SamplerAddressMode::Clamp, SamplerAddressMode::MirrorRepeat);
  EXPECT_NE(SamplerAddressMode::MirrorRepeat, SamplerAddressMode::ClampToBorder);
  EXPECT_NE(SamplerAddressMode::Repeat, SamplerAddressMode::ClampToBorder);
}

} // namespace igl::tests
