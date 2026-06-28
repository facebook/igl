/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <functional>
#include <igl/DepthStencilState.h>

namespace igl::tests {

TEST(StencilStateDescTest, DefaultConstruction) {
  const StencilStateDesc desc;
  EXPECT_EQ(desc.stencilFailureOperation, StencilOperation::Keep);
  EXPECT_EQ(desc.depthFailureOperation, StencilOperation::Keep);
  EXPECT_EQ(desc.depthStencilPassOperation, StencilOperation::Keep);
  EXPECT_EQ(desc.stencilCompareFunction, CompareFunction::AlwaysPass);
  EXPECT_EQ(desc.readMask, static_cast<uint32_t>(~0));
  EXPECT_EQ(desc.writeMask, static_cast<uint32_t>(~0));
}

TEST(StencilStateDescTest, EqualityOpReflexive) {
  const StencilStateDesc desc;
  EXPECT_EQ(desc, desc);
}

TEST(StencilStateDescTest, EqualityOpSameValues) {
  const StencilStateDesc a;
  const StencilStateDesc b;
  EXPECT_EQ(a, b);
}

TEST(StencilStateDescTest, InequalityOpDifferentStencilFailureOperation) {
  StencilStateDesc a;
  StencilStateDesc b;
  b.stencilFailureOperation = StencilOperation::Replace;
  EXPECT_NE(a, b);
}

TEST(StencilStateDescTest, InequalityOpDifferentDepthFailureOperation) {
  StencilStateDesc a;
  StencilStateDesc b;
  b.depthFailureOperation = StencilOperation::Zero;
  EXPECT_NE(a, b);
}

TEST(StencilStateDescTest, InequalityOpDifferentDepthStencilPassOperation) {
  StencilStateDesc a;
  StencilStateDesc b;
  b.depthStencilPassOperation = StencilOperation::IncrementClamp;
  EXPECT_NE(a, b);
}

TEST(StencilStateDescTest, InequalityOpDifferentStencilCompareFunction) {
  StencilStateDesc a;
  StencilStateDesc b;
  b.stencilCompareFunction = CompareFunction::Less;
  EXPECT_NE(a, b);
}

TEST(StencilStateDescTest, InequalityOpDifferentReadMask) {
  StencilStateDesc a;
  StencilStateDesc b;
  b.readMask = 0x0Fu;
  EXPECT_NE(a, b);
}

TEST(StencilStateDescTest, InequalityOpDifferentWriteMask) {
  StencilStateDesc a;
  StencilStateDesc b;
  b.writeMask = 0xF0u;
  EXPECT_NE(a, b);
}

TEST(StencilStateDescTest, HashConsistency) {
  const StencilStateDesc desc;
  const std::hash<StencilStateDesc> hasher;
  EXPECT_EQ(hasher(desc), hasher(desc));
}

TEST(StencilStateDescTest, HashEqualObjectsHaveSameHash) {
  const StencilStateDesc a;
  const StencilStateDesc b;
  const std::hash<StencilStateDesc> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

TEST(StencilStateDescTest, HashDifferentObjectsHaveDifferentHash) {
  StencilStateDesc a;
  StencilStateDesc b;
  b.stencilFailureOperation = StencilOperation::Replace;
  const std::hash<StencilStateDesc> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

TEST(DepthStencilStateDescTest, DefaultConstruction) {
  const DepthStencilStateDesc desc;
  EXPECT_EQ(desc.compareFunction, CompareFunction::AlwaysPass);
  EXPECT_FALSE(desc.isDepthWriteEnabled);
  EXPECT_TRUE(desc.debugName.empty());
  EXPECT_EQ(desc.frontFaceStencil, StencilStateDesc{});
  EXPECT_EQ(desc.backFaceStencil, StencilStateDesc{});
}

TEST(DepthStencilStateDescTest, EqualityOpReflexive) {
  const DepthStencilStateDesc desc;
  EXPECT_EQ(desc, desc);
}

TEST(DepthStencilStateDescTest, EqualityOpSameValues) {
  const DepthStencilStateDesc a;
  const DepthStencilStateDesc b;
  EXPECT_EQ(a, b);
}

TEST(DepthStencilStateDescTest, InequalityOpDifferentCompareFunction) {
  DepthStencilStateDesc a;
  DepthStencilStateDesc b;
  b.compareFunction = CompareFunction::Less;
  EXPECT_NE(a, b);
}

TEST(DepthStencilStateDescTest, InequalityOpDifferentIsDepthWriteEnabled) {
  DepthStencilStateDesc a;
  DepthStencilStateDesc b;
  b.isDepthWriteEnabled = true;
  EXPECT_NE(a, b);
}

TEST(DepthStencilStateDescTest, InequalityOpDifferentFrontFaceStencil) {
  DepthStencilStateDesc a;
  DepthStencilStateDesc b;
  b.frontFaceStencil.stencilFailureOperation = StencilOperation::Replace;
  EXPECT_NE(a, b);
}

TEST(DepthStencilStateDescTest, InequalityOpDifferentBackFaceStencil) {
  DepthStencilStateDesc a;
  DepthStencilStateDesc b;
  b.backFaceStencil.readMask = 0x0Fu;
  EXPECT_NE(a, b);
}

TEST(DepthStencilStateDescTest, DebugNameIgnoredInEquality) {
  DepthStencilStateDesc a;
  DepthStencilStateDesc b;
  a.debugName = "alpha";
  b.debugName = "beta";
  EXPECT_EQ(a, b);
}

TEST(DepthStencilStateDescTest, HashConsistency) {
  const DepthStencilStateDesc desc;
  const std::hash<DepthStencilStateDesc> hasher;
  EXPECT_EQ(hasher(desc), hasher(desc));
}

TEST(DepthStencilStateDescTest, HashEqualObjectsHaveSameHash) {
  const DepthStencilStateDesc a;
  const DepthStencilStateDesc b;
  const std::hash<DepthStencilStateDesc> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

TEST(DepthStencilStateDescTest, HashDifferentObjectsHaveDifferentHash) {
  DepthStencilStateDesc a;
  DepthStencilStateDesc b;
  b.compareFunction = CompareFunction::Less;
  const std::hash<DepthStencilStateDesc> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

TEST(DepthStencilStateDescTest, HashDiffersOnIsDepthWriteEnabled) {
  DepthStencilStateDesc a;
  DepthStencilStateDesc b;
  b.isDepthWriteEnabled = true;
  const std::hash<DepthStencilStateDesc> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

TEST(DepthStencilStateDescTest, HashDiffersOnFrontFaceStencil) {
  DepthStencilStateDesc a;
  DepthStencilStateDesc b;
  b.frontFaceStencil.stencilFailureOperation = StencilOperation::Replace;
  const std::hash<DepthStencilStateDesc> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

TEST(DepthStencilStateDescTest, HashDiffersOnBackFaceStencil) {
  DepthStencilStateDesc a;
  DepthStencilStateDesc b;
  b.backFaceStencil.readMask = 0x0Fu;
  const std::hash<DepthStencilStateDesc> hasher;
  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

TEST(CompareFunctionTest, EnumSpotChecks) {
  EXPECT_EQ(static_cast<int>(CompareFunction::Never), 0);
  EXPECT_EQ(static_cast<int>(CompareFunction::AlwaysPass), 7);
}

TEST(CompareFunctionTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(CompareFunction::Never), 0u);
  EXPECT_EQ(static_cast<uint8_t>(CompareFunction::Less), 1u);
  EXPECT_EQ(static_cast<uint8_t>(CompareFunction::Equal), 2u);
  EXPECT_EQ(static_cast<uint8_t>(CompareFunction::LessEqual), 3u);
  EXPECT_EQ(static_cast<uint8_t>(CompareFunction::Greater), 4u);
  EXPECT_EQ(static_cast<uint8_t>(CompareFunction::NotEqual), 5u);
  EXPECT_EQ(static_cast<uint8_t>(CompareFunction::GreaterEqual), 6u);
  EXPECT_EQ(static_cast<uint8_t>(CompareFunction::AlwaysPass), 7u);
}

TEST(CompareFunctionTest, AllValuesDistinct) {
  EXPECT_NE(CompareFunction::Never, CompareFunction::Less);
  EXPECT_NE(CompareFunction::Less, CompareFunction::Equal);
  EXPECT_NE(CompareFunction::Equal, CompareFunction::LessEqual);
  EXPECT_NE(CompareFunction::LessEqual, CompareFunction::Greater);
  EXPECT_NE(CompareFunction::Greater, CompareFunction::NotEqual);
  EXPECT_NE(CompareFunction::NotEqual, CompareFunction::GreaterEqual);
  EXPECT_NE(CompareFunction::GreaterEqual, CompareFunction::AlwaysPass);
  EXPECT_NE(CompareFunction::Never, CompareFunction::AlwaysPass);
}

TEST(StencilOperationTest, EnumSpotChecks) {
  EXPECT_EQ(static_cast<int>(StencilOperation::Keep), 0);
  EXPECT_EQ(static_cast<int>(StencilOperation::DecrementWrap), 7);
}

TEST(StencilOperationTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(StencilOperation::Keep), 0u);
  EXPECT_EQ(static_cast<uint8_t>(StencilOperation::Zero), 1u);
  EXPECT_EQ(static_cast<uint8_t>(StencilOperation::Replace), 2u);
  EXPECT_EQ(static_cast<uint8_t>(StencilOperation::IncrementClamp), 3u);
  EXPECT_EQ(static_cast<uint8_t>(StencilOperation::DecrementClamp), 4u);
  EXPECT_EQ(static_cast<uint8_t>(StencilOperation::Invert), 5u);
  EXPECT_EQ(static_cast<uint8_t>(StencilOperation::IncrementWrap), 6u);
  EXPECT_EQ(static_cast<uint8_t>(StencilOperation::DecrementWrap), 7u);
}

TEST(StencilOperationTest, AllValuesDistinct) {
  EXPECT_NE(StencilOperation::Keep, StencilOperation::Zero);
  EXPECT_NE(StencilOperation::Zero, StencilOperation::Replace);
  EXPECT_NE(StencilOperation::Replace, StencilOperation::IncrementClamp);
  EXPECT_NE(StencilOperation::IncrementClamp, StencilOperation::DecrementClamp);
  EXPECT_NE(StencilOperation::DecrementClamp, StencilOperation::Invert);
  EXPECT_NE(StencilOperation::Invert, StencilOperation::IncrementWrap);
  EXPECT_NE(StencilOperation::IncrementWrap, StencilOperation::DecrementWrap);
  EXPECT_NE(StencilOperation::Keep, StencilOperation::DecrementWrap);
}

} // namespace igl::tests
