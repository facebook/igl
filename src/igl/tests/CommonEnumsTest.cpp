/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/Common.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// BackendType
// ---------------------------------------------------------------------------

TEST(BackendTypeTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(BackendType::Invalid), 0u);
  EXPECT_EQ(static_cast<uint8_t>(BackendType::OpenGL), 1u);
  EXPECT_EQ(static_cast<uint8_t>(BackendType::Metal), 2u);
  EXPECT_EQ(static_cast<uint8_t>(BackendType::Vulkan), 3u);
  EXPECT_EQ(static_cast<uint8_t>(BackendType::D3D12), 4u);
  // @fb-only
  // @fb-only
}

// ---------------------------------------------------------------------------
// NormalizedZRange
// ---------------------------------------------------------------------------

TEST(NormalizedZRangeTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(NormalizedZRange::NegOneToOne), 0u);
  EXPECT_EQ(static_cast<uint8_t>(NormalizedZRange::ZeroToOne), 1u);
}

TEST(NormalizedZRangeTest, ValuesAreDistinct) {
  EXPECT_NE(NormalizedZRange::NegOneToOne, NormalizedZRange::ZeroToOne);
}

// ---------------------------------------------------------------------------
// ResourceStorage
// ---------------------------------------------------------------------------

TEST(ResourceStorageTest, InvalidIsZero) {
  EXPECT_EQ(static_cast<int>(ResourceStorage::Invalid), 0);
}

TEST(ResourceStorageTest, AllValuesDistinct) {
  EXPECT_NE(ResourceStorage::Invalid, ResourceStorage::Private);
  EXPECT_NE(ResourceStorage::Invalid, ResourceStorage::Shared);
  EXPECT_NE(ResourceStorage::Invalid, ResourceStorage::Managed);
  EXPECT_NE(ResourceStorage::Invalid, ResourceStorage::Memoryless);
  EXPECT_NE(ResourceStorage::Private, ResourceStorage::Shared);
  EXPECT_NE(ResourceStorage::Private, ResourceStorage::Managed);
  EXPECT_NE(ResourceStorage::Private, ResourceStorage::Memoryless);
  EXPECT_NE(ResourceStorage::Shared, ResourceStorage::Managed);
  EXPECT_NE(ResourceStorage::Shared, ResourceStorage::Memoryless);
  EXPECT_NE(ResourceStorage::Managed, ResourceStorage::Memoryless);
}

// ---------------------------------------------------------------------------
// Result::Code
// ---------------------------------------------------------------------------

TEST(ResultCodeTest, OkIsZero) {
  EXPECT_EQ(static_cast<int>(Result::Code::Ok), 0);
}

TEST(ResultCodeTest, ErrorCodesAreNonZero) {
  EXPECT_NE(static_cast<int>(Result::Code::ArgumentInvalid), 0);
  EXPECT_NE(static_cast<int>(Result::Code::ArgumentNull), 0);
  EXPECT_NE(static_cast<int>(Result::Code::ArgumentOutOfRange), 0);
  EXPECT_NE(static_cast<int>(Result::Code::InvalidOperation), 0);
  EXPECT_NE(static_cast<int>(Result::Code::Unsupported), 0);
  EXPECT_NE(static_cast<int>(Result::Code::Unimplemented), 0);
  EXPECT_NE(static_cast<int>(Result::Code::RuntimeError), 0);
  EXPECT_NE(static_cast<int>(Result::Code::DeviceLost), 0);
}

TEST(ResultCodeTest, AllCodesDistinct) {
  EXPECT_NE(Result::Code::Ok, Result::Code::ArgumentInvalid);
  EXPECT_NE(Result::Code::Ok, Result::Code::ArgumentNull);
  EXPECT_NE(Result::Code::Ok, Result::Code::ArgumentOutOfRange);
  EXPECT_NE(Result::Code::Ok, Result::Code::InvalidOperation);
  EXPECT_NE(Result::Code::Ok, Result::Code::Unsupported);
  EXPECT_NE(Result::Code::Ok, Result::Code::Unimplemented);
  EXPECT_NE(Result::Code::Ok, Result::Code::RuntimeError);
  EXPECT_NE(Result::Code::Ok, Result::Code::DeviceLost);
  EXPECT_NE(Result::Code::ArgumentInvalid, Result::Code::ArgumentNull);
  EXPECT_NE(Result::Code::ArgumentInvalid, Result::Code::ArgumentOutOfRange);
  EXPECT_NE(Result::Code::ArgumentInvalid, Result::Code::InvalidOperation);
  EXPECT_NE(Result::Code::ArgumentInvalid, Result::Code::Unsupported);
  EXPECT_NE(Result::Code::ArgumentInvalid, Result::Code::Unimplemented);
  EXPECT_NE(Result::Code::ArgumentInvalid, Result::Code::RuntimeError);
  EXPECT_NE(Result::Code::ArgumentInvalid, Result::Code::DeviceLost);
  EXPECT_NE(Result::Code::ArgumentNull, Result::Code::ArgumentOutOfRange);
  EXPECT_NE(Result::Code::ArgumentNull, Result::Code::InvalidOperation);
  EXPECT_NE(Result::Code::ArgumentNull, Result::Code::Unsupported);
  EXPECT_NE(Result::Code::ArgumentNull, Result::Code::Unimplemented);
  EXPECT_NE(Result::Code::ArgumentNull, Result::Code::RuntimeError);
  EXPECT_NE(Result::Code::ArgumentNull, Result::Code::DeviceLost);
  EXPECT_NE(Result::Code::ArgumentOutOfRange, Result::Code::InvalidOperation);
  EXPECT_NE(Result::Code::ArgumentOutOfRange, Result::Code::Unsupported);
  EXPECT_NE(Result::Code::ArgumentOutOfRange, Result::Code::Unimplemented);
  EXPECT_NE(Result::Code::ArgumentOutOfRange, Result::Code::RuntimeError);
  EXPECT_NE(Result::Code::ArgumentOutOfRange, Result::Code::DeviceLost);
  EXPECT_NE(Result::Code::InvalidOperation, Result::Code::Unsupported);
  EXPECT_NE(Result::Code::InvalidOperation, Result::Code::Unimplemented);
  EXPECT_NE(Result::Code::InvalidOperation, Result::Code::RuntimeError);
  EXPECT_NE(Result::Code::InvalidOperation, Result::Code::DeviceLost);
  EXPECT_NE(Result::Code::Unsupported, Result::Code::Unimplemented);
  EXPECT_NE(Result::Code::Unsupported, Result::Code::RuntimeError);
  EXPECT_NE(Result::Code::Unsupported, Result::Code::DeviceLost);
  EXPECT_NE(Result::Code::Unimplemented, Result::Code::RuntimeError);
  EXPECT_NE(Result::Code::Unimplemented, Result::Code::DeviceLost);
  EXPECT_NE(Result::Code::RuntimeError, Result::Code::DeviceLost);
}

// ---------------------------------------------------------------------------
// CullMode
// ---------------------------------------------------------------------------

TEST(CullModeTest, AllValuesDistinct) {
  EXPECT_NE(CullMode::Disabled, CullMode::Front);
  EXPECT_NE(CullMode::Disabled, CullMode::Back);
  EXPECT_NE(CullMode::Front, CullMode::Back);
}

// ---------------------------------------------------------------------------
// WindingMode
// ---------------------------------------------------------------------------

TEST(WindingModeTest, ValuesAreDistinct) {
  EXPECT_NE(WindingMode::Clockwise, WindingMode::CounterClockwise);
}

// ---------------------------------------------------------------------------
// PrimitiveType
// ---------------------------------------------------------------------------

TEST(PrimitiveTypeTest, AllValuesDistinct) {
  EXPECT_NE(PrimitiveType::Point, PrimitiveType::Line);
  EXPECT_NE(PrimitiveType::Line, PrimitiveType::LineStrip);
  EXPECT_NE(PrimitiveType::LineStrip, PrimitiveType::Triangle);
  EXPECT_NE(PrimitiveType::Triangle, PrimitiveType::TriangleStrip);
  EXPECT_NE(PrimitiveType::Point, PrimitiveType::TriangleStrip);
}

} // namespace igl::tests
