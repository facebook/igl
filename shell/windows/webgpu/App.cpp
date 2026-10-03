/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <memory>
#include <shell/windows/common/GlfwShell.h>
#include <vector>
#include <shell/shared/platform/win/PlatformWin.h>
#include <igl/webgpu/Device.h>
#include <igl/webgpu/HWDevice.h>
#include <igl/webgpu/Surface.h>

using namespace igl;
namespace igl::shell {
namespace {

/// Dawn in a GLFW window (a WGPUSurface on the native window), or offscreen with --headless.
class WebGPUShell final : public GlfwShell {
 public:
  // GlfwShell::teardown() destroys the device; textures must not outlive its context.
  void releaseResources() noexcept;

 private:
  SurfaceTextures createSurfaceTextures() noexcept final;
  std::shared_ptr<Platform> createPlatform() noexcept final;

  void willCreateWindow() noexcept final;

  [[nodiscard]] webgpu::Handle<WGPUSurface> createWindowSurface(const webgpu::Device& device);
  [[nodiscard]] std::shared_ptr<ITexture> getDepth(uint32_t width, uint32_t height);

  std::unique_ptr<webgpu::Surface> surface_;
  std::shared_ptr<ITexture> offscreenColor_;
  std::shared_ptr<ITexture> depth_;
};

void WebGPUShell::releaseResources() noexcept {
  depth_.reset();
  offscreenColor_.reset();
  surface_.reset();
}

void WebGPUShell::willCreateWindow() noexcept {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

webgpu::Handle<WGPUSurface> WebGPUShell::createWindowSurface(const webgpu::Device& device) {
  WGPUSurfaceDescriptor desc = WGPU_SURFACE_DESCRIPTOR_INIT;
#if defined(_WIN32)
  WGPUSurfaceSourceWindowsHWND source = WGPU_SURFACE_SOURCE_WINDOWS_HWND_INIT;
  source.hinstance = GetModuleHandle(nullptr);
  source.hwnd = glfwGetWin32Window(window());
#else
  WGPUSurfaceSourceXlibWindow source = WGPU_SURFACE_SOURCE_XLIB_WINDOW_INIT;
  source.display = glfwGetX11Display();
  source.window = glfwGetX11Window(window());
#endif
  desc.nextInChain = &source.chain;
  return webgpu::Handle<WGPUSurface>(
      wgpuInstanceCreateSurface(device.getContext().getInstance(), &desc));
}

std::shared_ptr<Platform> WebGPUShell::createPlatform() noexcept {
  Result ret;
  auto ctx = webgpu::HWDevice::createContext({}, &ret);
  if (!ctx) {
    IGL_LOG_ERROR("[IGL Shell] WebGPU context creation failed: %s\n", ret.message.c_str());
    return nullptr;
  }
  const std::vector<HWDeviceDesc> devices =
      webgpu::HWDevice::queryDevices(*ctx, HWDeviceQueryDesc(HWDeviceType::Unknown), &ret);
  if (devices.empty()) {
    IGL_LOG_ERROR("[IGL Shell] No WebGPU adapter: %s\n", ret.message.c_str());
    return nullptr;
  }
  std::unique_ptr<webgpu::Device> device =
      webgpu::HWDevice::create(std::move(ctx), devices[0], &ret);
  if (!device) {
    IGL_LOG_ERROR("[IGL Shell] WebGPU device creation failed: %s\n", ret.message.c_str());
    return nullptr;
  }
  if (window() != nullptr) {
    surface_ = webgpu::Surface::create(*device, createWindowSurface(*device), &ret);
    if (!surface_) {
      IGL_LOG_ERROR("[IGL Shell] WebGPU surface creation failed: %s\n", ret.message.c_str());
      return nullptr;
    }
  }
  return std::make_shared<PlatformWin>(std::move(device));
}

std::shared_ptr<ITexture> WebGPUShell::getDepth(uint32_t width, uint32_t height) {
  if (!depth_ || depth_->getDimensions().width != width ||
      depth_->getDimensions().height != height) {
    Result ret;
    depth_ = platform().getDevice().createTexture(
        TextureDesc::new2D(TextureFormat::S8_UInt_Z24_UNorm,
                           width,
                           height,
                           TextureDesc::TextureUsageBits::Attachment),
        &ret);
    if (!depth_) {
      IGL_LOG_ERROR("[IGL Shell] WebGPU depth texture creation failed: %s\n", ret.message.c_str());
    }
  }
  return depth_;
}

SurfaceTextures WebGPUShell::createSurfaceTextures() noexcept {
  IGL_PROFILER_FUNCTION();
  const auto width = static_cast<uint32_t>(shellParams().viewportSize.x);
  const auto height = static_cast<uint32_t>(shellParams().viewportSize.y);
  // Present destroys a surface texture, and GlfwShell reads the screenshot back after present.
  if (!surface_ || shellParams().screenshotNumber != ~0u) {
    if (!offscreenColor_ || offscreenColor_->getDimensions().width != width ||
        offscreenColor_->getDimensions().height != height) {
      Result ret;
      offscreenColor_ = platform().getDevice().createTexture(
          TextureDesc::new2D(sessionConfig().swapchainColorTextureFormat,
                             width,
                             height,
                             TextureDesc::TextureUsageBits::Attachment |
                                 TextureDesc::TextureUsageBits::Sampled),
          &ret);
      if (!offscreenColor_) {
        IGL_LOG_ERROR("[IGL Shell] WebGPU color texture creation failed: %s\n",
                      ret.message.c_str());
      }
    }
    return {.color = offscreenColor_, .depth = getDepth(width, height)};
  }
  if (surface_->getWidth() != width || surface_->getHeight() != height) {
    const Result ret =
        surface_->configure(width, height, sessionConfig().swapchainColorTextureFormat);
    if (!ret.isOk()) {
      IGL_LOG_ERROR("[IGL Shell] WebGPU surface configuration failed: %s\n", ret.message.c_str());
    }
  }
  return {.color = surface_->getCurrentTexture(), .depth = getDepth(width, height)};
}

} // namespace

} // namespace igl::shell

int main(int argc, char* argv[]) {
  igl::shell::WebGPUShell shell;

  const igl::shell::RenderSessionWindowConfig suggestedWindowConfig = {
      .width = 1024,
      .height = 768,
      .windowMode = shell::WindowMode::Window,
  };
  const igl::shell::RenderSessionConfig suggestedConfig = {
      .displayName = "WebGPU",
      .backendVersion = {.flavor = BackendFlavor::WebGPU, .majorVersion = 1, .minorVersion = 0},
      .swapchainColorTextureFormat = TextureFormat::BGRA_UNorm8,
  };

  if (!shell.initialize(argc, argv, suggestedWindowConfig, suggestedConfig)) {
    shell.releaseResources();
    shell.teardown();
    return -1;
  }

  shell.run();
  shell.releaseResources();
  shell.teardown();

  return 0;
}
