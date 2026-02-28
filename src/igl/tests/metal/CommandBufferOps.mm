/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <cstring>
#include <igl/IGL.h>
#include <igl/metal/CommandBuffer.h>
#include <igl/metal/Device.h>

namespace igl::tests {

//
// MetalCommandBufferOpsTest
//
// This test covers command buffer operations on Metal.
// Tests debug labels, buffer copying, and wait until completed.
//
class MetalCommandBufferOpsTest : public ::testing::Test {
 public:
  MetalCommandBufferOpsTest() = default;
  ~MetalCommandBufferOpsTest() override = default;

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
// DebugLabels
//
// Test pushing and popping debug labels on a command buffer without crash.
//
TEST_F(MetalCommandBufferOpsTest, DebugLabels) {
  Result res;
  const CommandBufferDesc cbDesc;
  auto cmdBuf = cmdQueue_->createCommandBuffer(cbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(cmdBuf, nullptr);

  cmdBuf->pushDebugGroupLabel("TestGroup", Color(1, 0, 0, 1));
  cmdBuf->popDebugGroupLabel();
}

//
// CopyBuffer
//
// Test copying data between two buffers via a command buffer.
//
TEST_F(MetalCommandBufferOpsTest, CopyBuffer) {
  Result res;

  // Create source buffer with data
  const float srcData[] = {1.0f, 2.0f, 3.0f, 4.0f};
  BufferDesc srcDesc(
      BufferDesc::BufferTypeBits::Storage, srcData, sizeof(srcData), ResourceStorage::Shared);
  auto srcBuffer = device_->createBuffer(srcDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  // Create destination buffer (empty)
  BufferDesc dstDesc(
      BufferDesc::BufferTypeBits::Storage, nullptr, sizeof(srcData), ResourceStorage::Shared);
  auto dstBuffer = device_->createBuffer(dstDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  // Create command buffer and copy
  const CommandBufferDesc cbDesc;
  auto cmdBuf = cmdQueue_->createCommandBuffer(cbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  cmdBuf->copyBuffer(*srcBuffer, *dstBuffer, 0, 0, sizeof(srcData));

  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();

  // Verify destination buffer contents
  Result mapRes;
  auto* mapped = static_cast<float*>(dstBuffer->map(BufferRange(sizeof(srcData), 0), &mapRes));
  ASSERT_TRUE(mapRes.isOk()) << mapRes.message;
  ASSERT_NE(mapped, nullptr);
  ASSERT_EQ(mapped[0], 1.0f);
  ASSERT_EQ(mapped[1], 2.0f);
  ASSERT_EQ(mapped[2], 3.0f);
  ASSERT_EQ(mapped[3], 4.0f);
  dstBuffer->unmap();
}

//
// WaitUntilCompleted
//
// Test that waitUntilCompleted returns without timeout on a submitted command buffer.
//
TEST_F(MetalCommandBufferOpsTest, WaitUntilCompleted) {
  Result res;
  const CommandBufferDesc cbDesc;
  auto cmdBuf = cmdQueue_->createCommandBuffer(cbDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  // Create a compute encoder to make the command buffer non-empty
  auto encoder = cmdBuf->createComputeCommandEncoder();
  ASSERT_NE(encoder, nullptr);
  encoder->endEncoding();

  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();
  // If we reach here, the wait completed successfully
}

} // namespace igl::tests
