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
#include <android/native_window_jni.h>
#include <cmath>
#include <igl/IGL.h>
#if IGL_BACKEND_OPENGL
#include <igl/opengl/egl/HWDevice.h>
#include <igl/opengl/egl/PlatformDevice.h>
#endif
#include <shell/shared/fileLoader/android/FileLoaderAndroid.h>
#include <shell/shared/imageLoader/android/ImageLoaderAndroid.h>
#include <shell/shared/renderSession/DefaultSession.h>
#include <shell/shared/renderSession/ShellParams.h>
#if IGL_BACKEND_VULKAN
#include <igl/vulkan/Device.h>
#include <igl/vulkan/HWDevice.h>
#include <igl/vulkan/VulkanContext.h>
#endif
#include <memory>
#include <sstream>

namespace igl::samples {

using namespace igl;

void TinyRenderer::init(AAssetManager* mgr,
                        ANativeWindow* nativeWindow,
                        BackendTypeID backendTypeID) {
  backendTypeID_ = backendTypeID;
  nativeWindow_ = nativeWindow;
  Result result;
  const igl::HWDeviceQueryDesc queryDesc(HWDeviceType::IntegratedGpu);
  std::unique_ptr<IDevice> d;

  switch (backendTypeID_) {
#if IGL_BACKEND_OPENGL
  case BackendTypeID::GLES2:
  case BackendTypeID::GLES3: {
    auto hwDevice = opengl::egl::HWDevice();
    auto hwDevices = hwDevice.queryDevices(queryDesc, &result);
    IGL_ASSERT(result.isOk());
    // Decide which backend api to use, default as GLES3
    auto backendType = (backendTypeID_ == BackendTypeID::GLES3) ? igl::opengl::RenderingAPI::GLES3
                                                                : igl::opengl::RenderingAPI::GLES2;
    d = hwDevice.create(hwDevices[0], backendType, nullptr, &result);
    shellParams_.shouldPresent = false;
    break;
  }
#endif

#if IGL_BACKEND_VULKAN
  case BackendTypeID::Vulkan: {
    isSurfaceCreated_ = true;
    IGL_ASSERT(nativeWindow != nullptr);
    vulkan::VulkanContextConfig config;
    config.terminateOnValidationError = true;
    auto ctx = vulkan::HWDevice::createContext(config, nativeWindow);

    auto devices = vulkan::HWDevice::queryDevices(
        *ctx, HWDeviceQueryDesc(HWDeviceType::IntegratedGpu), &result);

    IGL_ASSERT(result.isOk());
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
    IGL_ASSERT_NOT_IMPLEMENTED();
    break;
  }
  }

  IGL_ASSERT(d != nullptr);
  // We want to catch failed device creation instead of letting implicitly fail
  IGL_REPORT_ERROR(result.isOk());
  if (d) {
    platform_ = std::make_shared<igl::shell::PlatformAndroid>(std::move(d));
    IGL_ASSERT(platform_ != nullptr);

#if IGL_BACKEND_VULKAN
    updateSurfaceTransformRotate(platform_->getDevice());
#endif

    static_cast<igl::shell::ImageLoaderAndroid&>(platform_->getImageLoader()).setAssetManager(mgr);
    static_cast<igl::shell::FileLoaderAndroid&>(platform_->getFileLoader()).setAssetManager(mgr);
    session_ = igl::shell::createDefaultRenderSession(platform_);
    session_->setShellParams(shellParams_);
    IGL_ASSERT(session_ != nullptr);
    session_->initialize();
  }
}

void TinyRenderer::recreateSwapchain(ANativeWindow* nativeWindow, bool createSurface) {
#if IGL_BACKEND_VULKAN
  nativeWindow_ = nativeWindow;
  width_ = static_cast<uint32_t>(ANativeWindow_getWidth(nativeWindow));
  height_ = static_cast<uint32_t>(ANativeWindow_getHeight(nativeWindow));

  auto* platform_device = platform_->getDevice().getPlatformDevice<igl::vulkan::PlatformDevice>();
  // need clear the cached textures before recreate swap chain.
  platform_device->clear();

  auto& vulkan_device = static_cast<igl::vulkan::Device&>(platform_->getDevice());
  auto& vk_context = vulkan_device.getVulkanContext();

  if (createSurface) {
      vk_context.createSurface(nativeWindow, nullptr);
  }
  vk_context.initSwapchain(width_, height_);

  // need release frame buffer when recreate swap chain
  session_->releaseFramebuffer();
#endif
}

void TinyRenderer::render(float displayScale) {
  igl::DeviceScope const scope(platform_->getDevice());

  // process user input
  IGL_ASSERT(platform_ != nullptr);
  platform_->getInputDispatcher().processEvents();

  // draw
  Result result;
  igl::SurfaceTextures surfaceTextures;

  switch (backendTypeID_) {
#if IGL_BACKEND_OPENGL
  case BackendTypeID::GLES2:
  case BackendTypeID::GLES3: {
    auto* platformDevice = platform_->getDevice().getPlatformDevice<opengl::egl::PlatformDevice>();
    surfaceTextures.color = platformDevice->createTextureFromNativeDrawable(&result);
    surfaceTextures.depth =
        platformDevice->createTextureFromNativeDepth(igl::TextureFormat::Z_UNorm24, &result);
    break;
  }
#endif

#if IGL_BACKEND_VULKAN
  case BackendTypeID::Vulkan: {
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
  IGL_ASSERT(result.isOk());
  IGL_REPORT_ERROR(result.isOk());
  session_->setPixelsPerPoint(displayScale);
  session_->update(std::move(surfaceTextures));
}

void TinyRenderer::onSurfacesChanged(ANativeWindow* /*surface*/, int width, int height) {
  igl::DeviceScope const scope(platform_->getDevice());

  width_ = static_cast<uint32_t>(width);
  height_ = static_cast<uint32_t>(height);
#if IGL_BACKEND_OPENGL
  if (backendTypeID_ == BackendTypeID::GLES2 || backendTypeID_ == BackendTypeID::GLES3) {
    auto* readSurface = eglGetCurrentSurface(EGL_READ);
    auto* drawSurface = eglGetCurrentSurface(EGL_DRAW);

    IGL_ASSERT(platform_ != nullptr);
    Result result;
    platform_->getDevice().getPlatformDevice<opengl::egl::PlatformDevice>()->updateSurfaces(
        readSurface, drawSurface, &result);
    IGL_ASSERT(result.isOk());
    IGL_REPORT_ERROR(result.isOk());
  }
#endif

#if IGL_BACKEND_VULKAN
  if (backendTypeID_ == BackendTypeID::Vulkan){
      if (isSurfaceCreated_){
          isSurfaceCreated_ = false;
          return;
      }

      recreateSwapchain(nativeWindow_, false);

      updateSurfaceTransformRotate(platform_->getDevice());
  }
#endif
}

void TinyRenderer::updateSurfaceTransformRotate(igl::IDevice& device) {
#if IGL_BACKEND_VULKAN
    if (backendTypeID_ != BackendTypeID::Vulkan){
        return;
    }
    igl::vulkan::Device& vulkan_device = static_cast<igl::vulkan::Device&>(device);
    igl::vulkan::VulkanContext& vk_context = vulkan_device.getVulkanContext();
    const VkSurfaceCapabilitiesKHR& surface_capabilities = vk_context.getSurfaceCapabilities();

    SurfaceTransformRotate surfaceTransformRotate = kSurfaceTransformRotate0;

    switch (surface_capabilities.currentTransform) {
        case VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR:
            surfaceTransformRotate = kSurfaceTransformRotate0;
            break;

        case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
            surfaceTransformRotate = kSurfaceTransformRotate90;
            break;

        case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
            surfaceTransformRotate = kSurfaceTransformRotate180;
            break;

        case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
            surfaceTransformRotate = kSurfaceTransformRotate270;
            break;

        default:
            break;
    }

    platform_->updateSurfaceTransformRotate(surfaceTransformRotate);
#endif
}

void TinyRenderer::touchEvent(bool isDown, float x, float y, float dx, float dy) {
  const float scale = platform_->getDisplayContext().pixelsPerPoint;
  IGL_ASSERT(scale > 0.0f);
  platform_->getInputDispatcher().queueEvent(
      igl::shell::TouchEvent(isDown, x / scale, y / scale, dx / scale, dy / scale));
}

void TinyRenderer::setClearColorValue(float r, float g, float b, float a) {
  shellParams_.clearColorValue = {r, g, b, a};
};

} // namespace igl::samples
