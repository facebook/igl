/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/gpu_benchmark/BenchmarkReport.h>

#include <IGLU/gpu_benchmark/Statistics.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <numeric>
#include <string_view>
#include <igl/Common.h> // IWYU pragma: keep

namespace iglu::gpu_benchmark {

namespace {

constexpr int kNanosDecimals = 1;
constexpr int kRatioDecimals = 4;
constexpr int kRateDecimals = 3;
constexpr int kDriftDecimals = 6;
constexpr int kSecondsDecimals = 6;
constexpr int kMillisecondsDecimals = 3;
constexpr int kMegabytesDecimals = 2;
constexpr double kNanosPerMillisecond = 1.0e6;
constexpr double kBytesPerMegabyte = 1.0e6;

[[nodiscard]] std::string formatFixed(double value, int decimals) {
  const int length = std::snprintf(nullptr, 0, "%.*f", decimals, value);
  if (length <= 0) {
    return {};
  }
  std::string text(static_cast<size_t>(length), '\0');
  std::snprintf(text.data(), text.size() + 1, "%.*f", decimals, value);
  return text;
}

[[nodiscard]] std::string sanitize(std::string_view text) {
  std::string clean(text);
  for (char& c : clean) {
    if (c == ',') {
      c = ';';
    } else if (c == '"') {
      c = '\'';
    } else if (c == '\n' || c == '\r') {
      c = ' ';
    }
  }
  return clean;
}

[[nodiscard]] std::string makeRow(const std::vector<std::string>& fields) {
  std::string row;
  for (size_t i = 0; i < fields.size(); ++i) {
    if (i > 0) {
      row += ',';
    }
    row += sanitize(fields[i]);
  }
  return row;
}

[[nodiscard]] std::vector<Summary> summarizeAll(const std::vector<VariantResult>& results) {
  std::vector<Summary> summaries;
  summaries.reserve(results.size());
  for (const VariantResult& result : results) {
    summaries.push_back(summarize(result.gpuNanos));
  }
  return summaries;
}

// Index of the `baselineVariant` result with the scenario and size of `result`, or results.size().
[[nodiscard]] size_t findBaseline(const std::vector<VariantResult>& results,
                                  const VariantResult& result,
                                  const std::string& baselineVariant) {
  if (!baselineVariant.empty()) {
    for (size_t i = 0; i < results.size(); ++i) {
      if (results[i].scenario == result.scenario && results[i].size == result.size &&
          results[i].variant == baselineVariant) {
        return i;
      }
    }
  }
  return results.size();
}

// Paired estimator: the median of the per-round ratios when both variants have the same non-zero
// number of rounds and some round has a finite ratio, else the ratio of the overall medians. Empty
// if there is no baseline or nothing to compare.
[[nodiscard]] std::string ratioVsBaseline(const std::vector<VariantResult>& results,
                                          const std::vector<Summary>& summaries,
                                          size_t index,
                                          const std::string& baselineVariant) {
  const size_t baselineIndex = findBaseline(results, results[index], baselineVariant);
  if (baselineIndex == results.size()) {
    return {};
  }
  const std::vector<double>& rounds = results[index].roundMedianNanos;
  if (!rounds.empty() && rounds.size() == results[baselineIndex].roundMedianNanos.size()) {
    const double paired = medianOfRatios(rounds, results[baselineIndex].roundMedianNanos);
    if (paired > 0.0) {
      return formatFixed(paired, kRatioDecimals);
    }
  }
  const double median = summaries[index].median;
  const double baselineMedian = summaries[baselineIndex].median;
  if (median <= 0.0 || baselineMedian <= 0.0) {
    return {};
  }
  return formatFixed(median / baselineMedian, kRatioDecimals);
}

constexpr size_t kTableColumnCount = 8;
constexpr size_t kTableTextColumnCount = 3;
using TableRow = std::array<std::string, kTableColumnCount>;

// Indices of `results` grouped by scenario and size, in order of first appearance.
[[nodiscard]] std::vector<size_t> groupedOrder(const std::vector<VariantResult>& results) {
  std::vector<std::pair<std::string_view, std::string_view>> groups;
  std::vector<size_t> groupOfResult;
  groupOfResult.reserve(results.size());
  for (const VariantResult& result : results) {
    const std::pair<std::string_view, std::string_view> group{result.scenario, result.size};
    auto it = std::find(groups.begin(), groups.end(), group);
    if (it == groups.end()) {
      it = groups.insert(groups.end(), group);
    }
    groupOfResult.push_back(static_cast<size_t>(it - groups.begin()));
  }

  std::vector<size_t> order(results.size());
  std::iota(order.begin(), order.end(), size_t{0});
  std::stable_sort(order.begin(), order.end(), [&groupOfResult](size_t lhs, size_t rhs) {
    return groupOfResult[lhs] < groupOfResult[rhs];
  });
  return order;
}

// One line per row, each column padded to its widest cell: text columns on the left, the number
// columns on the right.
[[nodiscard]] std::string alignRows(const std::vector<TableRow>& rows) {
  std::array<size_t, kTableColumnCount> widths{};
  for (const TableRow& row : rows) {
    for (size_t column = 0; column < kTableColumnCount; ++column) {
      widths[column] = std::max(widths[column], row[column].size());
    }
  }

  std::string text;
  for (const TableRow& row : rows) {
    for (size_t column = 0; column < kTableColumnCount; ++column) {
      if (column > 0) {
        text += "  ";
      }
      const size_t padding = widths[column] - row[column].size();
      if (column < kTableTextColumnCount) {
        text += row[column];
        text.append(padding, ' ');
      } else {
        text.append(padding, ' ');
        text += row[column];
      }
    }
    text += '\n';
  }
  return text;
}

} // namespace

BenchmarkReport::BenchmarkReport(std::string csvPrefix) : csvPrefix_(std::move(csvPrefix)) {}

void BenchmarkReport::addMeta(const std::string& key, const std::string& value) {
  meta_.emplace_back(key, value);
}

void BenchmarkReport::addResult(VariantResult result) {
  results_.push_back(std::move(result));
}

std::vector<std::string> BenchmarkReport::csvRows(const std::string& baselineVariant,
                                                  bool includeSamples) const {
  size_t rowCount = meta_.size() + results_.size();
  if (includeSamples) {
    for (const VariantResult& result : results_) {
      rowCount += result.gpuNanos.size();
    }
  }
  std::vector<std::string> rows;
  rows.reserve(rowCount);
  for (const auto& [key, value] : meta_) {
    rows.push_back(makeRow({csvPrefix_, "meta", key, value}));
  }

  const std::vector<Summary> summaries = summarizeAll(results_);
  for (size_t i = 0; i < results_.size(); ++i) {
    const VariantResult& result = results_[i];
    const Summary& summary = summaries[i];
    const std::string gbps =
        summary.median > 0.0
            ? formatFixed(static_cast<double>(result.traffic.totalBytes()) / summary.median,
                          kRateDecimals)
            : std::string{};

    rows.push_back(makeRow({
        csvPrefix_,
        "summary",
        result.scenario,
        result.size,
        result.variant,
        std::to_string(summary.count),
        result.timerKind == TimerKind::GpuTimer ? "gpu" : "cpu",
        formatFixed(summary.median, kNanosDecimals),
        formatFixed(summary.p10, kNanosDecimals),
        formatFixed(summary.p90, kNanosDecimals),
        formatFixed(summary.mad, kNanosDecimals),
        ratioVsBaseline(results_, summaries, i, baselineVariant),
        std::to_string(result.traffic.readBytes),
        std::to_string(result.traffic.writeBytes),
        gbps,
        std::to_string(result.footprint.bytes),
        result.footprint.memorylessBacked ? "1" : "0",
        formatFixed(relativeDriftPerStep(result.roundMedianNanos), kDriftDecimals),
        result.note,
    }));

    if (includeSamples) {
      for (size_t sample = 0; sample < result.gpuNanos.size(); ++sample) {
        rows.push_back(makeRow({
            csvPrefix_,
            "sample",
            result.scenario,
            result.size,
            result.variant,
            std::to_string(sample),
            formatFixed(result.gpuNanos[sample], kNanosDecimals),
        }));
      }
    }
  }
  return rows;
}

std::string BenchmarkReport::table(const std::string& baselineVariant) const {
  if (results_.empty()) {
    return {};
  }
  const std::vector<Summary> summaries = summarizeAll(results_);

  std::vector<TableRow> rows;
  rows.reserve(results_.size() + 1);
  rows.push_back({"scenario",
                  "size",
                  "variant",
                  "median ms",
                  "p10-p90 ms",
                  "ratio",
                  "model MB",
                  "footprint MB"});
  bool anyMemorylessBacked = false;
  for (const size_t i : groupedOrder(results_)) {
    const VariantResult& result = results_[i];
    const Summary& summary = summaries[i];
    const std::string ratio = ratioVsBaseline(results_, summaries, i, baselineVariant);
    anyMemorylessBacked = anyMemorylessBacked || result.footprint.memorylessBacked;
    rows.push_back({
        result.scenario,
        result.size,
        result.variant,
        formatFixed(summary.median / kNanosPerMillisecond, kMillisecondsDecimals),
        formatFixed(summary.p10 / kNanosPerMillisecond, kMillisecondsDecimals) + "-" +
            formatFixed(summary.p90 / kNanosPerMillisecond, kMillisecondsDecimals),
        ratio.empty() ? "-" : ratio,
        formatFixed(static_cast<double>(result.traffic.totalBytes()) / kBytesPerMegabyte,
                    kMegabytesDecimals),
        formatFixed(static_cast<double>(result.footprint.bytes) / kBytesPerMegabyte,
                    kMegabytesDecimals) +
            (result.footprint.memorylessBacked ? "*" : ""),
    });
  }

  std::string text = alignRows(rows);
  if (anyMemorylessBacked) {
    text += "* memoryless textures are backed by regular memory on this device\n";
  }
  return text;
}

void BenchmarkReport::log(const std::string& baselineVariant, bool includeSamples) const {
  const std::string tableText = table(baselineVariant);
  std::string_view remaining = tableText;
  while (!remaining.empty()) {
    const size_t newline = remaining.find('\n');
    const std::string line(remaining.substr(0, newline));
    IGLLog(IGLLogInfo, "%s\n", line.c_str());
    remaining = newline == std::string_view::npos ? std::string_view{}
                                                  : remaining.substr(newline + 1);
  }
  for (const std::string& row : csvRows(baselineVariant, includeSamples)) {
    IGLLog(IGLLogInfo, "%s\n", row.c_str());
  }
}

std::string BenchmarkReport::phaseRow(const std::string& csvPrefix,
                                      const std::string& beginOrEnd,
                                      const std::string& scenario,
                                      const std::string& size,
                                      const std::string& variant,
                                      double seconds,
                                      uint64_t iterations) {
  return makeRow({
      csvPrefix,
      "phase",
      beginOrEnd,
      scenario,
      size,
      variant,
      formatFixed(seconds, kSecondsDecimals),
      std::to_string(iterations),
  });
}

} // namespace iglu::gpu_benchmark
