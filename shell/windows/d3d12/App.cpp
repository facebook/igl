/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <memory>
#include <shell/windows/common/GlfwShell.h>
#include <shell/shared/platform/win/PlatformWin.h>
#include <igl/Core.h>
#include <igl/IGL.h>
#include <igl/d3d12/D3D12Context.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/PlatformDevice.h>

using namespace igl;
namespace igl::shell {
namespace {

class D3D12Shell final : public GlfwShell {
  SurfaceTextures createSurfaceTextures() noexcept final;
  std::shared_ptr<Platform> createPlatform() noexcept final;

  void willCreateWindow() noexcept final;
};

void D3D12Shell::willCreateWindow() noexcept {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

std::shared_ptr<Platform> D3D12Shell::createPlatform() noexcept {
  auto ctx = std::make_unique<d3d12::D3D12Context>();

  HWND hwnd = window() ? glfwGetWin32Window(window()) : nullptr;
  if (!hwnd) {
    IGL_LOG_ERROR("D3D12Shell: No valid window handle\n");
    return nullptr;
  }

  Result result = ctx->initialize(hwnd,
                                  static_cast<uint32_t>(shellParams().viewportSize.x),
                                  static_cast<uint32_t>(shellParams().viewportSize.y));
  if (!result.isOk()) {
    IGL_LOG_ERROR("D3D12Shell: Failed to initialize D3D12Context: %s\n", result.message.c_str());
    return nullptr;
  }

  auto device = std::make_unique<d3d12::Device>(std::move(ctx));
  return std::make_shared<PlatformWin>(std::move(device));
}

SurfaceTextures D3D12Shell::createSurfaceTextures() noexcept {
  IGL_PROFILER_FUNCTION();
  auto& device = platform().getDevice();
  const auto& d3d12PlatformDevice = device.getPlatformDevice<igl::d3d12::PlatformDevice>();

  IGL_DEBUG_ASSERT(d3d12PlatformDevice != nullptr);

  Result ret;
  auto color = d3d12PlatformDevice->createTextureFromNativeDrawable(&ret);
  IGL_DEBUG_ASSERT(ret.isOk());
  auto depth = d3d12PlatformDevice->createTextureFromNativeDepth(
      static_cast<uint32_t>(shellParams().viewportSize.x),
      static_cast<uint32_t>(shellParams().viewportSize.y),
      &ret);
  IGL_DEBUG_ASSERT(ret.isOk());

  return SurfaceTextures{std::move(color), std::move(depth)};
}

} // namespace
} // namespace igl::shell

int main(int argc, char* argv[]) {
  igl::shell::D3D12Shell shell;

  igl::shell::RenderSessionWindowConfig suggestedWindowConfig = {
      .width = 1024,
      .height = 768,
      .windowMode = igl::shell::WindowMode::MaximizedWindow,
  };
  igl::shell::RenderSessionConfig suggestedConfig = {
      .displayName = "Direct3D 12",
      .backendVersion = {.flavor = BackendFlavor::D3D12, .majorVersion = 12, .minorVersion = 0},
      .swapchainColorTextureFormat = TextureFormat::BGRA_UNorm8,
  };

  if (!shell.initialize(argc, argv, suggestedWindowConfig, suggestedConfig)) {
    shell.teardown();
    return -1;
  }

  shell.run();
  shell.teardown();

  return 0;
}
