/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

/*
 * Custom test entrypoint: initialize COM for D3D12 before running gtest.
 */

#include <gtest/gtest.h>

#include <csignal>
#include <cstdio>
// COM initialization is only needed for D3D12 tests
// IGL_D3D12_TEST is defined only in iglD3D12Tests build
#if defined(_WIN32) && defined(IGL_D3D12_TEST)
#include <combaseapi.h>
#include <windows.h>
#endif

static void signalHandler(int signum) {
  std::printf("CRASH: Signal %d caught\n", signum);
  std::_Exit(signum);
}

int main(int argc, char** argv) {
  // Install basic signal handler for early crash diagnostics
  std::signal(SIGSEGV, signalHandler);

  // Initialize COM in multithreaded mode for D3D12 usage (Windows only)
#if defined(_WIN32) && defined(IGL_D3D12_TEST)
  const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(hr)) {
    std::printf("COM initialization failed: 0x%08X\n", static_cast<unsigned>(hr));
    return 1;
  }
#endif

  ::testing::InitGoogleTest(&argc, argv);
  const int result = RUN_ALL_TESTS();

#if defined(_WIN32) && defined(IGL_D3D12_TEST)
  CoUninitialize();
#endif
  return result;
}
