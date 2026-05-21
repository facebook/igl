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

} // namespace igl::tests
