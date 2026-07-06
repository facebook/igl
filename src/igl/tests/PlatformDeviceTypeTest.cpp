/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/Common.h>
#include <igl/PlatformDevice.h>
#include <igl/RenderCommandEncoder.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// PlatformDeviceType enum
// ---------------------------------------------------------------------------

TEST(PlatformDeviceTypeTest, EnumValues) {
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::Unknown), 0);
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::Metal), 1);
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::OpenGL), 2);
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::OpenGLEgl), 3);
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::OpenGLWgl), 4);
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::OpenGLx), 5);
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::OpenGLIOS), 6);
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::OpenGLMacOS), 7);
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::OpenGLWebGL), 8);
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::Vulkan), 9);
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::D3D12), 10);
}

TEST(PlatformDeviceTypeTest, AllValuesDistinct) {
  const PlatformDeviceType values[] = {
      PlatformDeviceType::Unknown,
      PlatformDeviceType::Metal,
      PlatformDeviceType::OpenGL,
      PlatformDeviceType::OpenGLEgl,
      PlatformDeviceType::OpenGLWgl,
      PlatformDeviceType::OpenGLx,
      PlatformDeviceType::OpenGLIOS,
      PlatformDeviceType::OpenGLMacOS,
      PlatformDeviceType::OpenGLWebGL,
      PlatformDeviceType::Vulkan,
      PlatformDeviceType::D3D12,
  };
  for (size_t i = 0; i < std::size(values); ++i) {
    for (size_t j = i + 1; j < std::size(values); ++j) {
      EXPECT_NE(values[i], values[j]);
    }
  }
}

TEST(PlatformDeviceTypeTest, UnknownIsZero) {
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::Unknown), 0);
}

// ---------------------------------------------------------------------------
// BindTarget namespace constants
// ---------------------------------------------------------------------------

TEST(BindTargetConstantsTest, Values) {
  EXPECT_EQ(BindTarget::kVertex, 0x0001);
  EXPECT_EQ(BindTarget::kFragment, 0x0002);
  EXPECT_EQ(BindTarget::kAllGraphics, 0x0003);
  EXPECT_EQ(BindTarget::kTask, 0x0004);
  EXPECT_EQ(BindTarget::kMesh, 0x0008);
}

TEST(BindTargetConstantsTest, AllGraphicsIsBitwiseOrOfVertexAndFragment) {
  EXPECT_EQ(BindTarget::kAllGraphics, BindTarget::kVertex | BindTarget::kFragment);
}

TEST(BindTargetConstantsTest, TaskAndMeshDoNotOverlapGraphics) {
  EXPECT_EQ(BindTarget::kTask & BindTarget::kAllGraphics, 0);
  EXPECT_EQ(BindTarget::kMesh & BindTarget::kAllGraphics, 0);
}

TEST(BindTargetConstantsTest, IndividualBitsArePowersOfTwo) {
  EXPECT_EQ(BindTarget::kVertex & (BindTarget::kVertex - 1), 0);
  EXPECT_EQ(BindTarget::kFragment & (BindTarget::kFragment - 1), 0);
  EXPECT_EQ(BindTarget::kTask & (BindTarget::kTask - 1), 0);
  EXPECT_EQ(BindTarget::kMesh & (BindTarget::kMesh - 1), 0);
}

// ---------------------------------------------------------------------------
// Dimensions struct
// ---------------------------------------------------------------------------

TEST(DimensionsTest, DefaultConstruction) {
  const Dimensions dims;
  EXPECT_EQ(dims.width, 0u);
  EXPECT_EQ(dims.height, 0u);
  EXPECT_EQ(dims.depth, 0u);
}

TEST(DimensionsTest, ParameterizedConstruction) {
  const Dimensions dims(128, 256, 64);
  EXPECT_EQ(dims.width, 128u);
  EXPECT_EQ(dims.height, 256u);
  EXPECT_EQ(dims.depth, 64u);
}

TEST(DimensionsTest, Equality) {
  const Dimensions a(10, 20, 30);
  const Dimensions b(10, 20, 30);
  EXPECT_EQ(a, b);
}

TEST(DimensionsTest, InequalityWidth) {
  const Dimensions a(10, 20, 30);
  const Dimensions b(11, 20, 30);
  EXPECT_NE(a, b);
}

TEST(DimensionsTest, InequalityHeight) {
  const Dimensions a(10, 20, 30);
  const Dimensions b(10, 21, 30);
  EXPECT_NE(a, b);
}

TEST(DimensionsTest, InequalityDepth) {
  const Dimensions a(10, 20, 30);
  const Dimensions b(10, 20, 31);
  EXPECT_NE(a, b);
}

TEST(DimensionsTest, DefaultEqualsDefault) {
  const Dimensions a;
  const Dimensions b;
  EXPECT_EQ(a, b);
}

} // namespace igl::tests
