/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace iglu::gpu_benchmark {

/// Benchmark parameters from two sources: the shell's `BenchmarkRenderSessionParams::customParams`
/// and command line tokens such as those `convertArgvToParams()` returns.
///
/// `args` accept `--key value`, `--key=value`, and a bare `--key` that is followed by another `--`
/// token or by nothing, which has the value "1". Only keys that start with the prefix once their
/// leading dashes are stripped are kept, for both sources. The prefix should include its
/// separator, "tile-" rather than "tile", or "tilex" matches too. A key is stored and looked up
/// without the dashes but with the prefix, for example "tile-rounds" for `--tile-rounds`. A key
/// present in both sources takes its value from `args`.
///
/// Nothing here throws: a value that does not parse yields the default given by the caller. That,
/// a clamped value and a malformed size are logged as warnings.
class ParamMap {
 public:
  ParamMap(const std::vector<std::pair<std::string, std::string>>& customParams,
           const std::vector<std::string>& args,
           const std::string& prefix);

  [[nodiscard]] bool has(const std::string& key) const;

  [[nodiscard]] std::string getString(const std::string& key,
                                      const std::string& defaultValue) const;

  [[nodiscard]] int64_t getInt(const std::string& key, int64_t defaultValue) const;

  /// Finite values only.
  [[nodiscard]] double getDouble(const std::string& key, double defaultValue) const;

  /// "1", "true", "yes" and "on" are true; "0", "false", "no" and "off" are false, in any case. A
  /// flag with no value is true.
  [[nodiscard]] bool getBool(const std::string& key, bool defaultValue) const;

  /// Whole numbers in [`minValue`, `maxValue`]: a value outside the range is clamped to it.
  /// `defaultValue` is returned as it is when the key is missing or its value does not parse,
  /// even if it is outside the range.
  [[nodiscard]] uint32_t getUint(const std::string& key,
                                 uint32_t defaultValue,
                                 uint32_t minValue,
                                 uint32_t maxValue) const;

  /// Comma-separated items with surrounding whitespace trimmed and empty items dropped. The
  /// default is returned if no item remains.
  [[nodiscard]] std::vector<std::string> getList(
      const std::string& key,
      const std::vector<std::string>& defaultValue) const;

  /// Comma-separated `<width>x<height>` items, for example "1920x1080,3840x2160". Items that are
  /// malformed or have a zero extent are skipped. The default is returned if no item remains.
  [[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>> getSizes(
      const std::string& key,
      const std::vector<std::pair<uint32_t, uint32_t>>& defaultValue) const;

 private:
  std::unordered_map<std::string, std::string> values_;
};

} // namespace iglu::gpu_benchmark
