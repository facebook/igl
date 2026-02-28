/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/metal/Buffer.h>

#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <cstring>
#include <igl/IGL.h>
#include <igl/metal/Device.h>

namespace igl::tests {

//
// MetalBufferTest
//
// This test covers igl::metal::Buffer.
// Tests creation, upload, mapping, and storage modes of Metal buffers.
//
class MetalBufferTest : public ::testing::Test {
 public:
  MetalBufferTest() = default;
  ~MetalBufferTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(device_, cmdQueue_);
    ASSERT_NE(device_, nullptr);
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

//
// CreateBufferWithData
//
// Test creating a buffer with initial data, verify size and content.
//
TEST_F(MetalBufferTest, CreateBufferWithData) {
  const float data[] = {1.0f, 2.0f, 3.0f, 4.0f};
  const size_t dataSize = sizeof(data);

  BufferDesc desc(BufferDesc::BufferTypeBits::Uniform, data, dataSize, ResourceStorage::Shared);
  desc.debugName = "testBufferWithData";

  Result res;
  auto buffer = device_->createBuffer(desc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(buffer, nullptr);

  ASSERT_EQ(buffer->getSizeInBytes(), dataSize);

  // Map and verify data
  Result mapRes;
  auto* mapped = static_cast<float*>(buffer->map(BufferRange(dataSize, 0), &mapRes));
  ASSERT_TRUE(mapRes.isOk()) << mapRes.message;
  ASSERT_NE(mapped, nullptr);
  ASSERT_EQ(mapped[0], 1.0f);
  ASSERT_EQ(mapped[1], 2.0f);
  ASSERT_EQ(mapped[2], 3.0f);
  ASSERT_EQ(mapped[3], 4.0f);
  buffer->unmap();
}

//
// CreateBufferEmpty
//
// Test creating a buffer without initial data.
//
TEST_F(MetalBufferTest, CreateBufferEmpty) {
  const size_t bufferSize = 256;

  BufferDesc desc(
      BufferDesc::BufferTypeBits::Uniform, nullptr, bufferSize, ResourceStorage::Shared);
  desc.debugName = "testBufferEmpty";

  Result res;
  auto buffer = device_->createBuffer(desc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(buffer, nullptr);
  ASSERT_EQ(buffer->getSizeInBytes(), bufferSize);
}

//
// BufferUploadAndVerify
//
// Test uploading data to a buffer, then mapping and verifying the data matches.
//
TEST_F(MetalBufferTest, BufferUploadAndVerify) {
  const size_t bufferSize = 4 * sizeof(float);

  BufferDesc desc(
      BufferDesc::BufferTypeBits::Uniform, nullptr, bufferSize, ResourceStorage::Shared);
  desc.debugName = "testBufferUpload";

  Result res;
  auto buffer = device_->createBuffer(desc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(buffer, nullptr);

  const float uploadData[] = {10.0f, 20.0f, 30.0f, 40.0f};
  res = buffer->upload(uploadData, BufferRange(sizeof(uploadData), 0));
  ASSERT_TRUE(res.isOk()) << res.message;

  Result mapRes;
  auto* mapped = static_cast<float*>(buffer->map(BufferRange(bufferSize, 0), &mapRes));
  ASSERT_TRUE(mapRes.isOk()) << mapRes.message;
  ASSERT_NE(mapped, nullptr);
  ASSERT_EQ(mapped[0], 10.0f);
  ASSERT_EQ(mapped[1], 20.0f);
  ASSERT_EQ(mapped[2], 30.0f);
  ASSERT_EQ(mapped[3], 40.0f);
  buffer->unmap();
}

//
// BufferMapUnmap
//
// Test mapping, writing, unmapping, then re-mapping to verify written data.
//
TEST_F(MetalBufferTest, BufferMapUnmap) {
  const size_t bufferSize = 4 * sizeof(float);

  BufferDesc desc(
      BufferDesc::BufferTypeBits::Uniform, nullptr, bufferSize, ResourceStorage::Shared);
  desc.debugName = "testMapUnmap";

  Result res;
  auto buffer = device_->createBuffer(desc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(buffer, nullptr);

  // Map and write data
  Result mapRes;
  auto* mapped = static_cast<float*>(buffer->map(BufferRange(bufferSize, 0), &mapRes));
  ASSERT_TRUE(mapRes.isOk()) << mapRes.message;
  ASSERT_NE(mapped, nullptr);
  mapped[0] = 100.0f;
  mapped[1] = 200.0f;
  mapped[2] = 300.0f;
  mapped[3] = 400.0f;
  buffer->unmap();

  // Re-map and verify
  auto* mapped2 = static_cast<float*>(buffer->map(BufferRange(bufferSize, 0), &mapRes));
  ASSERT_TRUE(mapRes.isOk()) << mapRes.message;
  ASSERT_NE(mapped2, nullptr);
  ASSERT_EQ(mapped2[0], 100.0f);
  ASSERT_EQ(mapped2[1], 200.0f);
  ASSERT_EQ(mapped2[2], 300.0f);
  ASSERT_EQ(mapped2[3], 400.0f);
  buffer->unmap();
}

//
// StorageModeShared
//
// Verify that a buffer created with Shared storage mode reports Shared.
//
TEST_F(MetalBufferTest, StorageModeShared) {
  const size_t bufferSize = 64;

  BufferDesc desc(
      BufferDesc::BufferTypeBits::Uniform, nullptr, bufferSize, ResourceStorage::Shared);
  desc.debugName = "testSharedStorage";

  Result res;
  auto buffer = device_->createBuffer(desc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(buffer, nullptr);
  ASSERT_EQ(buffer->storage(), ResourceStorage::Shared);
}

//
// RingBufferCreation
//
// Create a ring buffer and verify it is non-null.
//
TEST_F(MetalBufferTest, RingBufferCreation) {
  const size_t bufferSize = 256;

  BufferDesc desc(
      BufferDesc::BufferTypeBits::Uniform, nullptr, bufferSize, ResourceStorage::Shared);
  desc.hint = BufferDesc::BufferAPIHintBits::Ring;
  desc.debugName = "testRingBuffer";

  Result res;
  auto buffer = device_->createBuffer(desc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(buffer, nullptr);
  ASSERT_EQ(buffer->getSizeInBytes(), bufferSize);
}

} // namespace igl::tests
