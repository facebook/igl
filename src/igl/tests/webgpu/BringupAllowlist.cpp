/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <string_view>

// Some runners (the XCTest bridge for Apple tests) enumerate every registered test and ignore
// --gtest_filter, so the allowlist skips tests from a listener instead. A test skipped here still
// constructs and destroys its fixture, but never runs SetUp().

#if defined(IGL_WEBGPU_BRINGUP_ALLOWLIST)

namespace igl::tests::webgpu {

namespace {

bool matches(std::string_view pattern, std::string_view suite, std::string_view name) {
  const size_t dot = pattern.find('.');
  if (dot == std::string_view::npos || pattern.substr(0, dot) != suite) {
    return false;
  }
  const std::string_view test = pattern.substr(dot + 1);
  return test == "*" || test == name;
}

bool isAllowed(const ::testing::TestInfo& info) {
  const std::string_view file = info.file() != nullptr ? info.file() : "";
  if (file.find("tests/webgpu/") != std::string_view::npos) {
    return true;
  }
  std::string_view list = IGL_WEBGPU_BRINGUP_ALLOWLIST;
  while (!list.empty()) {
    const size_t colon = list.find(':');
    if (matches(list.substr(0, colon), info.test_suite_name(), info.name())) {
      return true;
    }
    if (colon == std::string_view::npos) {
      break;
    }
    list.remove_prefix(colon + 1);
  }
  return false;
}

class BringupAllowlist final : public ::testing::EmptyTestEventListener {
 public:
  void OnTestStart(const ::testing::TestInfo& info) override {
    if (!isAllowed(info)) {
      GTEST_SKIP() << "Not in the WebGPU bring-up allowlist (tests/webgpu/bringup.bzl)";
    }
  }
};

// NOLINTNEXTLINE(clang-diagnostic-global-constructors)
const bool kBringupAllowlistInstalled = [] {
  // gtest takes ownership of the listener.
  ::testing::UnitTest::GetInstance()->listeners().Append(new BringupAllowlist());
  return true;
}();

} // namespace

} // namespace igl::tests::webgpu

#endif // defined(IGL_WEBGPU_BRINGUP_ALLOWLIST)
