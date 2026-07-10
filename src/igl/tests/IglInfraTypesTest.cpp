/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <array>
#include <igl/CommandBuffer.h>
#include <igl/PlatformDevice.h>

namespace igl::tests {

// ---------------------------------------------------------------------------
// PlatformDeviceType
// ---------------------------------------------------------------------------

TEST(PlatformDeviceTypeTest, ValuesAreDistinct) {
  // PlatformDeviceType is a type discriminator used via IPlatformDevice::isType();
  // its numeric ordinals are not part of any serialization contract, so we verify
  // distinctness rather than pinning specific integer values.
  const std::array values = {
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
      // @fb-only
  };
  for (size_t outer = 0; outer < values.size(); ++outer) {
    for (size_t inner = outer + 1; inner < values.size(); ++inner) {
      EXPECT_NE(values[outer], values[inner]);
    }
  }
}

// ---------------------------------------------------------------------------
// CommandBufferDesc
// ---------------------------------------------------------------------------

TEST(CommandBufferDescTest, DefaultValues) {
  const CommandBufferDesc desc;
  EXPECT_TRUE(desc.debugName.empty());
  EXPECT_EQ(desc.timer, nullptr);
  EXPECT_EQ(desc.timestampQueries, nullptr);
}

// ---------------------------------------------------------------------------
// CommandBufferStatistics
// ---------------------------------------------------------------------------

TEST(CommandBufferStatisticsTest, DefaultDrawCountZero) {
  const CommandBufferStatistics stats;
  EXPECT_EQ(stats.currentDrawCount, 0u);
}

} // namespace igl::tests
