/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdint>
#include <cstring>
#include <emscripten/emscripten.h>
#include <memory>
#include <vector>
#include <igl/tests/webgpu/TriangleRender.h>
#include <igl/webgpu/HWDevice.h>
#include <igl/webgpu/PlatformDevice.h>
#include <igl/webgpu/Readback.h>
#include <igl/webgpu/Surface.h>
#include <igl/webgpu/Texture.h>

// Exports of the TriangleWebGPU sample. Only igl_triangle() is a JSPI export (it may suspend);
// every other export runs on the page's imported device with suspension disallowed, so it never
// waits. tests/webgpu/wasm/run_triangle_node.mjs audits this split.

namespace {

struct ImportedDevice {
  std::unique_ptr<igl::webgpu::Device> device;
  igl::tests::webgpu::TriangleRenderer renderer;
  std::shared_ptr<igl::ITexture> target;
  igl::webgpu::AsyncTextureReadback readback;
  std::unique_ptr<igl::webgpu::Surface> surface;
  igl::tests::webgpu::TriangleRenderer surfaceRenderer;
};

std::unique_ptr<ImportedDevice>& getImported() {
  static std::unique_ptr<ImportedDevice> imported;
  return imported;
}

} // namespace

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

// Imports Module.iglWebGPUDevice and creates the triangle pipeline on it. Returns 0 on success.
extern "C" EMSCRIPTEN_KEEPALIVE int igl_import_device() {
  igl::Result ret;
  auto imported = std::make_unique<ImportedDevice>();
  imported->device = igl::webgpu::HWDevice::createWithJsDevice({}, &ret);
  if (!imported->device) {
    IGL_LOG_ERROR("createWithJsDevice(): %s\n", ret.message.c_str());
    return 1;
  }
  imported->device->getContext().setSuspensionAllowed(false);
  ret = imported->renderer.initialize(*imported->device, igl::TextureFormat::RGBA_UNorm8);
  if (!ret.isOk()) {
    IGL_LOG_ERROR("TriangleRenderer::initialize(): %s\n", ret.message.c_str());
    return 2;
  }
  getImported() = std::move(imported);
  return 0;
}

// Renders a size x size frame on the imported device and starts reading it back without waiting.
extern "C" EMSCRIPTEN_KEEPALIVE int igl_render_async(uint32_t size) {
  auto& imported = getImported();
  if (!imported) {
    return 1;
  }
  igl::Result ret;
  imported->target = imported->device->createTexture(
      igl::TextureDesc::new2D(igl::TextureFormat::RGBA_UNorm8,
                              size,
                              size,
                              igl::TextureDesc::TextureUsageBits::Attachment),
      &ret);
  if (!ret.isOk()) {
    return 2;
  }
  ret = imported->renderer.render(imported->target);
  if (!ret.isOk()) {
    return 3;
  }
  ret = static_cast<igl::IDevice&>(*imported->device)
            .getPlatformDevice<igl::webgpu::PlatformDevice>()
            ->readPixelsAsync(*imported->target,
                              igl::TextureRangeDesc::new2D(0, 0, size, size),
                              imported->readback,
                              /*flipVertically=*/false);
  return ret.isOk() ? 0 : 4;
}

// Returns 1 and copies the frame (top row first) to `out` once igl_render_async()'s readback has
// completed, 0 while it is in flight, and a negative value on errors.
extern "C" EMSCRIPTEN_KEEPALIVE int igl_poll_async(uint8_t* out) {
  auto& imported = getImported();
  if (!imported || !imported->readback.isPending()) {
    return -1;
  }
  if (!imported->readback.poll()) {
    return 0;
  }
  return imported->readback.copyTo(out).isOk() ? 1 : -2;
}

// Waits on the imported device while suspension is disallowed; returns 0 when every wait failed
// cleanly (the guard's debug abort only logs here).
extern "C" EMSCRIPTEN_KEEPALIVE int igl_wait_disallowed() {
  auto& imported = getImported();
  if (!imported) {
    return -1;
  }
  const bool debugBreak = igl::isDebugBreakEnabled();
  igl::setDebugBreakEnabled(false);
  const igl::Result waited = imported->device->getContext().waitForSubmittedWork();
  std::vector<uint8_t> texel(4);
  const auto& texture = static_cast<const igl::webgpu::Texture&>(*imported->target);
  const igl::Result read = igl::webgpu::readTexture(
      imported->device->getContext(),
      {.texture = texture.getWGPUTexture(), .width = 1, .height = 1, .bytesPerTexel = 4},
      texel.data());
  igl::setDebugBreakEnabled(debugBreak);
  return !waited.isOk() && !read.isOk() ? 0 : 1;
}

// Like igl_wait_disallowed(), but with suspension allowed: outside a JSPI export the wait traps
// with a WebAssembly.SuspendError, which is what the guard prevents.
extern "C" EMSCRIPTEN_KEEPALIVE int igl_wait_unguarded() {
  auto& imported = getImported();
  if (!imported) {
    return -1;
  }
  auto& ctx = imported->device->getContext();
  ctx.setSuspensionAllowed(true);
  const igl::Result waited = ctx.waitForSubmittedWork();
  ctx.setSuspensionAllowed(false);
  return waited.isOk() ? 0 : 1;
}

// Makes a WebGPU validation error on the imported device; its error scope does not wait, so this
// returns 0 and the error shows up later in igl_take_errors().
extern "C" EMSCRIPTEN_KEEPALIVE int igl_make_latched_error() {
  auto& imported = getImported();
  if (!imported) {
    return -1;
  }
  auto& ctx = imported->device->getContext();
  WGPUBufferDescriptor desc = WGPU_BUFFER_DESCRIPTOR_INIT;
  desc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_MapWrite;
  desc.size = 4;
  ctx.pushErrorScope(WGPUErrorFilter_Validation);
  const igl::webgpu::Handle<WGPUBuffer> buffer(wgpuDeviceCreateBuffer(ctx.getDevice(), &desc));
  return ctx.popErrorScope().isOk() ? 0 : 1;
}

// Returns the number of errors latched on the imported device since the last call.
extern "C" EMSCRIPTEN_KEEPALIVE int igl_take_errors() {
  auto& imported = getImported();
  if (!imported) {
    return -1;
  }
  return static_cast<int>(imported->device->getContext().takeErrors().size());
}

// Renders to the canvas matching `selector` from now on, at width x height in its preferred format.
extern "C" EMSCRIPTEN_KEEPALIVE int igl_canvas_init(const char* selector,
                                                    uint32_t width,
                                                    uint32_t height) {
  auto& imported = getImported();
  if (!imported) {
    return -1;
  }
  igl::Result ret;
  imported->surface = igl::webgpu::Surface::createFromCanvas(*imported->device, selector, &ret);
  if (!imported->surface) {
    IGL_LOG_ERROR("Surface::createFromCanvas(): %s\n", ret.message.c_str());
    return 1;
  }
  ret = imported->surface->configure(width, height);
  if (!ret.isOk()) {
    IGL_LOG_ERROR("Surface::configure(): %s\n", ret.message.c_str());
    return 2;
  }
  ret = imported->surfaceRenderer.initialize(*imported->device, imported->surface->getFormat());
  return ret.isOk() ? 0 : 3;
}

// Draws one frame into the canvas; the browser shows it when the calling task ends.
extern "C" EMSCRIPTEN_KEEPALIVE int igl_canvas_frame() {
  auto& imported = getImported();
  if (!imported || !imported->surface) {
    return -1;
  }
  igl::Result ret;
  const auto texture = imported->surface->getCurrentTexture(&ret);
  if (!texture) {
    return 1;
  }
  return imported->surfaceRenderer.render(texture).isOk() ? 0 : 2;
}
