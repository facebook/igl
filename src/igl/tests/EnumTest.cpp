/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <array>
#include <igl/Common.h>
#include <igl/RenderPipelineState.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// CullMode
// ---------------------------------------------------------------------------

// CullMode feeds RenderPipelineDesc's hash via EnumToValue(); pin the ordinals
// since a reorder would silently change hashes for in-flight pipeline cache keys.
TEST(CullModeTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(CullMode::Disabled), 0u);
  EXPECT_EQ(static_cast<uint8_t>(CullMode::Front), 1u);
  EXPECT_EQ(static_cast<uint8_t>(CullMode::Back), 2u);
}

// ---------------------------------------------------------------------------
// WindingMode
// ---------------------------------------------------------------------------

// WindingMode feeds RenderPipelineDesc's hash via EnumToValue(); pin the ordinals
// since a reorder would silently change hashes for in-flight pipeline cache keys.
TEST(WindingModeTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(WindingMode::Clockwise), 0u);
  EXPECT_EQ(static_cast<uint8_t>(WindingMode::CounterClockwise), 1u);
}

// ---------------------------------------------------------------------------
// PrimitiveType
// ---------------------------------------------------------------------------

TEST(PrimitiveTypeTest, ValuesAreDistinct) {
  // PrimitiveType is never cast to a raw integer for hashing or ABI purposes;
  // every backend maps it through an explicit switch (e.g.
  // primitiveTypeToVkPrimitiveTopology(), toGlPrimitive()), so its ordinals are
  // not part of any contract. Verify distinctness rather than pinning integers.
  const std::array values = {
      PrimitiveType::Point,
      PrimitiveType::Line,
      PrimitiveType::LineStrip,
      PrimitiveType::Triangle,
      PrimitiveType::TriangleStrip,
  };
  for (size_t outer = 0; outer < values.size(); ++outer) {
    for (size_t inner = outer + 1; inner < values.size(); ++inner) {
      EXPECT_NE(values[outer], values[inner]);
    }
  }
}

// ---------------------------------------------------------------------------
// ResourceStorage
// ---------------------------------------------------------------------------

// ResourceStorage's ordinals aren't part of any hashing/serialization contract,
// so verify distinctness rather than pinning integers (see PrimitiveType above).
TEST(ResourceStorageTest, EnumValues) {
  EXPECT_NE(static_cast<int>(ResourceStorage::Private), static_cast<int>(ResourceStorage::Shared));
  EXPECT_NE(static_cast<int>(ResourceStorage::Shared), static_cast<int>(ResourceStorage::Managed));
  EXPECT_NE(static_cast<int>(ResourceStorage::Private), static_cast<int>(ResourceStorage::Managed));
}

// ---------------------------------------------------------------------------
// BlendOp
// ---------------------------------------------------------------------------

// BlendOp feeds RenderPipelineDesc's hash via EnumToValue(); pin the ordinals
// since a reorder would silently change hashes for in-flight pipeline cache keys.
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

// BlendFactor feeds RenderPipelineDesc's hash via EnumToValue(); pin the ordinals
// since a reorder would silently change hashes for in-flight pipeline cache keys.
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
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::SrcAlphaSaturated), 10u);
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::BlendColor), 11u);
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::OneMinusBlendColor), 12u);
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::BlendAlpha), 13u);
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::OneMinusBlendAlpha), 14u);
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::Src1Color), 15u);
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::OneMinusSrc1Color), 16u);
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::Src1Alpha), 17u);
  EXPECT_EQ(static_cast<uint8_t>(BlendFactor::OneMinusSrc1Alpha), 18u);
}

// ---------------------------------------------------------------------------
// PolygonFillMode
// ---------------------------------------------------------------------------

// PolygonFillMode feeds RenderPipelineDesc's hash via EnumToValue(); pin the
// ordinals since a reorder would silently change hashes for in-flight pipeline
// cache keys.
TEST(PolygonFillModeTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(PolygonFillMode::Fill), 0u);
  EXPECT_EQ(static_cast<uint8_t>(PolygonFillMode::Line), 1u);
}

// ---------------------------------------------------------------------------
// ColorWriteBits
// ---------------------------------------------------------------------------

TEST(ColorWriteBitsTest, BitsAreDistinct) {
  EXPECT_EQ(kColorWriteBitsDisabled, 0u);
  EXPECT_EQ(kColorWriteBitsRed, 1u << 0);
  EXPECT_EQ(kColorWriteBitsGreen, 1u << 1);
  EXPECT_EQ(kColorWriteBitsBlue, 1u << 2);
  EXPECT_EQ(kColorWriteBitsAlpha, 1u << 3);
  EXPECT_EQ(kColorWriteBitsAll, 15u);
}

TEST(ColorWriteBitsTest, AllCombinesAllBits) {
  EXPECT_EQ(kColorWriteBitsAll,
            kColorWriteBitsRed | kColorWriteBitsGreen | kColorWriteBitsBlue | kColorWriteBitsAlpha);
}

} // namespace igl::tests
