/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/gpu_benchmark/ParamMap.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <string_view>
#include <system_error>

namespace iglu::gpu_benchmark {

namespace {

constexpr std::string_view kWhitespace = " \t\r\n";

[[nodiscard]] std::string_view trim(std::string_view text) {
  const size_t first = text.find_first_not_of(kWhitespace);
  if (first == std::string_view::npos) {
    return {};
  }
  const size_t last = text.find_last_not_of(kWhitespace);
  return text.substr(first, last - first + 1);
}

[[nodiscard]] std::string_view stripLeadingDashes(std::string_view key) {
  const size_t first = key.find_first_not_of('-');
  return first == std::string_view::npos ? std::string_view{} : key.substr(first);
}

[[nodiscard]] std::string toLowerAscii(std::string_view text) {
  std::string lower(text);
  for (char& c : lower) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return lower;
}

template<typename Integer>
[[nodiscard]] std::optional<Integer> parseInteger(std::string_view text) {
  const std::string_view trimmed = trim(text);
  if (trimmed.empty()) {
    return std::nullopt;
  }
  const char* const end = trimmed.data() + trimmed.size();
  Integer value{};
  // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
  const std::from_chars_result result = std::from_chars(trimmed.data(), end, value);
  if (result.ec != std::errc{} || result.ptr != end) {
    return std::nullopt;
  }
  return value;
}

[[nodiscard]] std::vector<std::string> splitList(std::string_view text) {
  std::vector<std::string> items;
  size_t begin = 0;
  while (begin <= text.size()) {
    const size_t comma = text.find(',', begin);
    const size_t end = comma == std::string_view::npos ? text.size() : comma;
    const std::string_view item = trim(text.substr(begin, end - begin));
    if (!item.empty()) {
      items.emplace_back(item);
    }
    begin = end + 1;
  }
  return items;
}

[[nodiscard]] std::optional<std::pair<uint32_t, uint32_t>> parseSize(std::string_view item) {
  const size_t separator = item.find_first_of("xX");
  if (separator == std::string_view::npos) {
    return std::nullopt;
  }
  const std::optional<uint32_t> width = parseInteger<uint32_t>(item.substr(0, separator));
  const std::optional<uint32_t> height = parseInteger<uint32_t>(item.substr(separator + 1));
  if (!width || !height || *width == 0 || *height == 0) {
    return std::nullopt;
  }
  return std::pair<uint32_t, uint32_t>{*width, *height};
}

} // namespace

ParamMap::ParamMap(const std::vector<std::pair<std::string, std::string>>& customParams,
                   const std::vector<std::string>& args,
                   const std::string& prefix) {
  for (const auto& [key, value] : customParams) {
    const std::string_view name = stripLeadingDashes(key);
    if (!name.empty() && name.starts_with(prefix)) {
      values_[std::string(name)] = value;
    }
  }

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string_view arg = args[i];
    if (!arg.starts_with("--")) {
      continue;
    }
    const std::string_view body = stripLeadingDashes(arg);
    const size_t equals = body.find('=');
    const std::string_view name = body.substr(0, equals);
    if (name.empty() || !name.starts_with(prefix)) {
      continue;
    }
    if (equals != std::string_view::npos) {
      values_[std::string(name)] = std::string(body.substr(equals + 1));
    } else if (i + 1 < args.size() && !args[i + 1].starts_with("--")) {
      values_[std::string(name)] = args[++i];
    } else {
      values_[std::string(name)] = "1";
    }
  }
}

bool ParamMap::has(const std::string& key) const {
  return values_.find(key) != values_.end();
}

std::string ParamMap::getString(const std::string& key, const std::string& defaultValue) const {
  const auto it = values_.find(key);
  return it != values_.end() ? it->second : defaultValue;
}

int64_t ParamMap::getInt(const std::string& key, int64_t defaultValue) const {
  const auto it = values_.find(key);
  if (it == values_.end()) {
    return defaultValue;
  }
  return parseInteger<int64_t>(it->second).value_or(defaultValue);
}

uint32_t ParamMap::getUint(const std::string& key,
                           uint32_t defaultValue,
                           uint32_t minValue,
                           uint32_t maxValue) const {
  const auto it = values_.find(key);
  if (it == values_.end()) {
    return defaultValue;
  }
  const std::optional<int64_t> value = parseInteger<int64_t>(it->second);
  if (!value) {
    return defaultValue;
  }
  const int64_t lower = minValue;
  const int64_t upper = std::max(lower, static_cast<int64_t>(maxValue));
  return static_cast<uint32_t>(std::clamp(*value, lower, upper));
}

double ParamMap::getDouble(const std::string& key, double defaultValue) const {
  const auto it = values_.find(key);
  if (it == values_.end()) {
    return defaultValue;
  }
  const char* const begin = it->second.c_str();
  char* end = nullptr;
  const double value = std::strtod(begin, &end);
  if (end == begin || !std::isfinite(value) || !trim(end).empty()) {
    return defaultValue;
  }
  return value;
}

bool ParamMap::getBool(const std::string& key, bool defaultValue) const {
  const auto it = values_.find(key);
  if (it == values_.end()) {
    return defaultValue;
  }
  const std::string text = toLowerAscii(trim(it->second));
  if (text.empty() || text == "1" || text == "true" || text == "yes" || text == "on") {
    return true;
  }
  if (text == "0" || text == "false" || text == "no" || text == "off") {
    return false;
  }
  return defaultValue;
}

std::vector<std::string> ParamMap::getList(const std::string& key,
                                           const std::vector<std::string>& defaultValue) const {
  const auto it = values_.find(key);
  if (it == values_.end()) {
    return defaultValue;
  }
  std::vector<std::string> items = splitList(it->second);
  if (items.empty()) {
    return defaultValue;
  }
  return items;
}

std::vector<std::pair<uint32_t, uint32_t>> ParamMap::getSizes(
    const std::string& key,
    const std::vector<std::pair<uint32_t, uint32_t>>& defaultValue) const {
  const auto it = values_.find(key);
  if (it == values_.end()) {
    return defaultValue;
  }
  std::vector<std::pair<uint32_t, uint32_t>> sizes;
  for (const std::string& item : splitList(it->second)) {
    if (const std::optional<std::pair<uint32_t, uint32_t>> size = parseSize(item)) {
      sizes.push_back(*size);
    }
  }
  if (sizes.empty()) {
    return defaultValue;
  }
  return sizes;
}

} // namespace iglu::gpu_benchmark
