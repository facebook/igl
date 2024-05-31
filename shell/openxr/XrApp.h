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

#include <shell/openxr/XrPlatform.h>

#include <glm/glm.hpp>

#include <igl/IGL.h>
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>

struct android_app;
struct AAssetManager;

// forward declarations
namespace igl::shell::openxr {
class XrSwapchainProvider;
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
    enum RefreshRateMode {
      UseDefault,
      UseMaxRefreshRate,
      UseSpecificRefreshRate,
    };
    RefreshRateMode refreshRateMode_ = RefreshRateMode::UseDefault;
    float desiredSpecificRefreshRate_ = 90.0f;
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
  bool createPassthrough();
  bool createHandsTracking();
  void updateHandMeshes();
  void updateHandTracking();
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

  float getCurrentRefreshRate();
  float getMaxRefreshRate();
  bool setRefreshRate(float refreshRate);
  void setMaxRefreshRate();
  bool isRefreshRateSupported(float refreshRate);
  const std::vector<float>& getSupportedRefreshRates();

 private:
  static constexpr uint32_t kNumViews = 2; // 2 for stereo

  void queryCurrentRefreshRate();
  void querySupportedRefreshRates();
  void setupProjectionAndDepth(std::vector<XrCompositionLayerProjectionView>& projectionViews,
                               std::vector<XrCompositionLayerDepthInfoKHR>& depthInfos);
  void endFrameQuadLayerComposition(XrFrameState frameState);

  [[nodiscard]] inline bool passthroughSupported() const noexcept;
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

  XrViewConfigurationProperties viewConfigProps_ = {.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
  std::array<XrViewConfigurationView, kNumViews> viewports_;
  std::array<XrView, kNumViews> views_;
  std::array<XrPosef, kNumViews> viewStagePoses_;
  std::array<glm::mat4, kNumViews> viewTransforms_;
  std::array<glm::vec3, kNumViews> cameraPositions_;

  bool useSinglePassStereo_ = true;
  bool useQuadLayerComposition_ = false;
  uint32_t numQuadLayersPerView_ = 1;
  igl::shell::QuadLayerParams quadLayersParams_;

  // If useSinglePassStereo_ is true, only one XrSwapchainProvider will be created.
  std::vector<std::unique_ptr<XrSwapchainProvider>> swapchainProviders_;

  XrSpace headSpace_ = XR_NULL_HANDLE;
  XrSpace currentSpace_ = XR_NULL_HANDLE;
  bool stageSpaceSupported_ = false;

  bool additiveBlendingSupported_ = false;

  XrPassthroughFB passthrough_ = XR_NULL_HANDLE;
  XrPassthroughLayerFB passthrougLayer_ = XR_NULL_HANDLE;

  PFN_xrCreatePassthroughFB xrCreatePassthroughFB_ = nullptr;
  PFN_xrDestroyPassthroughFB xrDestroyPassthroughFB_ = nullptr;
  PFN_xrPassthroughStartFB xrPassthroughStartFB_ = nullptr;
  PFN_xrCreatePassthroughLayerFB xrCreatePassthroughLayerFB_ = nullptr;
  PFN_xrDestroyPassthroughLayerFB xrDestroyPassthroughLayerFB_ = nullptr;
  PFN_xrPassthroughLayerSetStyleFB xrPassthroughLayerSetStyleFB_ = nullptr;

  PFN_xrCreateHandTrackerEXT xrCreateHandTrackerEXT_ = nullptr;
  PFN_xrDestroyHandTrackerEXT xrDestroyHandTrackerEXT_ = nullptr;
  PFN_xrLocateHandJointsEXT xrLocateHandJointsEXT_ = nullptr;
  PFN_xrGetHandMeshFB xrGetHandMeshFB_ = nullptr;

  XrHandTrackerEXT leftHandTracker_ = XR_NULL_HANDLE;
  XrHandTrackerEXT rightHandTracker_ = XR_NULL_HANDLE;

  std::vector<float> supportedRefreshRates_;
  float currentRefreshRate_ = 0.0f;

  PFN_xrGetDisplayRefreshRateFB xrGetDisplayRefreshRateFB_ = nullptr;
  PFN_xrEnumerateDisplayRefreshRatesFB xrEnumerateDisplayRefreshRatesFB_ = nullptr;
  PFN_xrRequestDisplayRefreshRateFB xrRequestDisplayRefreshRateFB_ = nullptr;

  std::unique_ptr<impl::XrAppImpl> impl_;

  bool initialized_ = false;

  std::shared_ptr<igl::shell::Platform> platform_;
  std::unique_ptr<igl::shell::RenderSession> renderSession_;

  std::unique_ptr<igl::shell::ShellParams> shellParams_;
};
} // namespace igl::shell::openxr
