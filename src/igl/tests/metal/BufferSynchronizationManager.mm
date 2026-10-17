/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/metal/BufferSynchronizationManager.h>

#include "../util/Common.h"

#include <chrono>
#include <future>
#include <thread>
#include <igl/IGL.h>
#include <igl/metal/CommandBuffer.h>

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

  void SetUp() override {
    setDebugBreakEnabled(false);
    bufferSyncManager_ =
        std::make_shared<metal::BufferSynchronizationManager>(IGL_METAL_MAX_IN_FLIGHT_BUFFERS);

    util::createDeviceAndQueue(device_, commandQueue_);
  }

  void TearDown() override {}

 protected:
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

//
// ThirdEndOfFrameSyncWaitsForCompletedFrame
//
// CommandQueue skips the first frame's wait, so the third call is frame 3's, which reuses frame 0's
// buffer: it must block until an end-of-frame command buffer completes.
//
TEST_F(BufferSynchronizationManagerMTLTest, ThirdEndOfFrameSyncWaitsForCompletedFrame) {
  Result ret;
  auto cmdBuffer = commandQueue_->createCommandBuffer({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(cmdBuffer, nullptr);

  bufferSyncManager_->manageEndOfFrameSync();
  bufferSyncManager_->manageEndOfFrameSync();

  std::promise<void> thirdSyncReturned;
  std::future<void> thirdSync = thirdSyncReturned.get_future();
  std::thread waiter(
      [manager = bufferSyncManager_, returned = std::move(thirdSyncReturned)]() mutable {
        manager->manageEndOfFrameSync();
        returned.set_value();
      });

  const bool heldBack =
      thirdSync.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout;

  bufferSyncManager_->markCommandBufferAsEndOfFrame(*cmdBuffer);
  commandQueue_->submit(*cmdBuffer, /*endOfFrame=*/false);

  const bool released = thirdSync.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
  if (released) {
    waiter.join();
    EXPECT_EQ(bufferSyncManager_->getCurrentInFlightBufferIndex(), 0u);
  } else {
    // The waiter owns a reference to the manager, so detaching cannot destroy a semaphore in use.
    waiter.detach();
  }
  EXPECT_TRUE(heldBack);
  EXPECT_TRUE(released);
}

} // namespace igl::tests
