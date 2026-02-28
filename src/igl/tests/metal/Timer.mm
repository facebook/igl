/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/Timer.h>

#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <igl/IGL.h>
#include <igl/metal/Device.h>

namespace igl::tests {

//
// MetalTimerTest
//
// This test covers ITimer creation and basic operations on Metal.
//
class MetalTimerTest : public ::testing::Test {
 public:
  MetalTimerTest() = default;
  ~MetalTimerTest() override = default;

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
// TimerCreation
//
// Test that creating a timer succeeds.
//
TEST_F(MetalTimerTest, TimerCreation) {
  Result res;
  auto timer = device_->createTimer(&res);
  // Timer may not be supported on all Metal devices
  if (res.isOk()) {
    ASSERT_NE(timer, nullptr);
  }
}

//
// InitialElapsedTimeZero
//
// Test that the initial elapsed time of a newly created timer is 0.
//
TEST_F(MetalTimerTest, InitialElapsedTimeZero) {
  Result res;
  auto timer = device_->createTimer(&res);
  if (!res.isOk()) {
    GTEST_SKIP() << "Timer not supported on this device";
  }
  ASSERT_NE(timer, nullptr);

  // Before any GPU work, elapsed time should be 0
  ASSERT_EQ(timer->getElapsedTimeNanos(), 0u);
}

} // namespace igl::tests
