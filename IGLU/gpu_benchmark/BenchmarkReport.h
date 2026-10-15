/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/gpu_benchmark/GpuFrameTimer.h>
#include <IGLU/gpu_benchmark/TrafficModel.h>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace iglu::gpu_benchmark {

/// All measurements of one variant of one scenario at one size.
struct VariantResult {
  std::string scenario;
  std::string size;
  std::string variant;
  /// How `gpuNanos` was measured.
  TimerKind timerKind = TimerKind::GpuTimer;
  /// One entry per timed command buffer, in nanoseconds.
  std::vector<double> gpuNanos;
  /// Median of each round, in round order, which is what drift and the ratio to the baseline are
  /// judged on. NaN marks a round without a measurement.
  std::vector<double> roundMedianNanos;
  /// Per frame, that is per timed command buffer.
  TrafficEstimate traffic;
  FootprintEstimate footprint;
  std::string note;
};

/// Collects the results of a benchmark run and reports them as a table and as CSV rows.
///
/// Every CSV row starts with the prefix given to the constructor, for example "TILEBENCH_V1", so
/// that rows can be picked out of a log. Nothing is quoted: commas in free text become ';', double
/// quotes become single quotes and line breaks become spaces. Byte counts are integers and other
/// numbers are fixed-point. The rows are:
///  - `<prefix>,meta,<key>,<value>`
///  - `<prefix>,summary,<scenario>,<size>,<variant>,<n>,<timer>,<median_ns>,<p10_ns>,<p90_ns>,
///    <mad_ns>,<ratio_vs_baseline>,<model_read_bytes>,<model_write_bytes>,<model_gbps>,
///    <footprint_bytes>,<memoryless_backed>,<drift_per_round>,<note>`
///  - `<prefix>,sample,<scenario>,<size>,<variant>,<index>,<gpu_ns>`
///  - `<prefix>,phase,<begin|end>,<scenario>,<size>,<variant>,<seconds>,<iterations>`
///
/// The columns of a summary row come from a result as follows:
///  - `n` is the number of samples; `median_ns`, `p10_ns`, `p90_ns` and `mad_ns` are the
///    statistics of `gpuNanos`, see Summary
///  - `timer` is `gpu` for TimerKind::GpuTimer and `cpu` for TimerKind::CpuWallClock
///  - `ratio_vs_baseline` is the paired estimator against the baseline variant of the same
///    scenario and size: the median of the per-round ratios of `roundMedianNanos`, see
///    medianOfRatios(), when both have the same non-zero number of rounds. Otherwise it is the
///    median divided by the median of the baseline. It is empty without a baseline, or when there
///    is nothing to compare
///  - `model_gbps` is the modeled bytes per frame divided by `median_ns`, which is GB/s. It is
///    empty when the median is 0
///  - `memoryless_backed` is 1 if the footprint counts memoryless textures as regular memory
///  - `drift_per_round` is the relative drift of `roundMedianNanos`, see relativeDriftPerStep()
///
/// Rows are in the order of the meta values and results added. With `includeSamples`, the sample
/// rows of a result follow its summary row.
class BenchmarkReport {
 public:
  explicit BenchmarkReport(std::string csvPrefix);

  void addMeta(const std::string& key, const std::string& value);

  void addResult(VariantResult result);

  [[nodiscard]] std::vector<std::string> csvRows(const std::string& baselineVariant,
                                                 bool includeSamples = false) const;

  /// Aligned columns, one row per result, grouped by scenario and size: median in ms, p10-p90 in
  /// ms, ratio to the baseline variant as in csvRows(), modeled MB per frame and footprint in MB
  /// (10^6 bytes). A footprint marked with '*' counts memoryless textures as regular memory. One
  /// line per '\n'. Empty without results.
  [[nodiscard]] std::string table(const std::string& baselineVariant) const;

  /// Writes table() and csvRows() line by line with IGLLog() at IGLLogInfo, which unlike
  /// IGL_LOG_INFO() is not compiled out of optimized builds.
  void log(const std::string& baselineVariant, bool includeSamples = false) const;

  /// Row that marks the beginning or the end of a phase of the run, for tools that correlate the
  /// log with other traces. `seconds` and `iterations` are reported as given.
  [[nodiscard]] static std::string phaseRow(const std::string& csvPrefix,
                                            const std::string& beginOrEnd,
                                            const std::string& scenario,
                                            const std::string& size,
                                            const std::string& variant,
                                            double seconds,
                                            uint64_t iterations);

 private:
  std::string csvPrefix_;
  std::vector<std::pair<std::string, std::string>> meta_;
  std::vector<VariantResult> results_;
};

} // namespace iglu::gpu_benchmark
