/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/metal/TimestampQueries.h>

#import <Metal/MTLCounters.h>
#include <initializer_list>
#include <memory>

namespace igl::metal {

class TimestampQueriesTest : public ::testing::Test {
 protected:
  static std::unique_ptr<TimestampQueries> makeResolved(
      std::initializer_list<uint64_t> timestamps) {
    auto queries = std::make_unique<TimestampQueries>(nil, timestamps.size() / 2);
    queries->resolvedTimestamps_.assign(timestamps);
    queries->resolved_.store(true, std::memory_order_release);
    return queries;
  }
};

TEST_F(TimestampQueriesTest, StartCounterErrorInvalidatesTimingSlot) {
  auto queries = makeResolved({MTLCounterErrorValue, 100});

  const auto result = queries->getElapsedNanosResult(0);

  EXPECT_FALSE(result.valid);
  EXPECT_EQ(queries->getElapsedNanos(0), 0u);
  EXPECT_EQ(queries->getStartNanos(0), 0u);
  EXPECT_EQ(queries->getEndNanos(0), 0u);
  EXPECT_EQ(queries->getFrameElapsedNanos(), 0u);
}

TEST_F(TimestampQueriesTest, EndCounterErrorInvalidatesTimingSlot) {
  auto queries = makeResolved({100, MTLCounterErrorValue});

  const auto result = queries->getElapsedNanosResult(0);

  EXPECT_FALSE(result.valid);
  EXPECT_EQ(queries->getElapsedNanos(0), 0u);
  EXPECT_EQ(queries->getStartNanos(0), 0u);
  EXPECT_EQ(queries->getEndNanos(0), 0u);
  EXPECT_EQ(queries->getFrameElapsedNanos(), 0u);
}

TEST_F(TimestampQueriesTest, PositiveElapsedDurationIsValid) {
  auto queries = makeResolved({100, 175});

  const auto result = queries->getElapsedNanosResult(0);

  EXPECT_TRUE(result.valid);
  EXPECT_EQ(result.elapsedNanos, 75u);
  EXPECT_EQ(queries->getElapsedNanos(0), 75u);
  EXPECT_EQ(queries->getStartNanos(0), 100u);
  EXPECT_EQ(queries->getEndNanos(0), 175u);
  EXPECT_EQ(queries->getFrameElapsedNanos(), 75u);
}

TEST_F(TimestampQueriesTest, NonIncreasingEndIsValidZero) {
  auto queries = makeResolved({200, 100});

  const auto result = queries->getElapsedNanosResult(0);

  EXPECT_TRUE(result.valid);
  EXPECT_EQ(result.elapsedNanos, 0u);
  EXPECT_EQ(queries->getElapsedNanos(0), 0u);
  EXPECT_EQ(queries->getStartNanos(0), 200u);
  EXPECT_EQ(queries->getEndNanos(0), 100u);
  EXPECT_EQ(queries->getFrameElapsedNanos(), 0u);
}

TEST_F(TimestampQueriesTest, UnresolvedAndOutOfBoundsSlotsAreInvalid) {
  auto unresolved = std::make_unique<TimestampQueries>(nil, 1);
  EXPECT_FALSE(unresolved->getElapsedNanosResult(0).valid);
  EXPECT_EQ(unresolved->getElapsedNanos(0), 0u);
  EXPECT_EQ(unresolved->getStartNanos(0), 0u);
  EXPECT_EQ(unresolved->getEndNanos(0), 0u);
  EXPECT_EQ(unresolved->getFrameElapsedNanos(), 0u);

  auto resolved = makeResolved({100, 200});
  EXPECT_FALSE(resolved->getElapsedNanosResult(1).valid);
  EXPECT_EQ(resolved->getElapsedNanos(1), 0u);
  EXPECT_EQ(resolved->getStartNanos(1), 0u);
  EXPECT_EQ(resolved->getEndNanos(1), 0u);
}

TEST_F(TimestampQueriesTest, FrameElapsedUsesWallClockSpanAcrossSlots) {
  auto queries = makeResolved({100, 300, 200, 500});

  EXPECT_EQ(queries->getElapsedNanos(0), 200u);
  EXPECT_EQ(queries->getElapsedNanos(1), 300u);
  EXPECT_EQ(queries->getFrameElapsedNanos(), 400u);
}

} // namespace igl::metal
