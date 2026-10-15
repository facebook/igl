/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/gpu_benchmark/Schedule.h>

namespace iglu::gpu_benchmark {

namespace {

// 0, 1, n - 1, 2, n - 2, ...: the differences between neighbors are all distinct modulo n.
[[nodiscard]] std::vector<uint32_t> williamsBaseOrder(uint32_t numVariants) {
  std::vector<uint32_t> order(numVariants);
  for (uint32_t k = 1; k < numVariants; ++k) {
    order[k] = k % 2 == 1 ? (k + 1) / 2 : numVariants - k / 2;
  }
  return order;
}

} // namespace

std::vector<ScheduledBlock> makeInterleavedSchedule(uint32_t numVariants,
                                                    uint32_t warmupRounds,
                                                    uint32_t rounds) {
  std::vector<ScheduledBlock> schedule;
  if (numVariants == 0) {
    return schedule;
  }

  const std::vector<uint32_t> baseOrder = williamsBaseOrder(numVariants);
  const bool hasMirrorRounds = numVariants % 2 == 1;
  const uint64_t totalRounds = uint64_t{warmupRounds} + rounds;
  schedule.reserve(static_cast<size_t>(totalRounds * numVariants));
  for (uint64_t round = 0; round < totalRounds; ++round) {
    const bool mirrored = hasMirrorRounds && round % 2 == 1;
    const uint64_t shift = hasMirrorRounds ? round / 2 : round;
    for (uint32_t k = 0; k < numVariants; ++k) {
      const uint32_t base = baseOrder[mirrored ? numVariants - 1 - k : k];
      schedule.push_back({
          .round = static_cast<uint32_t>(round),
          .variant = static_cast<uint32_t>((base + shift) % numVariants),
          .warmup = round < warmupRounds,
      });
    }
  }
  return schedule;
}

} // namespace iglu::gpu_benchmark
