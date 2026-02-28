/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <igl/IGL.h>
#include <igl/metal/Device.h>

namespace igl::tests {

//
// MetalStorageModeTest
//
// This test covers the mapping of IGL ResourceStorage modes to Metal storage modes.
//
class MetalStorageModeTest : public ::testing::Test {
 public:
  MetalStorageModeTest() = default;
  ~MetalStorageModeTest() override = default;

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
// PrivateStorage
//
// Test that a buffer with Private storage mode is created successfully.
//
TEST_F(MetalStorageModeTest, PrivateStorage) {
  Result res;

  BufferDesc desc(BufferDesc::BufferTypeBits::Uniform, nullptr, 256, ResourceStorage::Private);
  desc.debugName = "privateBuffer";

  auto buffer = device_->createBuffer(desc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(buffer, nullptr);
  ASSERT_EQ(buffer->storage(), ResourceStorage::Private);
}

//
// SharedStorage
//
// Test that a buffer with Shared storage mode is created successfully and reports Shared.
//
TEST_F(MetalStorageModeTest, SharedStorage) {
  Result res;

  BufferDesc desc(BufferDesc::BufferTypeBits::Uniform, nullptr, 256, ResourceStorage::Shared);
  desc.debugName = "sharedBuffer";

  auto buffer = device_->createBuffer(desc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(buffer, nullptr);
  ASSERT_EQ(buffer->storage(), ResourceStorage::Shared);
}

} // namespace igl::tests
