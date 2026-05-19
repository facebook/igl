/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/FPSCounter.h>

namespace igl::tests {

TEST(FPSCounterTest, DefaultInitialFPS) {
  const FPSCounter counter(false);
  EXPECT_FLOAT_EQ(counter.getAverageFPS(), 72.0f);
}

TEST(FPSCounterTest, BelowThresholdNoUpdate) {
  FPSCounter counter(false);
  counter.updateFPS(0.5);
  EXPECT_FLOAT_EQ(counter.getAverageFPS(), 72.0f);
}

TEST(FPSCounterTest, AtThresholdUpdates) {
  FPSCounter counter(false);
  for (int i = 0; i < 60; ++i) {
    counter.updateFPS(1.0 / 60.0);
  }
  EXPECT_NEAR(counter.getAverageFPS(), 60.0f, 0.01f);
}

TEST(FPSCounterTest, MultipleIntervals) {
  FPSCounter counter(false);

  // Use exact binary fractions (1/N where N is a power of 2) so that
  // sequential addition accumulates to exactly 1.0 and reliably crosses
  // the threshold without floating-point round-off surprises.
  for (int i = 0; i < 128; ++i) {
    counter.updateFPS(1.0 / 128.0);
  }
  EXPECT_NEAR(counter.getAverageFPS(), 128.0f, 0.01f);

  for (int i = 0; i < 64; ++i) {
    counter.updateFPS(1.0 / 64.0);
  }
  EXPECT_NEAR(counter.getAverageFPS(), 64.0f, 0.01f);

  for (int i = 0; i < 32; ++i) {
    counter.updateFPS(1.0 / 32.0);
  }
  EXPECT_NEAR(counter.getAverageFPS(), 32.0f, 0.01f);
}

TEST(FPSCounterTest, CustomInterval) {
  FPSCounter counter(false, 2.0f);

  for (int i = 0; i < 3; ++i) {
    counter.updateFPS(0.5);
  }
  EXPECT_FLOAT_EQ(counter.getAverageFPS(), 72.0f);

  counter.updateFPS(0.5);
  EXPECT_NEAR(counter.getAverageFPS(), 2.0f, 0.01f);
}

TEST(FPSCounterTest, SingleFrameInterval) {
  FPSCounter counter(false);
  counter.updateFPS(2.0);
  EXPECT_FLOAT_EQ(counter.getAverageFPS(), 0.5f);
}

TEST(FPSCounterTest, AccumulatesAcrossMultipleSmallFrames) {
  FPSCounter counter(false);
  for (int i = 0; i < 999; ++i) {
    counter.updateFPS(0.001);
    EXPECT_FLOAT_EQ(counter.getAverageFPS(), 72.0f);
  }
  counter.updateFPS(0.001);
  EXPECT_NEAR(counter.getAverageFPS(), 1000.0f, 0.01f);
}

TEST(FPSCounterTest, NoPrintConstructor) {
  FPSCounter counter(false);
  EXPECT_FLOAT_EQ(counter.getAverageFPS(), 72.0f);
  for (int i = 0; i < 60; ++i) {
    counter.updateFPS(1.0 / 60.0);
  }
  EXPECT_NEAR(counter.getAverageFPS(), 60.0f, 0.01f);
}

} // namespace igl::tests
