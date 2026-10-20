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
#include <igl/opengl/IContext.h>
#include <igl/opengl/TimestampQueries.h>

namespace igl::tests {

//
// TimerOGLTest
//
// Tests for the OpenGL Timer.
//
class TimerOGLTest : public ::testing::Test {
 public:
  TimerOGLTest() = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    util::createDeviceAndQueue(iglDev_, cmdQueue_);
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_NE(cmdQueue_, nullptr);

    context_ = &static_cast<opengl::Device&>(*iglDev_).getContext();
    availabilityReadCount = 0;
    resultReadCount = 0;
    available = 1;
  }

  void TearDown() override {}

 protected:
  std::unique_ptr<opengl::TimestampQueries> createTimestampQueries(uint32_t capacity) {
    auto queries = std::make_unique<opengl::TimestampQueries>(*context_, capacity);
    queries->beginQuery_ = [](GLenum, GLuint) {};
    queries->getQueryObjectiv_ = [](GLuint, GLenum, GLint* value) {
      ++availabilityReadCount;
      *value = available;
    };
    queries->getQueryObjectui64v_ = [](GLuint, GLenum, GLuint64* value) {
      ++resultReadCount;
      *value = 123;
    };
    return queries;
  }

  inline static uint32_t availabilityReadCount = 0;
  inline static uint32_t resultReadCount = 0;
  inline static GLint available = 1;
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  opengl::IContext* context_ = nullptr;
};

TEST_F(TimerOGLTest, UnbegunGapDoesNotReadQuery) {
  auto queries = createTimestampQueries(2);
  if (!queries->isValid()) {
    GTEST_SKIP() << "Timer query objects are unavailable";
  }
  queries->beginElapsedQuery(1);
  EXPECT_EQ(queries->count(), 2u);
  EXPECT_FALSE(queries->wasBegun(0));
  EXPECT_TRUE(queries->wasBegun(1));
  EXPECT_FALSE(queries->wasBegun(queries->capacity()));

  const auto result = queries->getElapsedNanosResult(0);
  EXPECT_FALSE(result.valid);
  EXPECT_EQ(result.elapsedNanos, 0u);
  EXPECT_EQ(queries->getElapsedNanos(0), 0u);
  EXPECT_EQ(availabilityReadCount, 0u);
  EXPECT_EQ(resultReadCount, 0u);
}

TEST_F(TimerOGLTest, ResetClearsBegunSlots) {
  auto queries = createTimestampQueries(3);
  if (!queries->isValid()) {
    GTEST_SKIP() << "Timer query objects are unavailable";
  }
  for (uint32_t slot = 0; slot < 3; ++slot) {
    queries->beginElapsedQuery(slot);
  }
  queries->reset();
  EXPECT_EQ(queries->count(), 0u);
  EXPECT_FALSE(queries->wasBegun(2));
  queries->beginElapsedQuery(2);
  EXPECT_EQ(queries->count(), 3u);

  for (uint32_t slot = 0; slot < 2; ++slot) {
    EXPECT_FALSE(queries->wasBegun(slot));
    EXPECT_FALSE(queries->getElapsedNanosResult(slot).valid);
    EXPECT_EQ(queries->getElapsedNanos(slot), 0u);
  }
  EXPECT_EQ(availabilityReadCount, 0u);
  EXPECT_EQ(resultReadCount, 0u);
}

TEST_F(TimerOGLTest, BegunSlotReadsQuery) {
  auto queries = createTimestampQueries(1);
  if (!queries->isValid()) {
    GTEST_SKIP() << "Timer query objects are unavailable";
  }
  queries->beginElapsedQuery(0);

  const auto result = queries->getElapsedNanosResult(0);
  EXPECT_TRUE(result.valid);
  EXPECT_EQ(result.elapsedNanos, 123u);
  EXPECT_EQ(queries->getElapsedNanos(0), 123u);
  EXPECT_TRUE(queries->resultsAvailable());
  EXPECT_EQ(availabilityReadCount, 3u);
  EXPECT_EQ(resultReadCount, 2u);
}

TEST_F(TimerOGLTest, BegunUnavailableSlotDoesNotReadResult) {
  auto queries = createTimestampQueries(1);
  if (!queries->isValid()) {
    GTEST_SKIP() << "Timer query objects are unavailable";
  }
  queries->beginElapsedQuery(0);
  available = 0;

  EXPECT_FALSE(queries->getElapsedNanosResult(0).valid);
  EXPECT_FALSE(queries->resultsAvailable());
  EXPECT_EQ(availabilityReadCount, 2u);
  EXPECT_EQ(resultReadCount, 0u);
}

TEST_F(TimerOGLTest, NoBegunSlotsHaveNoAvailableResults) {
  auto queries = createTimestampQueries(1);
  if (!queries->isValid()) {
    GTEST_SKIP() << "Timer query objects are unavailable";
  }

  EXPECT_FALSE(queries->resultsAvailable());
  EXPECT_FALSE(queries->getElapsedNanosResult(0).valid);
  EXPECT_EQ(queries->getElapsedNanos(0), 0u);
  EXPECT_EQ(availabilityReadCount, 0u);
  EXPECT_EQ(resultReadCount, 0u);
}

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
