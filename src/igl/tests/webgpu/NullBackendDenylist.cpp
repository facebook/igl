/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Compiled into every WebGPU test binary; active only in iglWebGPUTestsNull, which defines
// IGL_WEBGPU_NULL_DENYLIST from tests/webgpu/null_denylist.bzl. The XCTest bridge ignores
// --gtest_filter, so denylisted tests are skipped from a listener.

#if defined(IGL_WEBGPU_NULL_DENYLIST)

#include <gtest/gtest.h>

#include <cstdlib>
#include <string_view>
#include <igl/tests/util/device/webgpu/TestDevice.h>

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

bool isDenied(std::string_view suite, std::string_view name) {
  std::string_view list = IGL_WEBGPU_NULL_DENYLIST;
  while (!list.empty()) {
    const size_t colon = list.find(':');
    if (matches(list.substr(0, colon), suite, name)) {
      return true;
    }
    if (colon == std::string_view::npos) {
      break;
    }
    list.remove_prefix(colon + 1);
  }
  return false;
}

class NullBackendDenylist final : public ::testing::EmptyTestEventListener {
 public:
  void OnTestStart(const ::testing::TestInfo& info) override {
    if (isDenied(info.test_suite_name(), info.name())) {
      GTEST_SKIP() << "Reads GPU results, which Dawn's Null backend does not produce "
                      "(tests/webgpu/null_denylist.bzl)";
    }
  }
};

// NOLINTNEXTLINE(clang-diagnostic-global-constructors)
const bool kNullBackendDenylistInstalled = [] {
  // Every WebGPU context in this binary resolves its adapter from IGL_WEBGPU_BACKEND. The
  // compile-time IGL_WEBGPU_DEFAULT_BACKEND would apply to the backend library, which this binary
  // shares with iglWebGPUTests. An explicit IGL_WEBGPU_BACKEND in the environment still wins.
  // NOLINTNEXTLINE(concurrency-mt-unsafe)
  setenv("IGL_WEBGPU_BACKEND", "null", /*overwrite=*/0);
  // gtest takes ownership of the listener.
  ::testing::UnitTest::GetInstance()->listeners().Append(new NullBackendDenylist());
  return true;
}();

} // namespace

TEST(WebGPUNullBackendTest, TestDevicesUseTheNullAdapter) {
  auto device = util::device::webgpu::createTestDevice();
  ASSERT_NE(device, nullptr);
  EXPECT_EQ(device->getContext().getBackendType(), WGPUBackendType_Null);
}

} // namespace igl::tests::webgpu

#endif // defined(IGL_WEBGPU_NULL_DENYLIST)
