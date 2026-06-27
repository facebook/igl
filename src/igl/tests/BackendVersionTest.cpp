/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/DeviceFeatures.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// BackendFlavor
// ---------------------------------------------------------------------------

TEST(BackendFlavorTest, EnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(BackendFlavor::Invalid), 0u);
  EXPECT_EQ(static_cast<uint8_t>(BackendFlavor::OpenGL), 1u);
  EXPECT_EQ(static_cast<uint8_t>(BackendFlavor::OpenGL_ES), 2u);
  EXPECT_EQ(static_cast<uint8_t>(BackendFlavor::Metal), 3u);
  EXPECT_EQ(static_cast<uint8_t>(BackendFlavor::Vulkan), 4u);
  EXPECT_EQ(static_cast<uint8_t>(BackendFlavor::D3D12), 5u);
}

// ---------------------------------------------------------------------------
// BackendVersion
// ---------------------------------------------------------------------------

TEST(BackendVersionTest, DefaultConstruction) {
  const BackendVersion bv;
  EXPECT_EQ(bv.flavor, BackendFlavor::Invalid);
  EXPECT_EQ(bv.majorVersion, 0u);
  EXPECT_EQ(bv.minorVersion, 0u);
}

TEST(BackendVersionTest, DesignatedInitializer) {
  const BackendVersion bv{
      .flavor = BackendFlavor::Vulkan,
      .majorVersion = 1,
      .minorVersion = 3,
  };
  EXPECT_EQ(bv.flavor, BackendFlavor::Vulkan);
  EXPECT_EQ(bv.majorVersion, 1u);
  EXPECT_EQ(bv.minorVersion, 3u);
}

TEST(BackendVersionTest, EqualityReflexive) {
  const BackendVersion bv{.flavor = BackendFlavor::Metal, .majorVersion = 3, .minorVersion = 0};
  EXPECT_EQ(bv, bv);
}

TEST(BackendVersionTest, EqualitySameValues) {
  const BackendVersion a{.flavor = BackendFlavor::Vulkan, .majorVersion = 1, .minorVersion = 3};
  const BackendVersion b{.flavor = BackendFlavor::Vulkan, .majorVersion = 1, .minorVersion = 3};
  EXPECT_EQ(a, b);
}

TEST(BackendVersionTest, InequalityDifferentFlavor) {
  const BackendVersion a{.flavor = BackendFlavor::Vulkan, .majorVersion = 1, .minorVersion = 0};
  const BackendVersion b{.flavor = BackendFlavor::Metal, .majorVersion = 1, .minorVersion = 0};
  EXPECT_NE(a, b);
}

TEST(BackendVersionTest, InequalityDifferentMajorVersion) {
  const BackendVersion a{.flavor = BackendFlavor::Vulkan, .majorVersion = 1, .minorVersion = 0};
  const BackendVersion b{.flavor = BackendFlavor::Vulkan, .majorVersion = 2, .minorVersion = 0};
  EXPECT_NE(a, b);
}

TEST(BackendVersionTest, InequalityDifferentMinorVersion) {
  const BackendVersion a{.flavor = BackendFlavor::Vulkan, .majorVersion = 1, .minorVersion = 0};
  const BackendVersion b{.flavor = BackendFlavor::Vulkan, .majorVersion = 1, .minorVersion = 3};
  EXPECT_NE(a, b);
}

} // namespace igl::tests
