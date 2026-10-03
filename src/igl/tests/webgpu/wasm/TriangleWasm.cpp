/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdint>
#include <cstring>
#include <emscripten/emscripten.h>
#include <vector>
#include <igl/tests/webgpu/TriangleRender.h>
#include <igl/webgpu/HWDevice.h>

// Renders the reference triangle through IGL's WebGPU backend on `navigator.gpu` and copies the
// size * size * 4 RGBA bytes (top row first) to `out`. Returns 0 on success. Waits suspend the
// caller, so this export must be called through JSPI.
extern "C" EMSCRIPTEN_KEEPALIVE int igl_triangle(uint32_t size, uint8_t* out) {
  igl::Result ret;
  auto ctx = igl::webgpu::HWDevice::createContext({}, &ret);
  if (!ctx) {
    IGL_LOG_ERROR("createContext(): %s\n", ret.message.c_str());
    return 1;
  }
  const auto devices = igl::webgpu::HWDevice::queryDevices(
      *ctx, igl::HWDeviceQueryDesc(igl::HWDeviceType::Unknown), &ret);
  if (devices.empty()) {
    IGL_LOG_ERROR("queryDevices(): %s\n", ret.message.c_str());
    return 2;
  }
  auto device = igl::webgpu::HWDevice::create(std::move(ctx), devices[0], &ret);
  if (!device) {
    IGL_LOG_ERROR("create(): %s\n", ret.message.c_str());
    return 3;
  }
  std::vector<uint8_t> rgba;
  ret = igl::tests::webgpu::renderTriangle(*device, size, rgba);
  if (!ret.isOk()) {
    IGL_LOG_ERROR("renderTriangle(): %s\n", ret.message.c_str());
    return 4;
  }
  std::memcpy(out, rgba.data(), rgba.size());
  return 0;
}
