/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <vector>

namespace iglu::gpu_benchmark {

/// One block of work of a benchmark run: `variant` runs during `round`.
struct ScheduledBlock {
  uint32_t round = 0;
  uint32_t variant = 0;
  /// A warm-up block runs but its measurements are discarded.
  bool warmup = false;
};

/// Schedules `numVariants` variants over `warmupRounds + rounds` rounds, the first `warmupRounds`
/// of them marked as warm-up.
///
/// The order within a round follows a Williams design, so that no variant always runs first or
/// last and the variant that ran just before has a balanced effect: each variant follows each
/// other variant equally often. Let n be `numVariants` and w be 0, 1, n - 1, 2, n - 2, ... For an
/// even n, round r runs the variants in the order `(w[k] + r) % n` for k in [0, n), which is the
/// Williams square. For an odd n, the rounds alternate between the square and its mirror image,
/// the same orders reversed: round r runs `(w[k] + r / 2) % n` for even r and
/// `(w[n - 1 - k] + r / 2) % n` for odd r. Two variants alternate their order every round.
///
/// For full balance, make `rounds` a multiple of `numVariants` (even) or of `2 * numVariants`
/// (odd). Empty if `numVariants` is 0.
[[nodiscard]] std::vector<ScheduledBlock> makeInterleavedSchedule(uint32_t numVariants,
                                                                  uint32_t warmupRounds,
                                                                  uint32_t rounds);

} // namespace iglu::gpu_benchmark
