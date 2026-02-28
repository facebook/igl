/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/opengl/Timer.h>

#include "../util/Common.h"

#include <igl/opengl/Device.h>
#include <igl/opengl/DeviceFeatureSet.h>
#include <igl/opengl/IContext.h>

namespace igl::tests {

//
// TimerOGLTest
//
// Tests for the OpenGL Timer.
//
class TimerOGLTest : public ::testing::Test {
 public:
  TimerOGLTest() = default;
  ~TimerOGLTest() override = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_NE(cmdQueue_, nullptr);

    context_ = &static_cast<opengl::Device&>(*iglDev_).getContext();
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  opengl::IContext* context_ = nullptr;
};

//
// BasicTimerQuery
//
// Create a timer, end it, and check that resultsAvailable can be queried.
//
TEST_F(TimerOGLTest, BasicTimerQuery) {
  if (!iglDev_->hasFeature(DeviceFeatures::Timers)) {
    GTEST_SKIP() << "Timer queries not supported";
  }

  Result ret;
  auto timer = iglDev_->createTimer(&ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(timer, nullptr);

  // Cast to opengl::Timer and test the end method
  auto* oglTimer = static_cast<opengl::Timer*>(timer.get());
  ASSERT_NE(oglTimer, nullptr);

  // End the timer query
  oglTimer->end();

  // Check for GL errors
  ASSERT_EQ(context_->checkForErrors(__FILE__, __LINE__), GL_NO_ERROR);

  // resultsAvailable() should be callable without crashing
  // Results may or may not be available immediately
  const bool available = oglTimer->resultsAvailable();
  // We don't assert on the value, just that calling it doesn't crash or error
  (void)available;

  // If results are available, getElapsedTimeNanos should return a value
  if (available) {
    const uint64_t elapsed = oglTimer->getElapsedTimeNanos();
    // Elapsed time should be non-negative (it's uint64_t so always >= 0)
    (void)elapsed;
  }
}

} // namespace igl::tests
