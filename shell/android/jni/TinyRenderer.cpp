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
#include <shell/shared/platform/DisplayContext.h>
#include <shell/shared/input/InputDispatcher.h>
#include <shell/shared/renderSession/RenderSession.h>
#include <shell/shared/renderSession/ShellParams.h>
#if IGL_BACKEND_VULKAN
#include <igl/vulkan/Device.h>
#include <igl/vulkan/HWDevice.h>
#include <igl/vulkan/VulkanContext.h>
#endif
#include <memory>
#include <sys/system_properties.h>
#include <unordered_set>

namespace {

// Helper functions to read Android system properties
std::optional<std::string> getAndroidSystemProperty(const char* keyName) noexcept {
  std::array<char, PROP_VALUE_MAX> value{};
  int len = __system_property_get(keyName, value.data());
  if (len > 0) {
    return std::string(value.data());
  }
  return std::nullopt;
}

std::optional<bool> getAndroidSystemPropertyBool(const char* keyName) noexcept {
  auto prop = getAndroidSystemProperty(keyName);
  if (!prop.has_value()) {
    return std::nullopt;
  }
  const auto& value = prop.value();
  if (value == "true" || value == "1") {
    return true;
  }
  if (value == "false" || value == "0") {
    return false;
  }
  return std::nullopt;
}

std::optional<int> getAndroidSystemPropertyInt(const char* keyName) noexcept {
  auto prop = getAndroidSystemProperty(keyName);
  if (!prop.has_value()) {
    return std::nullopt;
  }
  try {
    return std::stoi(prop.value());
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<size_t> getAndroidSystemPropertySizeT(const char* keyName) noexcept {
  auto prop = getAndroidSystemProperty(keyName);
  if (!prop.has_value()) {
    return std::nullopt;
  }
  try {
    return std::stoul(prop.value());
  } catch (...) {
    return std::nullopt;
  }
}

// Read shell parameters from Android system properties
void readShellParamsFromAndroidProps(igl::shell::ShellParams& shellParams,
                                     const char* prefix) noexcept {
  std::string prefixStr(prefix);
  prefixStr += ".";

  // Read ShellParams
  auto headless = getAndroidSystemPropertyBool((prefixStr + "headless").c_str());
  if (headless.has_value()) {
    shellParams.isHeadless = headless.value();
    if (shellParams.isHeadless && shellParams.screenshotNumber == ~0u) {
      shellParams.screenshotNumber = 0;
    }
  }

  auto disableVulkanValidation =
      getAndroidSystemPropertyBool((prefixStr + "disable-vulkan-validation-layers").c_str());
  if (disableVulkanValidation.has_value()) {
    shellParams.enableVulkanValidationLayers = !disableVulkanValidation.value();
  }

  auto screenshotFile = getAndroidSystemProperty((prefixStr + "screenshot-file").c_str());
  if (screenshotFile.has_value()) {
    shellParams.screenshotFileName = screenshotFile.value();
  }

  auto screenshotNumber = getAndroidSystemPropertyInt((prefixStr + "screenshot-number").c_str());
  if (screenshotNumber.has_value()) {
    shellParams.screenshotNumber = static_cast<uint32_t>(screenshotNumber.value());
  }

  auto viewportSize = getAndroidSystemProperty((prefixStr + "viewport-size").c_str());
  if (viewportSize.has_value()) {
    unsigned int w = 0;
    unsigned int h = 0;
    if (sscanf(viewportSize.value().c_str(), "%ux%u", &w, &h) == 2) {
      if (w && h) {
        shellParams.viewportSize = glm::vec2(w, h);
      }
    }
  }

  // Read BenchmarkRenderSessionParams - always try to read them
  auto timeout = getAndroidSystemPropertySizeT((prefixStr + "timeout").c_str());
  auto sessions = getAndroidSystemPropertySizeT((prefixStr + "sessions").c_str());
  auto logReporter = getAndroidSystemPropertyBool((prefixStr + "log-reporter").c_str());
  auto offscreenOnly = getAndroidSystemPropertyBool((prefixStr + "offscreen-only").c_str());
  auto benchmark = getAndroidSystemPropertyBool((prefixStr + "benchmark").c_str());

  // Read custom parameters using __system_property_foreach (API 26+)
  // Custom parameters are any properties under the prefix that are not standard params
  std::vector<std::pair<std::string, std::string>> customParams;

#if __ANDROID_API__ >= 26
  // Known standard parameter names to exclude from custom params
  static const std::unordered_set<std::string> standardParams = {"headless",
                                                                 "disable-vulkan-validation-layers",
                                                                 "screenshot-file",
                                                                 "screenshot-number",
                                                                 "viewport-size",
                                                                 "timeout",
                                                                 "sessions",
                                                                 "log-reporter",
                                                                 "offscreen-only",
                                                                 "benchmark"};

  struct CallbackData {
    const std::string& prefix;
    const std::unordered_set<std::string>& standardParams;
    std::vector<std::pair<std::string, std::string>>* customParams;
  };

  CallbackData callbackData{prefixStr, standardParams, &customParams};

  __system_property_foreach(
      [](const prop_info* pi, void* cookie) {
        auto* data = reinterpret_cast<CallbackData*>(cookie);

        // Get property name
        char name[PROP_NAME_MAX];
        char value[PROP_VALUE_MAX];
        __system_property_read(pi, name, value);

        std::string propName(name);
        // Check if property starts with our prefix
        if (propName.rfind(data->prefix, 0) == 0) {
          // Extract the key (remove prefix)
          std::string key = propName.substr(data->prefix.length());
          // Only add if not empty and not a standard parameter
          if (!key.empty() && data->standardParams.find(key) == data->standardParams.end()) {
            data->customParams->emplace_back(key, std::string(value));
          }
        }
      },
      reinterpret_cast<void*>(&callbackData));
#endif

  // If any benchmark parameter is set (including custom params), create the benchmark params
  if (timeout.has_value() || sessions.has_value() || logReporter.has_value() ||
      offscreenOnly.has_value() || benchmark.has_value() || !customParams.empty()) {
    if (!shellParams.benchmarkParams.has_value()) {
      shellParams.benchmarkParams = igl::shell::BenchmarkRenderSessionParams();
    }

    if (timeout.has_value()) {
      shellParams.benchmarkParams->renderSessionTimeoutMs = timeout.value();
    }
    if (sessions.has_value()) {
      shellParams.benchmarkParams->numSessionsToRun = sessions.value();
    }
    if (logReporter.has_value()) {
      shellParams.benchmarkParams->logReporter = logReporter.value();
    }
    if (offscreenOnly.has_value()) {
      shellParams.benchmarkParams->offscreenRenderingOnly = offscreenOnly.value();
    }

    // Add custom parameters
    for (const auto& [key, value] : customParams) {
      shellParams.benchmarkParams->customParams.emplace_back(key, value);
    }
  }
}

// Stores the current EGL context when created, and restores it when destroyed.
struct ContextGuard {
  ContextGuard(const igl::IDevice& device) {
#if IGL_BACKEND_OPENGL
    backend_ = device.getBackendType();
    if (backend_ == igl::BackendType::OpenGL) {
      display_ = eglGetCurrentDisplay();
      context_ = eglGetCurrentContext();
      readSurface_ = eglGetCurrentSurface(EGL_READ);
      drawSurface_ = eglGetCurrentSurface(EGL_DRAW);
    }
#endif
  }

  ~ContextGuard() {
#if IGL_BACKEND_OPENGL
    if (backend_ == igl::BackendType::OpenGL) {
      eglMakeCurrent(display_, readSurface_, drawSurface_, context_);
    }
#endif
  }
  ContextGuard(const ContextGuard&) = delete;
  ContextGuard& operator=(const ContextGuard&) = delete;
  ContextGuard(ContextGuard&&) = delete;
  ContextGuard& operator=(ContextGuard&&) = delete;

 private:
#if IGL_BACKEND_OPENGL
  igl::BackendType backend_;
  EGLDisplay display_;
  EGLContext context_;
  EGLSurface readSurface_;
  EGLSurface drawSurface_;
#endif
};

} // namespace

namespace igl::samples {

using namespace igl;

void TinyRenderer::init(AAssetManager* mgr,
                        ANativeWindow* nativeWindow,
                        shell::IRenderSessionFactory& factory,
                        BackendVersion backendVersion,
                        TextureFormat swapchainColorTextureFormat,
                        const std::vector<std::string>& args) {
  backendVersion_ = backendVersion;
  nativeWindow_ = nativeWindow;
  Result result;
  const igl::HWDeviceQueryDesc queryDesc(HWDeviceType::IntegratedGpu);
  std::unique_ptr<IDevice> d;

  // Read shell params from Android system properties first
  readShellParamsFromAndroidProps(shellParams_, factory.getAndroidSystemPropsPrefix());

  // Parse shell params from command line (overrides properties)
  shell::parseShellParams(args, shellParams_);

  switch (backendVersion_.flavor) {
#if IGL_BACKEND_OPENGL
  case igl::BackendFlavor::OpenGL_ES: {
    auto hwDevice = opengl::egl::HWDevice();
    // Decide which backend api to use, default as GLES3
    d = hwDevice.create(backendVersion_, &result);
    shellParams_.shouldPresent = false;

    if (swapchainColorTextureFormat == TextureFormat::Invalid) {
      swapchainColorTextureFormat_ = TextureFormat::RGBA_SRGB;
    }

    if (!d->hasFeature(DeviceFeatures::SRGB) && !d->hasFeature(DeviceFeatures::SRGBSwapchain)) {
      swapchainColorTextureFormat_ = TextureFormat::RGBA_UNorm8;
    }

    break;
  }
#endif

#if IGL_BACKEND_VULKAN
  case igl::BackendFlavor::Vulkan: {
    IGL_DEBUG_ASSERT(nativeWindow != nullptr);
    vulkan::VulkanContextConfig config;
    config.terminateOnValidationError = true;
    config.requestedSwapChainTextureFormat = swapchainColorTextureFormat;
    // Don't use headless mode on Android - instead we'll render to offscreen surface
    config.headless = false;

    auto ctx = vulkan::HWDevice::createContext(config, nativeWindow);

    auto devices =
        vulkan::HWDevice::queryDevices(*ctx, HWDeviceQueryDesc(HWDeviceType::Unknown), &result);

    if (!result.isOk()) {
      __android_log_print(ANDROID_LOG_ERROR, "igl", "Error: %s\n", result.message.c_str());
    }
    IGL_DEBUG_ASSERT(result.isOk());

    if (shellParams_.isHeadless) {
      // Use viewport size from shell params for headless mode
      width_ = static_cast<uint32_t>(shellParams_.viewportSize.x);
      height_ = static_cast<uint32_t>(shellParams_.viewportSize.y);
    } else {
      width_ = static_cast<uint32_t>(ANativeWindow_getWidth(nativeWindow));
      height_ = static_cast<uint32_t>(ANativeWindow_getHeight(nativeWindow));
    }

    // https://github.com/gpuweb/gpuweb/issues/4283
    // Only 49.5% of Android devices support dualSrcBlend.
    // Android devices that do not support dualSrcBlend primarily use ARM, ImgTec, and Qualcomm
    // GPUs.
    // https://vulkan.gpuinfo.org/listdevicescoverage.php?feature=dualSrcBlend&platform=android&option=not
    igl::vulkan::VulkanFeatures vulkanFeatures(config);
    vulkanFeatures.vkPhysicalDeviceFeatures2.features.dualSrcBlend = VK_FALSE;

    d = vulkan::HWDevice::create(std::move(ctx),
                                 devices[0],
                                 width_, // width
                                 height_, // height,,
                                 0,
                                 nullptr,
                                 &vulkanFeatures,
                                 "TinyRenderer",
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
  SurfaceTextures surfaceTextures;

  if (shellParams_.isHeadless) {
    // In headless mode, create offscreen textures instead of native drawable textures
    auto& device = platform_->getDevice();

    // Create or reuse offscreen color texture
    if (!offscreenColorTexture_ || offscreenColorTexture_->getSize().width != width_ ||
        offscreenColorTexture_->getSize().height != height_) {
      TextureDesc colorTexDesc = TextureDesc::new2D(swapchainColorTextureFormat_,
                                                    width_,
                                                    height_,
                                                    TextureDesc::TextureUsageBits::Attachment |
                                                        TextureDesc::TextureUsageBits::Sampled);
      colorTexDesc.storage = ResourceStorage::Private;
      offscreenColorTexture_ = device.createTexture(colorTexDesc, &result);
      IGL_DEBUG_ASSERT(result.isOk());
      IGL_SOFT_ASSERT(result.isOk());
    }

    // Create or reuse offscreen depth texture
    if (!offscreenDepthTexture_ || offscreenDepthTexture_->getSize().width != width_ ||
        offscreenDepthTexture_->getSize().height != height_) {
      TextureDesc depthTexDesc = TextureDesc::new2D(
          TextureFormat::Z_UNorm24, width_, height_, TextureDesc::TextureUsageBits::Attachment);
      depthTexDesc.storage = ResourceStorage::Private;
      offscreenDepthTexture_ = device.createTexture(depthTexDesc, &result);
      IGL_DEBUG_ASSERT(result.isOk());
      IGL_SOFT_ASSERT(result.isOk());
    }

    surfaceTextures.color = offscreenColorTexture_;
    surfaceTextures.depth = offscreenDepthTexture_;
  } else {
    // Normal mode: create surface textures from native drawable
    switch (backendVersion_.flavor) {
#if IGL_BACKEND_OPENGL
    case igl::BackendFlavor::OpenGL_ES: {
      auto* platformDevice =
          platform_->getDevice().getPlatformDevice<opengl::egl::PlatformDevice>();
      surfaceTextures.color =
          platformDevice->createTextureFromNativeDrawable(swapchainColorTextureFormat_, &result);
      surfaceTextures.depth =
          platformDevice->createTextureFromNativeDepth(igl::TextureFormat::Z_UNorm24, &result);
      break;
    }
#endif

#if IGL_BACKEND_VULKAN
    case igl::BackendFlavor::Vulkan: {
      auto* platformDevice = platform_->getDevice().getPlatformDevice<vulkan::PlatformDevice>();
      surfaceTextures.color = platformDevice->createTextureFromNativeDrawable(&result);
      surfaceTextures.depth =
          platformDevice->createTextureFromNativeDepth(width_, height_, &result);
      break;
    }
#endif

    default:
      Result::setResult(&result, Result::Code::Unsupported, "Invalid backend");
      break;
    }
    IGL_DEBUG_ASSERT(result.isOk());
    IGL_SOFT_ASSERT(result.isOk());
  }

  const ContextGuard guard(platform_->getDevice()); // wrap 'session_' operations

  platform_->getDevice().setCurrentThread();
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
    platform_->updatePreRotationMatrix();
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
