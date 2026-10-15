/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <IGLU/gpu_benchmark/Statistics.h>

#include <cmath>
#include <limits>

namespace iglu::gpu_benchmark::tests {

TEST(StatisticsTest, Empty) {
  const Summary s = summarize({});
  EXPECT_EQ(s.count, 0u);
  EXPECT_EQ(s.median, 0.0);
  EXPECT_EQ(s.p10, 0.0);
  EXPECT_EQ(s.p90, 0.0);
  EXPECT_EQ(s.mean, 0.0);
  EXPECT_EQ(s.mad, 0.0);
  EXPECT_EQ(s.min, 0.0);
  EXPECT_EQ(s.max, 0.0);
}

TEST(StatisticsTest, OddCountUnsorted) {
  const Summary s = summarize({5, 1, 3, 2, 4});
  EXPECT_EQ(s.count, 5u);
  EXPECT_DOUBLE_EQ(s.median, 3.0);
  EXPECT_NEAR(s.p10, 1.4, 1e-12);
  EXPECT_NEAR(s.p90, 4.6, 1e-12);
  EXPECT_DOUBLE_EQ(s.mean, 3.0);
  EXPECT_DOUBLE_EQ(s.mad, 1.0);
  EXPECT_DOUBLE_EQ(s.min, 1.0);
  EXPECT_DOUBLE_EQ(s.max, 5.0);
}

TEST(StatisticsTest, EvenCount) {
  const Summary s = summarize({4, 1, 3, 2});
  EXPECT_DOUBLE_EQ(s.median, 2.5);
  EXPECT_DOUBLE_EQ(s.mad, 1.0);
  EXPECT_DOUBLE_EQ(s.mean, 2.5);
}

TEST(StatisticsTest, SingleAndConstant) {
  const Summary one = summarize({7});
  EXPECT_EQ(one.count, 1u);
  EXPECT_DOUBLE_EQ(one.median, 7.0);
  EXPECT_DOUBLE_EQ(one.p10, 7.0);
  EXPECT_DOUBLE_EQ(one.p90, 7.0);
  EXPECT_DOUBLE_EQ(one.mad, 0.0);
  const Summary flat = summarize({5, 5, 5, 5});
  EXPECT_EQ(flat.median, 5.0);
  EXPECT_EQ(flat.p10, 5.0);
  EXPECT_EQ(flat.p90, 5.0);
  EXPECT_EQ(flat.mad, 0.0);
}

TEST(StatisticsTest, Percentile) {
  EXPECT_EQ(percentile({}, 0.5), 0.0);
  EXPECT_DOUBLE_EQ(percentile({10, 20}, 0.5), 15.0);
  EXPECT_DOUBLE_EQ(percentile({1, 2, 3}, 0.0), 1.0);
  EXPECT_DOUBLE_EQ(percentile({1, 2, 3}, 1.0), 3.0);
  EXPECT_DOUBLE_EQ(percentile({1, 2, 3}, -1.0), 1.0);
  EXPECT_DOUBLE_EQ(percentile({1, 2, 3}, 2.0), 3.0);
  EXPECT_DOUBLE_EQ(percentile({1, 2, 3}, std::nan("")), 1.0);
  EXPECT_DOUBLE_EQ(percentile({0, 100}, 0.25), 25.0);
}

TEST(StatisticsTest, MedianOfRatios) {
  EXPECT_DOUBLE_EQ(medianOfRatios({2, 4, 6}, {1, 2, 3}), 2.0);
  EXPECT_DOUBLE_EQ(medianOfRatios({1, 2, 3}, {0, 1, 0}), 2.0);
  EXPECT_EQ(medianOfRatios({1, 2}, {0, 0}), 0.0);
  EXPECT_EQ(medianOfRatios({}, {}), 0.0);
  EXPECT_DOUBLE_EQ(medianOfRatios({2, 4}, {1, 2, 3}), 2.0);
  EXPECT_DOUBLE_EQ(medianOfRatios({1, 3}, {1, 1}), 2.0);
}

TEST(StatisticsTest, Drift) {
  EXPECT_NEAR(relativeDriftPerStep({10, 11, 12, 13}), 1.0 / 11.5, 1e-12);
  EXPECT_NEAR(relativeDriftPerStep({13, 12, 11, 10}), -1.0 / 11.5, 1e-12);
  EXPECT_EQ(relativeDriftPerStep({5, 5, 5}), 0.0);
  EXPECT_EQ(relativeDriftPerStep({}), 0.0);
  EXPECT_EQ(relativeDriftPerStep({5}), 0.0);
  EXPECT_EQ(relativeDriftPerStep({-1, 1}), 0.0);
  EXPECT_NEAR(relativeDriftPerStep({100, 101}), 1.0 / 100.5, 1e-12);
}

TEST(StatisticsTest, SummarizeIgnoresNonFinite) {
  const double nan = std::nan("");
  const double inf = std::numeric_limits<double>::infinity();
  const Summary s = summarize({5, nan, 1, inf, 3, -inf, 2, 4});
  EXPECT_EQ(s.count, 5u);
  EXPECT_DOUBLE_EQ(s.median, 3.0);
  EXPECT_DOUBLE_EQ(s.mean, 3.0);
  EXPECT_DOUBLE_EQ(s.min, 1.0);
  EXPECT_DOUBLE_EQ(s.max, 5.0);
  const Summary none = summarize({nan, inf});
  EXPECT_EQ(none.count, 0u);
  EXPECT_EQ(none.median, 0.0);
  EXPECT_EQ(none.max, 0.0);
}

TEST(StatisticsTest, MedianOfRatiosSkipsNonFinite) {
  const double nan = std::nan("");
  const double inf = std::numeric_limits<double>::infinity();
  EXPECT_DOUBLE_EQ(medianOfRatios({nan, 4, 6}, {1, 2, 3}), 2.0);
  EXPECT_DOUBLE_EQ(medianOfRatios({2, 4, 6}, {1, nan, 3}), 2.0);
  EXPECT_DOUBLE_EQ(medianOfRatios({2, inf, 6}, {1, 2, 3}), 2.0);
  EXPECT_DOUBLE_EQ(medianOfRatios({2, 4, 6}, {1, inf, 3}), 2.0);
  EXPECT_EQ(medianOfRatios({nan, nan}, {1, 1}), 0.0);
  EXPECT_EQ(medianOfRatios({1, 1}, {nan, nan}), 0.0);
  EXPECT_EQ(medianOfRatios({1e300}, {1e-300}), 0.0);
}

TEST(StatisticsTest, DriftSkipsNonFinite) {
  const double nan = std::nan("");
  EXPECT_NEAR(relativeDriftPerStep({10, nan, 12, 13}), 0.0, 1.0);
  const double expected = [] {
    const double xs[] = {0, 2, 3};
    const double ys[] = {10, 12, 13};
    const double meanX = (0 + 2 + 3) / 3.0;
    const double meanY = (10 + 12 + 13) / 3.0;
    double cov = 0;
    double var = 0;
    for (int i = 0; i < 3; ++i) {
      cov += (xs[i] - meanX) * (ys[i] - meanY);
      var += (xs[i] - meanX) * (xs[i] - meanX);
    }
    return cov / var / meanY;
  }();
  EXPECT_NEAR(relativeDriftPerStep({10, nan, 12, 13}), expected, 1e-12);
  EXPECT_EQ(relativeDriftPerStep({nan, 5, nan}), 0.0);
  EXPECT_EQ(relativeDriftPerStep({nan, nan}), 0.0);
  EXPECT_NEAR(relativeDriftPerStep({10, 11, 12, 13}), 1.0 / 11.5, 1e-12);
}

} // namespace iglu::gpu_benchmark::tests
