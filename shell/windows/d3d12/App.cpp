/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <memory>
#include <shell/shared/platform/win/PlatformWin.h>
#include <shell/windows/common/GlfwShell.h>
#include <igl/Core.h>
#include <igl/IGL.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/D3D12Context.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

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
  // Get window handle
  HWND hwnd = window() ? glfwGetWin32Window(window()) : nullptr;

  // Create D3D12 context
  auto ctx = std::make_unique<d3d12::D3D12Context>();

  Result result = ctx->initialize(
      hwnd,
      (uint32_t)shellParams().viewportSize.x,
      (uint32_t)shellParams().viewportSize.y);

  if (!result.isOk()) {
    IGL_DEBUG_ABORT("Failed to initialize D3D12 context: %s", result.message.c_str());
  }

  // Create Device with D3D12Context
  auto device = std::make_unique<d3d12::Device>(std::move(ctx));

  return std::make_shared<PlatformWin>(std::move(device));
}

SurfaceTextures D3D12Shell::createSurfaceTextures() noexcept {
  IGL_PROFILER_FUNCTION();

  // For D3D12, we'll need to create textures from the swapchain back buffers
  // This is a simplified implementation - will need proper integration

  // For now, return empty surface textures
  // TODO: Implement proper D3D12 swapchain texture wrapping
  return SurfaceTextures{nullptr, nullptr};
}
} // namespace

} // namespace igl::shell

int main(int argc, char* argv[]) {
  igl::shell::D3D12Shell shell;

  igl::shell::RenderSessionWindowConfig suggestedWindowConfig = {
      .width = 1280,
      .height = 720,
      .windowMode = shell::WindowMode::Window,
  };
  igl::shell::RenderSessionConfig suggestedConfig = {
      .displayName = "DirectX 12",
      .backendVersion = {.flavor = BackendFlavor::D3D12, .majorVersion = 12, .minorVersion = 0},
      .swapchainColorTextureFormat = TextureFormat::RGBA_SRGB,
  };

  shell.initialize(argc, argv, suggestedWindowConfig, suggestedConfig);
  shell.run();

  return 0;
}
