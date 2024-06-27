/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/BufferSynchronizationManager.h>
#include <igl/metal/CommandBuffer.h>

#include "../util/Common.h"

#include <gtest/gtest.h>
#include <igl/IGL.h>

namespace igl::tests {

//
// CommandBufferMTLTest
//
// This test covers igl::metal::CommandBuffer.
// Most of the testing revolves confirming successful instantiation
// and creation of encoders.
//
#define IGL_METAL_MAX_IN_FLIGHT_BUFFERS 3
class BufferSynchronizationManagerMTLTest : public ::testing::Test {
 public:
  BufferSynchronizationManagerMTLTest() = default;
  ~BufferSynchronizationManagerMTLTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);
    bufferSyncManager_ =
        std::make_shared<metal::BufferSynchronizationManager>(IGL_METAL_MAX_IN_FLIGHT_BUFFERS);

    util::createDeviceAndQueue(device_, commandQueue_);
  }

  void TearDown() override {}

 public:
  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ICommandQueue> commandQueue_;
  std::shared_ptr<metal::BufferSynchronizationManager> bufferSyncManager_;
};

//
// CommandBufferCreation
//
// Test successful creation of MTLCommandBuffer.
//
TEST_F(BufferSynchronizationManagerMTLTest, BufferSynchronizationManagerCreation) {
  ASSERT_EQ(bufferSyncManager_->getMaxInflightBuffers(), IGL_METAL_MAX_IN_FLIGHT_BUFFERS);
  ASSERT_EQ(bufferSyncManager_->getCurrentInFlightBufferIndex(), 0);
}

TEST_F(BufferSynchronizationManagerMTLTest, BufferSynchronizationManagerEndOfFrameSync) {
  bufferSyncManager_->manageEndOfFrameSync();
  ASSERT_EQ(bufferSyncManager_->getCurrentInFlightBufferIndex(), 1);
}

} // namespace igl::tests
