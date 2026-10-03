/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Device-free (RecordingDevice), so iglHostTests runs these on every host.

#include <gtest/gtest.h>

#include "RecordingDevice.h"

#include <IGLU/managedUniformBuffer/ManagedUniformBuffer.h>

namespace igl::tests {

TEST(ManagedUniformBufferBindBytesTest, UsesBindBytesWhenTheDeviceSupportsTheSize) {
  RecordingDevice device;
  device.reportsBindBytes = true;
  device.maxBindBytesBytes = 4096;
  iglu::ManagedUniformBuffer buffer(device, {.index = 0, .length = 20});

  // bindBytes() takes the CPU copy, so no buffer is created to upload into.
  EXPECT_EQ(device.createBufferCount, 0u);
  // The copy is rounded up to 16 bytes and zero-filled past the requested length.
  ASSERT_NE(buffer.getData(), nullptr);
  const auto* bytes = static_cast<const uint8_t*>(buffer.getData());
  for (size_t i = 0; i < 32; ++i) {
    EXPECT_EQ(bytes[i], 0u) << i;
  }
}

TEST(ManagedUniformBufferBindBytesTest, CreatesABufferWhenTooLargeForBindBytes) {
  RecordingDevice device;
  device.reportsBindBytes = true;
  device.maxBindBytesBytes = 16;
  const iglu::ManagedUniformBuffer buffer(device, {.index = 0, .length = 32});

  EXPECT_EQ(device.createBufferCount, 1u);
  EXPECT_EQ(device.recordedType, BufferDesc::BufferTypeBits::Uniform);
}

TEST(ManagedUniformBufferBindBytesTest, ComparesTheRoundedSizeWithTheBindBytesLimit) {
  RecordingDevice device;
  device.reportsBindBytes = true;
  device.maxBindBytesBytes = 20;
  const iglu::ManagedUniformBuffer buffer(device, {.index = 0, .length = 20});

  EXPECT_EQ(device.createBufferCount, 1u);
}

TEST(ManagedUniformBufferBindBytesTest, CreatesABufferWithoutTheBindBytesFeature) {
  RecordingDevice device;
  device.maxBindBytesBytes = 4096;
  const iglu::ManagedUniformBuffer buffer(device, {.index = 0, .length = 16});

  EXPECT_EQ(device.createBufferCount, 1u);
}

TEST(ManagedUniformBufferBindBytesTest, CreatesABufferWithoutABindBytesLimit) {
  RecordingDevice device;
  device.reportsBindBytes = true;
  const iglu::ManagedUniformBuffer buffer(device, {.index = 0, .length = 16});

  EXPECT_EQ(device.createBufferCount, 1u);
}

} // namespace igl::tests
