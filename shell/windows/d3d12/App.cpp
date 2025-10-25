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
#include <igl/d3d12/HeadlessContext.h>
#include <igl/d3d12/PlatformDevice.h>
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

  // Offscreen fallback when windowed swapchain fails
  bool offscreen_ = false;
  std::shared_ptr<ITexture> fallbackColor_;
  std::shared_ptr<ITexture> fallbackDepth_;
};

void D3D12Shell::willCreateWindow() noexcept {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

std::shared_ptr<Platform> D3D12Shell::createPlatform() noexcept {
  // Get window handle
  HWND hwnd = window() ? glfwGetWin32Window(window()) : nullptr;

  // Create D3D12 context
  std::unique_ptr<d3d12::D3D12Context> ctx;
  Result result;
  {
    auto winCtx = std::make_unique<d3d12::D3D12Context>();
    result = winCtx->initialize(
        hwnd,
        (uint32_t)shellParams().viewportSize.x,
        (uint32_t)shellParams().viewportSize.y);
    if (result.isOk()) {
      ctx = std::move(winCtx);
    } else {
      IGL_LOG_ERROR("D3D12 windowed init failed: %s. Falling back to headless offscreen.\n", result.message.c_str());
      auto headless = std::make_unique<d3d12::HeadlessD3D12Context>();
      Result r2 = headless->initializeHeadless((uint32_t)shellParams().viewportSize.x,
                                               (uint32_t)shellParams().viewportSize.y);
      if (!r2.isOk()) {
        IGL_DEBUG_ABORT("Failed to initialize headless D3D12 context: %s", r2.message.c_str());
      }
      ctx = std::move(headless);
      offscreen_ = true;
    }
  }

  // Create Device with selected context
  auto device = std::make_unique<d3d12::Device>(std::move(ctx));

  return std::make_shared<PlatformWin>(std::move(device));
}

SurfaceTextures D3D12Shell::createSurfaceTextures() noexcept {
  IGL_PROFILER_FUNCTION();

  static int callCount = 0;
  if (callCount < 3) {
    IGL_LOG_INFO("createSurfaceTextures() called #%d\n", ++callCount);
  }

  auto& dev = platform().getDevice();

  if (!offscreen_) {
    const auto& d3dPlatformDevice = dev.getPlatformDevice<d3d12::PlatformDevice>();
    IGL_DEBUG_ASSERT(d3dPlatformDevice != nullptr);

    Result ret;
    auto color = d3dPlatformDevice->createTextureFromNativeDrawable(&ret);
    IGL_DEBUG_ASSERT(ret.isOk());
    auto depth = d3dPlatformDevice->createTextureFromNativeDepth(
        shellParams().viewportSize.x, shellParams().viewportSize.y, &ret);
    IGL_DEBUG_ASSERT(ret.isOk());
    return SurfaceTextures{color, depth};
  }

  // Offscreen fallback path: create or resize cached textures
  const uint32_t w = (uint32_t)shellParams().viewportSize.x;
  const uint32_t h = (uint32_t)shellParams().viewportSize.y;
  if (!fallbackColor_ || fallbackColor_->getDimensions().width != w || fallbackColor_->getDimensions().height != h) {
    // Use BGRA_SRGB format to match Vulkan swapchain format for exact parity
    TextureDesc colorDesc = TextureDesc::new2D(TextureFormat::BGRA_SRGB,
                                               w, h,
                                               TextureDesc::TextureUsageBits::Attachment |
                                                 TextureDesc::TextureUsageBits::Sampled,
                                               "Offscreen Color");
    Result colorResult;
    fallbackColor_ = dev.createTexture(colorDesc, &colorResult);
    if (!fallbackColor_) {
      IGL_LOG_ERROR("Failed to create offscreen color texture: %s\n", colorResult.message.c_str());
    } else {
      IGL_LOG_INFO("Created offscreen color texture: %ux%u format=%d\n", w, h, (int)TextureFormat::BGRA_SRGB);
    }
  }
  if (!fallbackDepth_ || fallbackDepth_->getDimensions().width != w || fallbackDepth_->getDimensions().height != h) {
    TextureDesc depthDesc = TextureDesc::new2D(TextureFormat::Z_UNorm32,
                                               w, h,
                                               TextureDesc::TextureUsageBits::Attachment,
                                               "Offscreen Depth");
    Result depthResult;
    fallbackDepth_ = dev.createTexture(depthDesc, &depthResult);
    if (!fallbackDepth_) {
      IGL_LOG_ERROR("Failed to create offscreen depth texture: %s\n", depthResult.message.c_str());
    } else {
      IGL_LOG_INFO("Created offscreen depth texture: %ux%u format=%d\n", w, h, (int)TextureFormat::Z_UNorm32);
    }
  }

  if (!fallbackColor_ || !fallbackDepth_) {
    IGL_LOG_ERROR("createSurfaceTextures(): Returning NULL textures! color=%p depth=%p\n",
                  fallbackColor_.get(), fallbackDepth_.get());
  }

  return SurfaceTextures{fallbackColor_, fallbackDepth_};
}
} // namespace

} // namespace igl::shell

int main(int argc, char* argv[]) {
  IGL_LOG_INFO("=== TinyMeshSession D3D12 Starting ===\n");

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

  IGL_LOG_INFO("main(): Calling shell.initialize()...\n");
  if (!shell.initialize(argc, argv, suggestedWindowConfig, suggestedConfig)) {
    IGL_LOG_ERROR("Failed to initialize shell\n");
    return 1;
  }

  IGL_LOG_INFO("main(): Initialization successful, calling shell.run()...\n");
  shell.run();

  IGL_LOG_INFO("main(): shell.run() completed, calling teardown()...\n");
  shell.teardown();

  IGL_LOG_INFO("=== TinyMeshSession D3D12 Exiting ===\n");
  return 0;
}
