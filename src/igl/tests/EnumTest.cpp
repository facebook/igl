/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/Common.h>
#include <igl/RenderPipelineState.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// CullMode
// ---------------------------------------------------------------------------

TEST(CullModeTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(CullMode::Disabled), 0u);
  EXPECT_EQ(static_cast<uint8_t>(CullMode::Front), 1u);
  EXPECT_EQ(static_cast<uint8_t>(CullMode::Back), 2u);
}

// ---------------------------------------------------------------------------
// WindingMode
// ---------------------------------------------------------------------------

TEST(WindingModeTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(WindingMode::Clockwise), 0u);
  EXPECT_EQ(static_cast<uint8_t>(WindingMode::CounterClockwise), 1u);
}

// ---------------------------------------------------------------------------
// PrimitiveType
// ---------------------------------------------------------------------------

TEST(PrimitiveTypeTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(PrimitiveType::Point), 0u);
  EXPECT_EQ(static_cast<uint8_t>(PrimitiveType::Line), 1u);
  EXPECT_EQ(static_cast<uint8_t>(PrimitiveType::LineStrip), 2u);
  EXPECT_EQ(static_cast<uint8_t>(PrimitiveType::Triangle), 3u);
  EXPECT_EQ(static_cast<uint8_t>(PrimitiveType::TriangleStrip), 4u);
}

// ---------------------------------------------------------------------------
// ResourceStorage
// ---------------------------------------------------------------------------

TEST(ResourceStorageTest, EnumValues) {
  EXPECT_NE(static_cast<int>(ResourceStorage::Private), static_cast<int>(ResourceStorage::Shared));
  EXPECT_NE(static_cast<int>(ResourceStorage::Shared), static_cast<int>(ResourceStorage::Managed));
  EXPECT_NE(static_cast<int>(ResourceStorage::Private), static_cast<int>(ResourceStorage::Managed));
}

// ---------------------------------------------------------------------------
// BlendOp
// ---------------------------------------------------------------------------

TEST(BlendOpTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(BlendOp::Add), 0u);
  EXPECT_EQ(static_cast<uint8_t>(BlendOp::Subtract), 1u);
  EXPECT_EQ(static_cast<uint8_t>(BlendOp::ReverseSubtract), 2u);
  EXPECT_EQ(static_cast<uint8_t>(BlendOp::Min), 3u);
  EXPECT_EQ(static_cast<uint8_t>(BlendOp::Max), 4u);
}

// ---------------------------------------------------------------------------
// BlendFactor
// ---------------------------------------------------------------------------

TEST(BlendFactorTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::Zero), 0u);
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::One), 1u);
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::SrcColor), 2u);
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::OneMinusSrcColor), 3u);
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::SrcAlpha), 4u);
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::OneMinusSrcAlpha), 5u);
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::DstColor), 6u);
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::OneMinusDstColor), 7u);
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::DstAlpha), 8u);
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::OneMinusDstAlpha), 9u);
}

// ---------------------------------------------------------------------------
// PolygonFillMode
// ---------------------------------------------------------------------------

TEST(PolygonFillModeTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(PolygonFillMode::Fill), 0u);
  EXPECT_EQ(static_cast<uint8_t>(PolygonFillMode::Line), 1u);
}

// ---------------------------------------------------------------------------
// ColorWriteBits
// ---------------------------------------------------------------------------

TEST(ColorWriteBitsTest, BitsAreDistinct) {
  EXPECT_EQ(kColorWriteBitsRed, 1u << 0);
  EXPECT_EQ(kColorWriteBitsGreen, 1u << 1);
  EXPECT_EQ(kColorWriteBitsBlue, 1u << 2);
  EXPECT_EQ(kColorWriteBitsAlpha, 1u << 3);
}

TEST(ColorWriteBitsTest, AllCombinesAllBits) {
  EXPECT_EQ(kColorWriteBitsAll,
            kColorWriteBitsRed | kColorWriteBitsGreen | kColorWriteBitsBlue | kColorWriteBitsAlpha);
}

} // namespace igl::tests
