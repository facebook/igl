/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <igl/Macros.h>

#include <array>
#include <string>
#include <unordered_set>
#include <vector>

#include <shell/openxr/XrComposition.h>
#include <shell/openxr/XrPlatform.h>
#include <shell/openxr/XrRefreshRate.h>

#include <glm/glm.hpp>

#include <igl/IGL.h>
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>

struct android_app;
struct AAssetManager;

// forward declarations
namespace igl::shell::openxr {
class XrHands;
class XrSwapchainProvider;
class XrPassthrough;
namespace impl {
class XrAppImpl;
}
} // namespace igl::shell::openxr

namespace igl::shell::openxr {

class XrApp {
 public:
  XrApp(std::unique_ptr<impl::XrAppImpl>&& impl, bool shouldPresent = true);
  ~XrApp();

  inline bool initialized() const {
    return initialized_;
  }

  struct InitParams {
    XrRefreshRate::Params refreshRateParams;
  };
  bool initialize(const struct android_app* app, const InitParams& params);

  XrInstance instance() const;

  void handleXrEvents();
  void handleActionView(const std::string& data);

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
  void enumerateBlendModes();
  void updateSwapchainProviders();
  void handleSessionStateChanges(XrSessionState state);
  void createShellSession(std::unique_ptr<igl::IDevice> device, AAssetManager* assetMgr);

  void createSpaces();
  XrFrameState beginFrame();
  void render();
  void endFrame(XrFrameState frameState);

  void updateQuadComposition() noexcept;

  [[nodiscard]] inline bool passthroughSupported() const noexcept;
  [[nodiscard]] inline bool passthroughEnabled() const noexcept;

  [[nodiscard]] inline bool handsTrackingSupported() const noexcept;
  [[nodiscard]] inline bool handsTrackingMeshSupported() const noexcept;
  [[nodiscard]] inline bool refreshRateExtensionSupported() const noexcept;
  [[nodiscard]] inline bool instanceCreateInfoAndroidSupported() const noexcept;
  [[nodiscard]] inline bool alphaBlendCompositionSupported() const noexcept;

  void* nativeWindow_ = nullptr;
  bool resumed_ = false;
  bool sessionActive_ = false;

  std::vector<XrExtensionProperties> extensions_;
  std::vector<const char*> enabledExtensions_;

  XrInstanceProperties instanceProps_ = {
      .type = XR_TYPE_INSTANCE_PROPERTIES,
      .next = nullptr,
  };

  XrSystemProperties systemProps_ = {
      .type = XR_TYPE_SYSTEM_PROPERTIES,
      .next = nullptr,
  };

#if IGL_PLATFORM_ANDROID
  XrInstanceCreateInfoAndroidKHR instanceCreateInfoAndroid_ = {
      .type = XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR,
  };
#endif // IGL_PLATFORM_ANDROID

  std::unordered_set<std::string> supportedOptionalXrExtensions_;

  XrInstance instance_ = XR_NULL_HANDLE;
  XrSystemId systemId_ = XR_NULL_SYSTEM_ID;
  XrSession session_ = XR_NULL_HANDLE;

  bool useSinglePassStereo_ = true;
  bool additiveBlendingSupported_ = false;
  bool useQuadLayerComposition_ = false;

  XrViewConfigurationProperties viewConfigProps_ = {.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
  std::array<XrViewConfigurationView, XrComposition::kNumViews> viewports_{};
  std::array<XrView, XrComposition::kNumViews> views_{};
  std::array<XrPosef, XrComposition::kNumViews> viewStagePoses_{};
  std::array<glm::mat4, XrComposition::kNumViews> viewTransforms_{};
  std::array<glm::vec3, XrComposition::kNumViews> cameraPositions_{};

  std::vector<std::unique_ptr<XrComposition>> compositionLayers_;

  XrSpace headSpace_ = XR_NULL_HANDLE;
  XrSpace currentSpace_ = XR_NULL_HANDLE;
  bool stageSpaceSupported_ = false;

  std::unique_ptr<XrPassthrough> passthrough_;
  std::unique_ptr<XrHands> hands_;
  std::unique_ptr<XrRefreshRate> refreshRate_;

  std::unique_ptr<impl::XrAppImpl> impl_;

  bool initialized_ = false;

  std::shared_ptr<igl::shell::Platform> platform_;
  std::unique_ptr<igl::shell::RenderSession> renderSession_;

  std::unique_ptr<igl::shell::ShellParams> shellParams_;
};
} // namespace igl::shell::openxr
