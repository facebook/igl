/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <IGLU/gpu_benchmark/Schedule.h>

#include <algorithm>
#include <map>
#include <set>
#include <vector>

namespace iglu::gpu_benchmark::tests {

namespace {

std::vector<std::vector<uint32_t>> ordersOf(const std::vector<ScheduledBlock>& schedule,
                                            uint32_t numVariants) {
  std::vector<std::vector<uint32_t>> orders;
  for (size_t i = 0; i < schedule.size(); i += numVariants) {
    std::vector<uint32_t> order;
    for (uint32_t k = 0; k < numVariants; ++k) {
      EXPECT_EQ(schedule[i + k].round, schedule[i].round);
      order.push_back(schedule[i + k].variant);
    }
    orders.push_back(order);
  }
  return orders;
}

void expectBalanced(const std::vector<std::vector<uint32_t>>& orders,
                    size_t first,
                    size_t count,
                    uint32_t n,
                    uint32_t timesEach) {
  std::vector<uint32_t> pairs(static_cast<size_t>(n) * n, 0);
  std::vector<uint32_t> positions(static_cast<size_t>(n) * n, 0);
  for (size_t r = first; r < first + count; ++r) {
    std::vector<uint32_t> sorted = orders[r];
    std::sort(sorted.begin(), sorted.end());
    for (uint32_t v = 0; v < n; ++v) {
      ASSERT_EQ(sorted[v], v) << "round " << r << " is not a permutation";
    }
    for (uint32_t k = 0; k < n; ++k) {
      ++positions[orders[r][k] * n + k];
      if (k + 1 < n) {
        ++pairs[orders[r][k] * n + orders[r][k + 1]];
      }
    }
  }
  for (uint32_t a = 0; a < n; ++a) {
    for (uint32_t b = 0; b < n; ++b) {
      EXPECT_EQ(positions[a * n + b], static_cast<uint32_t>(count / n))
          << "variant " << a << " position " << b;
      if (a != b) {
        EXPECT_EQ(pairs[a * n + b], timesEach) << "pair " << a << "," << b;
      }
    }
  }
}

} // namespace

TEST(ScheduleTest, Rotation) {
  const uint32_t numVariants = 3;
  const uint32_t warmupRounds = 1;
  const uint32_t rounds = 2;
  const std::vector<ScheduledBlock> schedule =
      makeInterleavedSchedule(numVariants, warmupRounds, rounds);
  ASSERT_EQ(schedule.size(), (warmupRounds + rounds) * numVariants);

  for (size_t blockIdx = 0; blockIdx < schedule.size(); ++blockIdx) {
    const ScheduledBlock& block = schedule[blockIdx];
    EXPECT_LT(block.variant, numVariants) << blockIdx;
    EXPECT_LT(block.round, warmupRounds + rounds) << blockIdx;
    EXPECT_EQ(block.warmup, block.round < warmupRounds) << blockIdx;
  }

  std::map<uint32_t, std::vector<uint32_t>> roundVariants;
  for (const ScheduledBlock& block : schedule) {
    roundVariants[block.round].push_back(block.variant);
  }

  for (const auto& [round, variants] : roundVariants) {
    EXPECT_EQ(variants.size(), numVariants) << "round " << round;
    std::set<uint32_t> uniqueVariants(variants.begin(), variants.end());
    EXPECT_EQ(uniqueVariants.size(), numVariants) << "round " << round << " has duplicates";
  }

  const size_t warmupBlocks = warmupRounds * numVariants;
  for (size_t i = 0; i < warmupBlocks; ++i) {
    EXPECT_TRUE(schedule[i].warmup) << "block " << i << " should be warmup";
  }
  for (size_t i = warmupBlocks; i < schedule.size(); ++i) {
    EXPECT_FALSE(schedule[i].warmup) << "block " << i << " should not be warmup";
  }
}

TEST(ScheduleTest, Degenerate) {
  EXPECT_TRUE(makeInterleavedSchedule(0, 2, 3).empty());
  EXPECT_TRUE(makeInterleavedSchedule(3, 0, 0).empty());
  const auto one = makeInterleavedSchedule(1, 0, 3);
  ASSERT_EQ(one.size(), 3u);
  for (const ScheduledBlock& block : one) {
    EXPECT_EQ(block.variant, 0u);
    EXPECT_FALSE(block.warmup);
  }
  const auto warmupOnly = makeInterleavedSchedule(2, 2, 0);
  ASSERT_EQ(warmupOnly.size(), 4u);
  for (const ScheduledBlock& block : warmupOnly) {
    EXPECT_TRUE(block.warmup);
  }
}

TEST(ScheduleTest, EvenIsWilliamsSquare) {
  const auto four = ordersOf(makeInterleavedSchedule(4, 0, 4), 4);
  ASSERT_EQ(four.size(), 4u);
  EXPECT_EQ(four[0], (std::vector<uint32_t>{0, 1, 3, 2}));
  EXPECT_EQ(four[1], (std::vector<uint32_t>{1, 2, 0, 3}));
  EXPECT_EQ(four[2], (std::vector<uint32_t>{2, 3, 1, 0}));
  EXPECT_EQ(four[3], (std::vector<uint32_t>{3, 0, 2, 1}));
  for (const uint32_t n : {2u, 4u, 6u, 8u}) {
    for (const uint32_t warmup : {0u, 1u, 3u}) {
      const auto orders = ordersOf(makeInterleavedSchedule(n, warmup, 2 * n), n);
      ASSERT_EQ(orders.size(), warmup + 2 * n);
      expectBalanced(orders, warmup, n, n, 1);
      expectBalanced(orders, warmup + 1, n, n, 1);
      expectBalanced(orders, warmup, 2 * n, n, 2);
    }
  }
}

TEST(ScheduleTest, TwoVariantsAlternate) {
  const auto orders = ordersOf(makeInterleavedSchedule(2, 2, 6), 2);
  ASSERT_EQ(orders.size(), 8u);
  for (size_t r = 0; r < orders.size(); ++r) {
    EXPECT_EQ(orders[r], r % 2 == 0 ? (std::vector<uint32_t>{0, 1}) : (std::vector<uint32_t>{1, 0}))
        << r;
  }
}

TEST(ScheduleTest, OddUsesSquareAndMirror) {
  const auto three = ordersOf(makeInterleavedSchedule(3, 0, 6), 3);
  ASSERT_EQ(three.size(), 6u);
  EXPECT_EQ(three[0], (std::vector<uint32_t>{0, 1, 2}));
  EXPECT_EQ(three[1], (std::vector<uint32_t>{2, 1, 0}));
  EXPECT_EQ(three[2], (std::vector<uint32_t>{1, 2, 0}));
  EXPECT_EQ(three[3], (std::vector<uint32_t>{0, 2, 1}));
  EXPECT_EQ(three[4], (std::vector<uint32_t>{2, 0, 1}));
  EXPECT_EQ(three[5], (std::vector<uint32_t>{1, 0, 2}));
  for (const uint32_t n : {1u, 3u, 5u, 7u, 9u}) {
    for (const uint32_t warmup : {0u, 1u, 2u, 5u}) {
      const auto orders = ordersOf(makeInterleavedSchedule(n, warmup, 2 * n), n);
      ASSERT_EQ(orders.size(), warmup + 2 * n);
      if (n > 1) {
        expectBalanced(orders, warmup, 2 * n, n, 2);
      }
    }
  }
}

TEST(ScheduleTest, WarmupMarking) {
  const auto schedule = makeInterleavedSchedule(3, 2, 4);
  ASSERT_EQ(schedule.size(), 18u);
  for (const ScheduledBlock& block : schedule) {
    EXPECT_EQ(block.warmup, block.round < 2);
    EXPECT_LT(block.variant, 3u);
  }
}

} // namespace iglu::gpu_benchmark::tests
