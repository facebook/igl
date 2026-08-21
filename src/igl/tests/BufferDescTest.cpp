/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/Buffer.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// IndexFormat
// ---------------------------------------------------------------------------

TEST(IndexFormatTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(IndexFormat::UInt8), 0u);
  EXPECT_EQ(static_cast<uint8_t>(IndexFormat::UInt16), 1u);
  EXPECT_EQ(static_cast<uint8_t>(IndexFormat::UInt32), 2u);
}

// ---------------------------------------------------------------------------
// BufferRange
// ---------------------------------------------------------------------------

TEST(BufferRangeTest, DefaultConstruction) {
  const BufferRange range;
  EXPECT_EQ(range.size, 0u);
  EXPECT_EQ(range.offset, 0u);
}

TEST(BufferRangeTest, ParameterizedConstruction) {
  const BufferRange range(256, 64);
  EXPECT_EQ(range.size, 256u);
  EXPECT_EQ(range.offset, 64u);
}

TEST(BufferRangeTest, SizeOnlyConstruction) {
  const BufferRange range(128);
  EXPECT_EQ(range.size, 128u);
  EXPECT_EQ(range.offset, 0u);
}

TEST(BufferRangeTest, ZeroSizeRange) {
  const BufferRange range(0, 100);
  EXPECT_EQ(range.size, 0u);
  EXPECT_EQ(range.offset, 100u);
}

// ---------------------------------------------------------------------------
// BufferDesc
// ---------------------------------------------------------------------------

TEST(BufferDescTest, DefaultConstruction) {
  const BufferDesc desc;
  EXPECT_EQ(desc.type, 0u);
  EXPECT_EQ(desc.data, nullptr);
  EXPECT_EQ(desc.length, 0u);
  EXPECT_EQ(desc.hint, 0u);
  EXPECT_TRUE(desc.debugName.empty());
  EXPECT_EQ(desc.storageStride, 0u);
}

TEST(BufferDescTest, TypeBitsAreDistinct) {
  EXPECT_EQ(BufferDesc::BufferTypeBits::Index, 1u << 0);
  EXPECT_EQ(BufferDesc::BufferTypeBits::Vertex, 1u << 1);
  EXPECT_EQ(BufferDesc::BufferTypeBits::Uniform, 1u << 2);
  EXPECT_EQ(BufferDesc::BufferTypeBits::Storage, 1u << 3);
  EXPECT_EQ(BufferDesc::BufferTypeBits::Indirect, 1u << 4);
}

TEST(BufferDescTest, TypeBitsCanBeCombined) {
  const BufferDesc::BufferType combined =
      BufferDesc::BufferTypeBits::Vertex | BufferDesc::BufferTypeBits::Storage;
  EXPECT_TRUE((combined & BufferDesc::BufferTypeBits::Vertex) != 0);
  EXPECT_TRUE((combined & BufferDesc::BufferTypeBits::Storage) != 0);
  EXPECT_FALSE((combined & BufferDesc::BufferTypeBits::Index) != 0);
  EXPECT_FALSE((combined & BufferDesc::BufferTypeBits::Uniform) != 0);
}

TEST(BufferDescTest, HintBitsAreDistinct) {
  EXPECT_EQ(BufferDesc::BufferAPIHintBits::Atomic, 1u << 0);
  EXPECT_EQ(BufferDesc::BufferAPIHintBits::UniformBlock, 1u << 1);
  EXPECT_EQ(BufferDesc::BufferAPIHintBits::Query, 1u << 2);
  EXPECT_EQ(BufferDesc::BufferAPIHintBits::Ring, 1u << 4);
  EXPECT_EQ(BufferDesc::BufferAPIHintBits::NoCopy, 1u << 5);
}

TEST(BufferDescTest, HintBitsCanBeCombined) {
  const BufferDesc::BufferAPIHint combined =
      BufferDesc::BufferAPIHintBits::Ring | BufferDesc::BufferAPIHintBits::NoCopy;
  EXPECT_TRUE((combined & BufferDesc::BufferAPIHintBits::Ring) != 0);
  EXPECT_TRUE((combined & BufferDesc::BufferAPIHintBits::NoCopy) != 0);
  EXPECT_FALSE((combined & BufferDesc::BufferAPIHintBits::Atomic) != 0);
}

TEST(BufferDescTest, DesignatedInitializerVertexBuffer) {
  const float verts[] = {0.0f, 1.0f, 2.0f};
  const BufferDesc desc{
      .type = BufferDesc::BufferTypeBits::Vertex,
      .data = verts,
      .length = sizeof(verts),
  };
  EXPECT_TRUE((desc.type & BufferDesc::BufferTypeBits::Vertex) != 0);
  EXPECT_EQ(desc.data, verts);
  EXPECT_EQ(desc.length, sizeof(verts));
}

TEST(BufferDescTest, DesignatedInitializerUniformBuffer) {
  const BufferDesc desc{
      .type = BufferDesc::BufferTypeBits::Uniform,
      .length = 256,
      .hint = BufferDesc::BufferAPIHintBits::Ring,
      .debugName = "perFrame",
  };
  EXPECT_TRUE((desc.type & BufferDesc::BufferTypeBits::Uniform) != 0);
  EXPECT_EQ(desc.length, 256u);
  EXPECT_TRUE((desc.hint & BufferDesc::BufferAPIHintBits::Ring) != 0);
  EXPECT_EQ(desc.debugName, "perFrame");
}

TEST(BufferDescTest, StorageStrideDefault) {
  const BufferDesc desc{
      .type = BufferDesc::BufferTypeBits::Storage,
      .length = 1024,
  };
  EXPECT_EQ(desc.storageStride, 0u);
}

TEST(BufferDescTest, StorageStrideCustom) {
  const BufferDesc desc{
      .type = BufferDesc::BufferTypeBits::Storage,
      .length = 1024,
      .storageStride = 16,
  };
  EXPECT_EQ(desc.storageStride, 16u);
}

} // namespace igl::tests
