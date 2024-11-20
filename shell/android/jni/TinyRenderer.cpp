/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include "TinyRenderer.h"

#include <EGL/egl.h>
#include <android/log.h>
#include <android/native_window.h>
#if IGL_BACKEND_OPENGL
#include <igl/opengl/egl/HWDevice.h>
#include <igl/opengl/egl/PlatformDevice.h>
#endif
#include <shell/shared/fileLoader/android/FileLoaderAndroid.h>
#include <shell/shared/imageLoader/android/ImageLoaderAndroid.h>
#include <shell/shared/input/InputDispatcher.h>
#include <shell/shared/platform/DisplayContext.h>
#include <shell/shared/renderSession/ShellParams.h>
#if IGL_BACKEND_VULKAN
#include <igl/vulkan/Device.h>
#include <igl/vulkan/HWDevice.h>
#include <igl/vulkan/VulkanContext.h>
#endif
#include <memory>

namespace {

// Stores the current EGL context when created, and restores it when destroyed.
struct ContextGuard {
  ContextGuard(const igl::IDevice& device) {
#if IGL_BACKEND_OPENGL
    backend = device.getBackendType();
    if (backend == igl::BackendType::OpenGL) {
      display = eglGetCurrentDisplay();
      context = eglGetCurrentContext();
      readSurface = eglGetCurrentSurface(EGL_READ);
      drawSurface = eglGetCurrentSurface(EGL_DRAW);
    }
#endif
  }

  ~ContextGuard() {
#if IGL_BACKEND_OPENGL
    if (backend == igl::BackendType::OpenGL) {
      eglMakeCurrent(display, readSurface, drawSurface, context);
    }
#endif
  }

 private:
#if IGL_BACKEND_OPENGL
  igl::BackendType backend;
  EGLDisplay display;
  EGLContext context;
  EGLSurface readSurface;
  EGLSurface drawSurface;
#endif
};

} // namespace

namespace igl::samples {

using namespace igl;

void TinyRenderer::init(AAssetManager* mgr,
                        ANativeWindow* nativeWindow,
                        shell::IRenderSessionFactory& factory,
                        BackendVersion backendVersion,
                        TextureFormat swapchainColorTextureFormat) {
  backendVersion_ = backendVersion;
  nativeWindow_ = nativeWindow;
  Result result;
  const igl::HWDeviceQueryDesc queryDesc(HWDeviceType::IntegratedGpu);
  std::unique_ptr<IDevice> d;

  switch (backendVersion_.flavor) {
#if IGL_BACKEND_OPENGL
  case igl::BackendFlavor::OpenGL_ES: {
    auto hwDevice = opengl::egl::HWDevice();
    auto hwDevices = hwDevice.queryDevices(queryDesc, &result);
    IGL_DEBUG_ASSERT(result.isOk());
    // Decide which backend api to use, default as GLES3
    auto backendType = (backendVersion_.majorVersion == 3) ? igl::opengl::RenderingAPI::GLES3
                                                           : igl::opengl::RenderingAPI::GLES2;
    d = hwDevice.create(hwDevices[0], backendType, nullptr, &result);
    shellParams_.shouldPresent = false;
    break;
  }
#endif

#if IGL_BACKEND_VULKAN
  case igl::BackendFlavor::Vulkan: {
    IGL_DEBUG_ASSERT(nativeWindow != nullptr);
    vulkan::VulkanContextConfig config;
    config.terminateOnValidationError = true;
    config.requestedSwapChainTextureFormat = swapchainColorTextureFormat;
    auto ctx = vulkan::HWDevice::createContext(config, nativeWindow);

    auto devices = vulkan::HWDevice::queryDevices(
        *ctx, HWDeviceQueryDesc(HWDeviceType::IntegratedGpu), &result);

    IGL_DEBUG_ASSERT(result.isOk());
    width_ = static_cast<uint32_t>(ANativeWindow_getWidth(nativeWindow));
    height_ = static_cast<uint32_t>(ANativeWindow_getHeight(nativeWindow));

    // https://github.com/gpuweb/gpuweb/issues/4283
    // Only 49.5% of Android devices support dualSrcBlend.
    // Android devices that do not support dualSrcBlend primarily use ARM, ImgTec, and Qualcomm
    // GPUs.
    // https://vulkan.gpuinfo.org/listdevicescoverage.php?feature=dualSrcBlend&platform=android&option=not
    igl::vulkan::VulkanFeatures vulkanFeatures(VK_API_VERSION_1_1, config);
    vulkanFeatures.enableDefaultFeatures1_1();
    vulkanFeatures.VkPhysicalDeviceFeatures2_.features.dualSrcBlend = VK_FALSE;

    d = vulkan::HWDevice::create(std::move(ctx),
                                 devices[0],
                                 width_, // width
                                 height_, // height,,
                                 0,
                                 nullptr,
                                 &vulkanFeatures,
                                 &result);
    break;
  }
#endif

  default: {
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
    return;
  }
  }

  IGL_DEBUG_ASSERT(d != nullptr);
  // We want to catch failed device creation instead of letting implicitly fail
  IGL_SOFT_ASSERT(result.isOk());
  if (d) {
    platform_ = std::make_shared<igl::shell::PlatformAndroid>(std::move(d));
    IGL_DEBUG_ASSERT(platform_ != nullptr);
    static_cast<igl::shell::ImageLoaderAndroid&>(platform_->getImageLoader()).setAssetManager(mgr);
    static_cast<igl::shell::FileLoaderAndroid&>(platform_->getFileLoader()).setAssetManager(mgr);

    const ContextGuard guard(platform_->getDevice()); // wrap 'session_' operations

    session_ = factory.createRenderSession(platform_);
    session_->setShellParams(shellParams_);
    IGL_DEBUG_ASSERT(session_ != nullptr);
    session_->initialize();
  }
}

void TinyRenderer::recreateSwapchain(ANativeWindow* nativeWindow, bool createSurface) {
#if IGL_BACKEND_VULKAN
  nativeWindow_ = nativeWindow;
  width_ = static_cast<uint32_t>(ANativeWindow_getWidth(nativeWindow));
  height_ = static_cast<uint32_t>(ANativeWindow_getHeight(nativeWindow));

  auto* platformDevice = platform_->getDevice().getPlatformDevice<igl::vulkan::PlatformDevice>();
  // need clear the cached textures before recreate swap chain.
  platformDevice->clear();

  auto& vulkanDevice = static_cast<igl::vulkan::Device&>(platform_->getDevice());
  auto& vkContext = vulkanDevice.getVulkanContext();

  if (createSurface) {
    vkContext.createSurface(nativeWindow, nullptr);
  }
  vkContext.initSwapchain(width_, height_);

  // need release frame buffer when recreate swap chain
  session_->releaseFramebuffer();
#endif
}

void TinyRenderer::render(float displayScale) {
  // process user input
  IGL_DEBUG_ASSERT(platform_ != nullptr);
  platform_->getInputDispatcher().processEvents();

  // draw
  Result result;
  igl::SurfaceTextures surfaceTextures;

  switch (backendVersion_.flavor) {
#if IGL_BACKEND_OPENGL
  case igl::BackendFlavor::OpenGL_ES: {
    auto* platformDevice = platform_->getDevice().getPlatformDevice<opengl::egl::PlatformDevice>();
    surfaceTextures.color = platformDevice->createTextureFromNativeDrawable(&result);
    surfaceTextures.depth =
        platformDevice->createTextureFromNativeDepth(igl::TextureFormat::Z_UNorm24, &result);
    break;
  }
#endif

#if IGL_BACKEND_VULKAN
  case igl::BackendFlavor::Vulkan: {
    auto* platformDevice = platform_->getDevice().getPlatformDevice<vulkan::PlatformDevice>();
    surfaceTextures.color = platformDevice->createTextureFromNativeDrawable(&result);
    surfaceTextures.depth = platformDevice->createTextureFromNativeDepth(width_, height_, &result);
    break;
  }
#endif

  default:
    Result::setResult(&result, Result::Code::Unsupported, "Invalid backend");
    break;
  }
  IGL_DEBUG_ASSERT(result.isOk());
  IGL_SOFT_ASSERT(result.isOk());

  const ContextGuard guard(platform_->getDevice()); // wrap 'session_' operations

  session_->setPixelsPerPoint(displayScale);
  session_->update(std::move(surfaceTextures));
}

void TinyRenderer::onSurfacesChanged(ANativeWindow* /*surface*/, int width, int height) {
  width_ = static_cast<uint32_t>(width);
  height_ = static_cast<uint32_t>(height);
#if IGL_BACKEND_OPENGL
  if (backendVersion_.flavor == igl::BackendFlavor::OpenGL_ES) {
    auto* readSurface = eglGetCurrentSurface(EGL_READ);
    auto* drawSurface = eglGetCurrentSurface(EGL_DRAW);

    IGL_DEBUG_ASSERT(platform_ != nullptr);
    Result result;
    platform_->getDevice().getPlatformDevice<opengl::egl::PlatformDevice>()->updateSurfaces(
        readSurface, drawSurface, &result);
    IGL_DEBUG_ASSERT(result.isOk());
    IGL_SOFT_ASSERT(result.isOk());
  }
#endif

#if IGL_BACKEND_VULKAN
  if (backendVersion_.flavor == igl::BackendFlavor::Vulkan) {
    recreateSwapchain(nativeWindow_, false);
  }
#endif
}

void TinyRenderer::touchEvent(bool isDown, float x, float y, float dx, float dy) {
  const float scale = platform_->getDisplayContext().pixelsPerPoint;
  IGL_DEBUG_ASSERT(scale > 0.0f);
  platform_->getInputDispatcher().queueEvent(
      igl::shell::TouchEvent(isDown, x / scale, y / scale, dx / scale, dy / scale));
}

void TinyRenderer::setClearColorValue(float r, float g, float b, float a) {
  shellParams_.clearColorValue = {r, g, b, a};
}

} // namespace igl::samples
