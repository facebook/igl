/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <string>
#include <vector>
#define XR_USE_PLATFORM_ANDROID
#include <openxr/openxr.h>

#include <glm/glm.hpp>

#include <igl/IGL.h>
#include <shell/shared/platform/android/PlatformAndroid.h>
#include <shell/shared/renderSession/RenderSession.h>

struct android_app;
class AAssetManager;

// forward declarations
namespace igl::shell::openxr::impl {
class XrAppImpl;
}

namespace igl::shell::openxr::mobile {
class XrSwapchainProvider;

class XrApp {
 public:
  XrApp(std::unique_ptr<impl::XrAppImpl>&& impl);
  ~XrApp();

  inline bool initialized() const {
    return initialized_;
  }
  bool initialize(const struct android_app* app);

  XrInstance instance() const;

  void handleXrEvents();

  void update();

  void setNativeWindow(void* win) {
    nativeWindow_ = win;
  }
  void* nativeWindow() const {
    return nativeWindow_;
  }

  void setResumed(bool resumed) {
    resumed_ = resumed;
  }
  bool resumed() const {
    return resumed_;
  }

  bool sessionActive() const {
    return sessionActive_;
  }
  XrSession session() const;

 private:
  bool checkExtensions();
  bool createInstance();
  bool createSystem();
  bool enumerateViewConfigurations();
  void enumerateReferenceSpaces();
  void createSwapchainProviders(const std::unique_ptr<igl::IDevice>& device);
  void handleSessionStateChanges(XrSessionState state);
  void createShellSession(std::unique_ptr<igl::IDevice> device, AAssetManager* assetMgr);

  void createSpaces();
  XrFrameState beginFrame();
  void render();
  void endFrame(XrFrameState frameState);

 private:
  void* nativeWindow_ = nullptr;
  bool resumed_ = false;
  bool sessionActive_ = false;

  std::vector<XrExtensionProperties> extensions_;
  std::vector<const char*> requiredExtensions_ = {
      XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME,
  };

  XrInstanceProperties instanceProps_ = {
      .type = XR_TYPE_INSTANCE_PROPERTIES,
      .next = nullptr,
  };

  XrSystemProperties systemProps_ = {
      .type = XR_TYPE_SYSTEM_PROPERTIES,
      .next = nullptr,
  };

  XrInstance instance_ = XR_NULL_HANDLE;
  XrSystemId systemId_ = XR_NULL_SYSTEM_ID;
  XrSession session_ = XR_NULL_HANDLE;

  XrViewConfigurationProperties viewConfigProps_ = {.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
  static constexpr auto kNumViews = 2; // 2 for stereo
  std::array<XrViewConfigurationView, kNumViews> viewports_;
  std::array<XrView, kNumViews> views_;
  std::array<XrPosef, kNumViews> viewStagePoses_;
  std::array<glm::mat4, kNumViews> viewTransforms_;
  std::array<glm::vec3, kNumViews> cameraPositions_;

  bool useSinglePassStereo_ = true;

  // If useSinglePassStereo_ is true, only one XrSwapchainProvider will be created.
  std::vector<std::unique_ptr<XrSwapchainProvider>> swapchainProviders_;

  XrSpace headSpace_ = XR_NULL_HANDLE;
  XrSpace localSpace_ = XR_NULL_HANDLE;
  XrSpace stageSpace_ = XR_NULL_HANDLE;
  bool stageSpaceSupported_ = false;

  std::unique_ptr<impl::XrAppImpl> impl_;

  bool initialized_ = false;

  std::shared_ptr<igl::shell::PlatformAndroid> platform_;
  std::unique_ptr<igl::shell::RenderSession> renderSession_;

  std::unique_ptr<igl::shell::ShellParams> shellParams_;
};
} // namespace igl::shell::openxr::mobile
