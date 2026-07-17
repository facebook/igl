/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/CommandQueue.h>
#include <igl/PlatformDevice.h>

namespace igl::tests {

TEST(PlatformDeviceTypeTest, UnknownValue) {
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::Unknown), 0);
}

TEST(PlatformDeviceTypeTest, MetalValue) {
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::Metal), 1);
}

TEST(PlatformDeviceTypeTest, OpenGLValue) {
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::OpenGL), 2);
}

TEST(PlatformDeviceTypeTest, OpenGLEglValue) {
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::OpenGLEgl), 3);
}

TEST(PlatformDeviceTypeTest, OpenGLWglValue) {
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::OpenGLWgl), 4);
}

TEST(PlatformDeviceTypeTest, OpenGLxValue) {
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::OpenGLx), 5);
}

TEST(PlatformDeviceTypeTest, OpenGLIOSValue) {
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::OpenGLIOS), 6);
}

TEST(PlatformDeviceTypeTest, OpenGLMacOSValue) {
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::OpenGLMacOS), 7);
}

TEST(PlatformDeviceTypeTest, OpenGLWebGLValue) {
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::OpenGLWebGL), 8);
}

TEST(PlatformDeviceTypeTest, VulkanValue) {
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::Vulkan), 9);
}

TEST(PlatformDeviceTypeTest, D3D12Value) {
  EXPECT_EQ(static_cast<int>(PlatformDeviceType::D3D12), 10);
}

TEST(PlatformDeviceTypeTest, AllValuesUnique) {
  const std::vector<PlatformDeviceType> allTypes = {
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
  for (size_t i = 0; i < allTypes.size(); ++i) {
    for (size_t j = i + 1; j < allTypes.size(); ++j) {
      EXPECT_NE(allTypes[i], allTypes[j]);
    }
  }
}

TEST(PlatformDeviceTypeTest, CopyAndCompare) {
  const PlatformDeviceType a = PlatformDeviceType::Vulkan;
  const PlatformDeviceType b = a;
  EXPECT_EQ(a, b);
}

TEST(PlatformDeviceTypeTest, SwitchCoverage) {
  auto toString = [](PlatformDeviceType type) -> const char* {
    switch (type) {
    case PlatformDeviceType::Unknown:
      return "Unknown";
    case PlatformDeviceType::Metal:
      return "Metal";
    case PlatformDeviceType::OpenGL:
      return "OpenGL";
    case PlatformDeviceType::OpenGLEgl:
      return "OpenGLEgl";
    case PlatformDeviceType::OpenGLWgl:
      return "OpenGLWgl";
    case PlatformDeviceType::OpenGLx:
      return "OpenGLx";
    case PlatformDeviceType::OpenGLIOS:
      return "OpenGLIOS";
    case PlatformDeviceType::OpenGLMacOS:
      return "OpenGLMacOS";
    case PlatformDeviceType::OpenGLWebGL:
      return "OpenGLWebGL";
    case PlatformDeviceType::Vulkan:
      return "Vulkan";
    case PlatformDeviceType::D3D12:
      return "D3D12";
    // @fb-only
      // @fb-only
    }
    return "InvalidPlatformDeviceType";
  };

  EXPECT_STREQ(toString(PlatformDeviceType::Unknown), "Unknown");
  EXPECT_STREQ(toString(PlatformDeviceType::Vulkan), "Vulkan");
  EXPECT_STREQ(toString(PlatformDeviceType::Metal), "Metal");
  EXPECT_STREQ(toString(PlatformDeviceType::D3D12), "D3D12");
}

TEST(CommandQueueStatisticsTest, DefaultFieldsAreZero) {
  const CommandQueueStatistics stats;
  EXPECT_EQ(stats.currentDrawCount, 0u);
  EXPECT_EQ(stats.lastFrameDrawCount, 0u);
}

TEST(CommandQueueStatisticsTest, ModifyCurrentDrawCount) {
  CommandQueueStatistics stats;
  stats.currentDrawCount = 42;
  EXPECT_EQ(stats.currentDrawCount, 42u);
  EXPECT_EQ(stats.lastFrameDrawCount, 0u);
}

TEST(CommandQueueStatisticsTest, ModifyLastFrameDrawCount) {
  CommandQueueStatistics stats;
  stats.lastFrameDrawCount = 100;
  EXPECT_EQ(stats.currentDrawCount, 0u);
  EXPECT_EQ(stats.lastFrameDrawCount, 100u);
}

TEST(CommandQueueStatisticsTest, CopyConstruct) {
  CommandQueueStatistics stats;
  stats.currentDrawCount = 10;
  stats.lastFrameDrawCount = 20;
  const CommandQueueStatistics copy = stats;
  EXPECT_EQ(copy.currentDrawCount, 10u);
  EXPECT_EQ(copy.lastFrameDrawCount, 20u);
}

TEST(CommandQueueStatisticsTest, CopyAssignment) {
  CommandQueueStatistics a;
  a.currentDrawCount = 5;
  a.lastFrameDrawCount = 15;
  CommandQueueStatistics b;
  b = a;
  EXPECT_EQ(b.currentDrawCount, 5u);
  EXPECT_EQ(b.lastFrameDrawCount, 15u);
}

TEST(CommandQueueStatisticsTest, DesignatedInitializer) {
  const CommandQueueStatistics stats{.currentDrawCount = 7, .lastFrameDrawCount = 3};
  EXPECT_EQ(stats.currentDrawCount, 7u);
  EXPECT_EQ(stats.lastFrameDrawCount, 3u);
}

TEST(CommandQueueDescTest, DefaultConstruct) {
  const CommandQueueDesc desc;
  (void)desc;
}

TEST(CommandQueueDescTest, CopyConstruct) {
  const CommandQueueDesc original;
  const CommandQueueDesc copy = original;
  (void)copy;
}

TEST(SubmitHandleTest, DefaultValue) {
  const SubmitHandle handle = 0;
  EXPECT_EQ(handle, 0u);
}

TEST(SubmitHandleTest, IsUint64) {
  static_assert(std::is_same_v<SubmitHandle, uint64_t>);
}

TEST(SubmitHandleTest, AssignAndCompare) {
  const SubmitHandle a = 12345;
  const SubmitHandle b = 12345;
  EXPECT_EQ(a, b);
}

} // namespace igl::tests
