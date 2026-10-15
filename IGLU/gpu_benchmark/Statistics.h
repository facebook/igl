/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <vector>

namespace iglu::gpu_benchmark {

struct Summary {
  size_t count = 0;
  double median = 0.0;
  double p10 = 0.0;
  double p90 = 0.0;
  double mean = 0.0;
  /// Median absolute deviation from the median, unscaled.
  double mad = 0.0;
  double min = 0.0;
  double max = 0.0;
};

/// Summarizes `samples`, ignoring NaN and infinite ones. `count` is the number of samples used. All
/// fields are 0 if there are none.
[[nodiscard]] Summary summarize(std::vector<double> samples);

/// Value at `fraction` in [0, 1] of `sortedSamples`, which must be sorted ascending. Interpolates
/// linearly between the closest ranks. 0 for an empty vector.
[[nodiscard]] double percentile(const std::vector<double>& sortedSamples, double fraction);

/// Median of the element-wise ratios `numerators[i] / denominators[i]`. Skips the pairs with a zero
/// denominator or a NaN or infinite value, so a caller can mark a missing round with NaN, and any
/// elements past the shorter vector. 0 if no ratio remains.
[[nodiscard]] double medianOfRatios(const std::vector<double>& numerators,
                                    const std::vector<double>& denominators);

/// Least-squares slope of `valuesInOrder` per step, divided by their mean: 0.01 means +1% per step.
/// NaN and infinite values are skipped, and the others keep their positions. 0 for fewer than 2
/// values or a zero mean.
[[nodiscard]] double relativeDriftPerStep(const std::vector<double>& valuesInOrder);

} // namespace iglu::gpu_benchmark
