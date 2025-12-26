/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

/* Minimal D3D12 test device factory using a headless context. */

#include "TestDevice.h"

#include <igl/IGL.h>
#include <igl/Log.h>
#include <igl/d3d12/D3D12Headers.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/HeadlessContext.h>

namespace igl::tests::util::device::d3d12 {

std::unique_ptr<igl::d3d12::Device> createTestDevice(bool enableDebugLayer) {
  IGL_LOG_INFO("[Tests] D3D12 test device requested (debug layer: %s)\n",
               enableDebugLayer ? "enabled" : "disabled");

  // Enabling the debug layer happens inside D3D12Context::createDevice() when available.
  // Build a headless context (no swapchain) suitable for unit tests.
  auto ctx = std::make_unique<igl::d3d12::HeadlessD3D12Context>();
  auto res = ctx->initializeHeadless(256, 256);
  if (res.code != Result::Code::Ok) {
    IGL_LOG_ERROR("[Tests] D3D12 headless context init failed: %s\n", res.message.c_str());
    return nullptr;
  }
  return std::make_unique<igl::d3d12::Device>(std::move(ctx));
}

} // namespace igl::tests::util::device::d3d12
