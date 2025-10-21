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
#include <igl/d3d12/Texture.h>

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

  auto& device = static_cast<d3d12::Device&>(platform().getDevice());
  auto& ctx = device.getD3D12Context();

  // Get current back buffer from swapchain
  uint32_t backBufferIndex = ctx.getCurrentBackBufferIndex();
  ID3D12Resource* backBuffer = ctx.getCurrentBackBuffer();

  if (!backBuffer) {
    IGL_LOG_ERROR("Failed to get back buffer from swapchain\n");
    return SurfaceTextures{nullptr, nullptr};
  }

  // Create color texture from swapchain back buffer
  TextureDesc colorDesc;
  colorDesc.type = TextureType::TwoD;
  colorDesc.format = TextureFormat::RGBA_SRGB;  // Matches swapchain format
  colorDesc.width = shellParams().viewportSize.x;
  colorDesc.height = shellParams().viewportSize.y;
  colorDesc.depth = 1;
  colorDesc.numLayers = 1;
  colorDesc.numSamples = 1;
  colorDesc.numMipLevels = 1;
  colorDesc.usage = TextureDesc::TextureUsageBits::Attachment;
  colorDesc.debugName = "Swapchain Back Buffer";

  auto color = d3d12::Texture::createFromResource(backBuffer, colorDesc.format, colorDesc,
                                                   ctx.getDevice(), ctx.getCommandQueue());

  // Create depth texture
  TextureDesc depthDesc;
  depthDesc.type = TextureType::TwoD;
  depthDesc.format = TextureFormat::Z_UNorm32;  // 32-bit depth
  depthDesc.width = shellParams().viewportSize.x;
  depthDesc.height = shellParams().viewportSize.y;
  depthDesc.depth = 1;
  depthDesc.numLayers = 1;
  depthDesc.numSamples = 1;
  depthDesc.numMipLevels = 1;
  depthDesc.usage = TextureDesc::TextureUsageBits::Attachment;
  depthDesc.debugName = "Depth Buffer";

  Result depthResult;
  auto depth = device.createTexture(depthDesc, &depthResult);

  if (!depthResult.isOk()) {
    IGL_LOG_ERROR("Failed to create depth texture: %s\n", depthResult.message.c_str());
    return SurfaceTextures{nullptr, nullptr};
  }

  return SurfaceTextures{std::move(color), std::move(depth)};
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

  if (!shell.initialize(argc, argv, suggestedWindowConfig, suggestedConfig)) {
    IGL_LOG_ERROR("Failed to initialize shell\n");
    return 1;
  }

  shell.run();
  shell.teardown();

  return 0;
}
