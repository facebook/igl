/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/gpu_benchmark/Statistics.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace iglu::gpu_benchmark {

Summary summarize(std::vector<double> samples) {
  std::erase_if(samples, [](double sample) { return !std::isfinite(sample); });
  if (samples.empty()) {
    return {};
  }
  std::sort(samples.begin(), samples.end());

  Summary summary{
      .count = samples.size(),
      .median = percentile(samples, 0.5),
      .p10 = percentile(samples, 0.1),
      .p90 = percentile(samples, 0.9),
      .mean = std::accumulate(samples.begin(), samples.end(), 0.0) /
              static_cast<double>(samples.size()),
      .min = samples.front(),
      .max = samples.back(),
  };

  std::vector<double> deviations;
  deviations.reserve(samples.size());
  for (const double sample : samples) {
    deviations.push_back(std::abs(sample - summary.median));
  }
  std::sort(deviations.begin(), deviations.end());
  summary.mad = percentile(deviations, 0.5);
  return summary;
}

double percentile(const std::vector<double>& sortedSamples, double fraction) {
  if (sortedSamples.empty()) {
    return 0.0;
  }
  const double clampedFraction = std::isnan(fraction) ? 0.0 : std::clamp(fraction, 0.0, 1.0);
  const double rank = clampedFraction * static_cast<double>(sortedSamples.size() - 1);
  const size_t lower = static_cast<size_t>(rank);
  const size_t upper = std::min(lower + 1, sortedSamples.size() - 1);
  const double weight = rank - static_cast<double>(lower);
  return sortedSamples[lower] + (sortedSamples[upper] - sortedSamples[lower]) * weight;
}

double medianOfRatios(const std::vector<double>& numerators,
                      const std::vector<double>& denominators) {
  const size_t pairCount = std::min(numerators.size(), denominators.size());
  std::vector<double> ratios;
  ratios.reserve(pairCount);
  for (size_t i = 0; i < pairCount; ++i) {
    if (!std::isfinite(numerators[i]) || !std::isfinite(denominators[i]) ||
        denominators[i] == 0.0) {
      continue;
    }
    const double ratio = numerators[i] / denominators[i];
    if (std::isfinite(ratio)) {
      ratios.push_back(ratio);
    }
  }
  std::sort(ratios.begin(), ratios.end());
  return percentile(ratios, 0.5);
}

double relativeDriftPerStep(const std::vector<double>& valuesInOrder) {
  size_t finiteCount = 0;
  double sum = 0.0;
  double stepSum = 0.0;
  for (size_t i = 0; i < valuesInOrder.size(); ++i) {
    if (std::isfinite(valuesInOrder[i])) {
      ++finiteCount;
      sum += valuesInOrder[i];
      stepSum += static_cast<double>(i);
    }
  }
  if (finiteCount < 2) {
    return 0.0;
  }
  const double count = static_cast<double>(finiteCount);
  const double mean = sum / count;
  if (mean == 0.0) {
    return 0.0;
  }

  const double meanStep = stepSum / count;
  double covariance = 0.0;
  double stepVariance = 0.0;
  for (size_t i = 0; i < valuesInOrder.size(); ++i) {
    if (std::isfinite(valuesInOrder[i])) {
      const double stepOffset = static_cast<double>(i) - meanStep;
      covariance += stepOffset * (valuesInOrder[i] - mean);
      stepVariance += stepOffset * stepOffset;
    }
  }
  return covariance / stepVariance / mean;
}

} // namespace iglu::gpu_benchmark
