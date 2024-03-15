/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/openxr/XrApp.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <string>

#include <android/asset_manager.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#include <openxr/openxr.h>

#include <igl/Common.h>
#if USE_VULKAN_BACKEND
#include <igl/vulkan/Common.h>

#ifndef XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_VULKAN
#endif
#endif // USE_VULKAN_BACKEND
#if USE_OPENGL_BACKEND
#if !defined(XR_USE_GRAPHICS_API_OPENGL_ES) && defined(IGL_CMAKE_BUILD)
#define XR_USE_GRAPHICS_API_OPENGL_ES
#endif
#endif // USE_OPENGL_BACKEND
#include <openxr/openxr_platform.h>

#include <glm/gtc/type_ptr.hpp>
#include <xr_linear.h>

#include <shell/shared/fileLoader/android/FileLoaderAndroid.h>
#include <shell/shared/imageLoader/android/ImageLoaderAndroid.h>
#include <shell/shared/platform/android/PlatformAndroid.h>
#include <shell/shared/renderSession/AppParams.h>
#include <shell/shared/renderSession/DefaultSession.h>
#include <shell/shared/renderSession/ShellParams.h>

#include <shell/openxr/XrLog.h>
#include <shell/openxr/XrSwapchainProvider.h>
#include <shell/openxr/impl/XrAppImpl.h>
#include <shell/openxr/impl/XrSwapchainProviderImpl.h>

namespace igl::shell::openxr {
constexpr auto kAppName = "IGL Shell OpenXR";
constexpr auto kEngineName = "IGL";
constexpr auto kSupportedViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

namespace {
inline glm::quat glmQuatFromXrQuat(const XrQuaternionf& quat) noexcept {
  return glm::quat(quat.w, quat.x, quat.y, quat.z);
}

inline glm::vec4 glmVecFromXrVec(const XrVector4f& vec) noexcept {
  return glm::vec4(vec.x, vec.y, vec.z, vec.w);
}

inline glm::vec4 glmVecFromXrVec(const XrVector4sFB& vec) noexcept {
  return glm::vec4(vec.x, vec.y, vec.z, vec.w);
}

inline glm::vec3 glmVecFromXrVec(const XrVector3f& vec) noexcept {
  return glm::vec3(vec.x, vec.y, vec.z);
}

inline glm::vec2 glmVecFromXrVec(const XrVector2f& vec) noexcept {
  return glm::vec2(vec.x, vec.y);
}

inline Pose poseFromXrPose(const XrPosef& pose) noexcept {
  return Pose{
      /*.orientation = */ glmQuatFromXrQuat(pose.orientation),
      /*.position = */ glmVecFromXrVec(pose.position),
  };
}

inline int64_t currentTimeInNs() {
  const auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
}
} // namespace

XrApp::XrApp(std::unique_ptr<impl::XrAppImpl>&& impl) :
  requiredExtensions_({
#if USE_VULKAN_BACKEND
    XR_KHR_VULKAN_ENABLE_EXTENSION_NAME,
#endif // USE_VULKAN_BACKEND
#if !defined(XR_USE_PLATFORM_MACOS) && !defined(IGL_CMAKE_BUILD)
        XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME,
#endif
  }),
  impl_(std::move(impl)),
  shellParams_(std::make_unique<ShellParams>()) {
  viewports_.fill({XR_TYPE_VIEW_CONFIGURATION_VIEW});
  views_.fill({XR_TYPE_VIEW});
#ifdef USE_COMPOSITION_LAYER_QUAD
  useQuadLayerComposition_ = true;
#endif
}

XrApp::~XrApp() {
  if (!initialized_)
    return;

  swapchainProviders_.clear();

  if (leftHandTracker_ != XR_NULL_HANDLE) {
    xrDestroyHandTrackerEXT_(leftHandTracker_);
  }
  if (rightHandTracker_ != XR_NULL_HANDLE) {
    xrDestroyHandTrackerEXT_(rightHandTracker_);
  }
  if (passthrough_ != XR_NULL_HANDLE) {
    xrDestroyPassthroughFB_(passthrough_);
  }
  xrDestroySpace(currentSpace_);
  xrDestroySpace(headSpace_);
  xrDestroySession(session_);
  xrDestroyInstance(instance_);
}

XrInstance XrApp::instance() const {
  return instance_;
}

XrSession XrApp::session() const {
  return session_;
}

bool XrApp::checkExtensions() {
  // Check that the extensions required are present.
  XrResult result;
  PFN_xrEnumerateInstanceExtensionProperties xrEnumerateInstanceExtensionProperties;
  XR_CHECK(result =
               xrGetInstanceProcAddr(XR_NULL_HANDLE,
                                     "xrEnumerateInstanceExtensionProperties",
                                     (PFN_xrVoidFunction*)&xrEnumerateInstanceExtensionProperties));
  if (result != XR_SUCCESS) {
    IGL_LOG_ERROR("Failed to get xrEnumerateInstanceExtensionProperties function pointer.");
    return false;
  }

  uint32_t numExtensions = 0;
  XR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, 0, &numExtensions, nullptr));
  IGL_LOG_INFO("xrEnumerateInstanceExtensionProperties found %u extension(s).", numExtensions);

  extensions_.resize(numExtensions, {XR_TYPE_EXTENSION_PROPERTIES});

  XR_CHECK(xrEnumerateInstanceExtensionProperties(
      NULL, numExtensions, &numExtensions, extensions_.data()));
  for (uint32_t i = 0; i < numExtensions; i++) {
    IGL_LOG_INFO("Extension #%d = '%s'.", i, extensions_[i].extensionName);
  }

  auto requiredExtensionsImpl_ = impl_->getXrRequiredExtensions();
  requiredExtensions_.insert(std::end(requiredExtensions_),
                             std::begin(requiredExtensionsImpl_),
                             std::end(requiredExtensionsImpl_));

  for (auto& requiredExtension : requiredExtensions_) {
    auto it = std::find_if(std::begin(extensions_),
                           std::end(extensions_),
                           [&requiredExtension](const XrExtensionProperties& extension) {
                             return strcmp(extension.extensionName, requiredExtension) == 0;
                           });
    if (it == std::end(extensions_)) {
      IGL_LOG_ERROR("Extension %s is required.", requiredExtension);
      return false;
    }
  }

  auto checkExtensionSupported = [this](const char* name) {
    return std::any_of(std::begin(extensions_),
                       std::end(extensions_),
                       [&](const XrExtensionProperties& extension) {
                         return strcmp(extension.extensionName, name) == 0;
                       });
  };

  passthroughSupported_ = checkExtensionSupported(XR_FB_PASSTHROUGH_EXTENSION_NAME);
  IGL_LOG_INFO("Passthrough is %s", passthroughSupported_ ? "supported" : "not supported");

  auto checkNeedRequiredExtension = [this](const char* name) {
    return std::find_if(std::begin(requiredExtensions_),
                        std::end(requiredExtensions_),
                        [&](const char* extensionName) {
                          return strcmp(extensionName, name) == 0;
                        }) == std::end(requiredExtensions_);
  };

  // Add passthough extension if supported.
  if (passthroughSupported_ && checkNeedRequiredExtension(XR_FB_PASSTHROUGH_EXTENSION_NAME)) {
    requiredExtensions_.push_back(XR_FB_PASSTHROUGH_EXTENSION_NAME);
  }

  handsTrackingSupported_ = checkExtensionSupported(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
  IGL_LOG_INFO("Hands tracking is %s", handsTrackingSupported_ ? "supported" : "not supported");

  handsTrackingMeshSupported_ = checkExtensionSupported(XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME);
  IGL_LOG_INFO("Hands tracking mesh is %s",
               handsTrackingMeshSupported_ ? "supported" : "not supported");

  // Add hands tracking extension if supported.
  if (handsTrackingSupported_ && checkNeedRequiredExtension(XR_EXT_HAND_TRACKING_EXTENSION_NAME)) {
    requiredExtensions_.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);

    if (handsTrackingMeshSupported_ &&
        checkNeedRequiredExtension(XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME)) {
      requiredExtensions_.push_back(XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME);
    }
  }

  refreshRateExtensionSupported_ = checkExtensionSupported(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
  IGL_LOG_INFO("RefreshRate is %s", refreshRateExtensionSupported_ ? "supported" : "not supported");

  if (refreshRateExtensionSupported_ && checkNeedRequiredExtension(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME)) {
    requiredExtensions_.push_back(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
  }

  compositionLayerSettingsSupported_ = checkExtensionSupported(XR_FB_COMPOSITION_LAYER_SETTINGS_EXTENSION_NAME);
  IGL_LOG_INFO("Composition Layer Settings are %s", compositionLayerSettingsSupported_ ? "supported" : "not supported");

  if (compositionLayerSettingsSupported_ && checkNeedRequiredExtension(XR_FB_COMPOSITION_LAYER_SETTINGS_EXTENSION_NAME)) {
    requiredExtensions_.push_back(XR_FB_COMPOSITION_LAYER_SETTINGS_EXTENSION_NAME);
  }

    return true;
}

bool XrApp::createInstance() {
  XrApplicationInfo appInfo = {};
  strcpy(appInfo.applicationName, kAppName);
  appInfo.applicationVersion = 0;
  strcpy(appInfo.engineName, kEngineName);
  appInfo.engineVersion = 0;
  appInfo.apiVersion = XR_CURRENT_API_VERSION;

  XrInstanceCreateInfo instanceCreateInfo = {
      .type = XR_TYPE_INSTANCE_CREATE_INFO,
      .next = impl_->getInstanceCreateExtension(),
      .createFlags = 0,
      .applicationInfo = appInfo,
      .enabledApiLayerCount = 0,
      .enabledApiLayerNames = nullptr,
      .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions_.size()),
      .enabledExtensionNames = requiredExtensions_.data(),
  };

  XrResult initResult;
  XR_CHECK(initResult = xrCreateInstance(&instanceCreateInfo, &instance_));
  if (initResult != XR_SUCCESS) {
    IGL_LOG_ERROR("Failed to create XR instance: %d.", initResult);
    return false;
  }

  XR_CHECK(xrGetInstanceProperties(instance_, &instanceProps_));
  IGL_LOG_INFO("Runtime %s: Version : %u.%u.%u",
               instanceProps_.runtimeName,
               XR_VERSION_MAJOR(instanceProps_.runtimeVersion),
               XR_VERSION_MINOR(instanceProps_.runtimeVersion),
               XR_VERSION_PATCH(instanceProps_.runtimeVersion));

  if (passthroughSupported_) {
    XR_CHECK(xrGetInstanceProcAddr(
        instance_, "xrCreatePassthroughFB", (PFN_xrVoidFunction*)(&xrCreatePassthroughFB_)));
    XR_CHECK(xrGetInstanceProcAddr(
        instance_, "xrDestroyPassthroughFB", (PFN_xrVoidFunction*)(&xrDestroyPassthroughFB_)));
    XR_CHECK(xrGetInstanceProcAddr(
        instance_, "xrPassthroughStartFB", (PFN_xrVoidFunction*)(&xrPassthroughStartFB_)));
    XR_CHECK(xrGetInstanceProcAddr(instance_,
                                   "xrCreatePassthroughLayerFB",
                                   (PFN_xrVoidFunction*)(&xrCreatePassthroughLayerFB_)));
    XR_CHECK(xrGetInstanceProcAddr(instance_,
                                   "xrDestroyPassthroughLayerFB",
                                   (PFN_xrVoidFunction*)(&xrDestroyPassthroughLayerFB_)));
    XR_CHECK(xrGetInstanceProcAddr(instance_,
                                   "xrPassthroughLayerSetStyleFB",
                                   (PFN_xrVoidFunction*)(&xrPassthroughLayerSetStyleFB_)));
  }

  if (handsTrackingSupported_) {
    XR_CHECK(xrGetInstanceProcAddr(
        instance_, "xrCreateHandTrackerEXT", (PFN_xrVoidFunction*)(&xrCreateHandTrackerEXT_)));
    IGL_ASSERT(xrCreateHandTrackerEXT_ != nullptr);
    XR_CHECK(xrGetInstanceProcAddr(
        instance_, "xrDestroyHandTrackerEXT", (PFN_xrVoidFunction*)(&xrDestroyHandTrackerEXT_)));
    IGL_ASSERT(xrDestroyHandTrackerEXT_ != nullptr);
    XR_CHECK(xrGetInstanceProcAddr(
        instance_, "xrLocateHandJointsEXT", (PFN_xrVoidFunction*)(&xrLocateHandJointsEXT_)));
    IGL_ASSERT(xrLocateHandJointsEXT_ != nullptr);
    if (handsTrackingMeshSupported_) {
      XR_CHECK(xrGetInstanceProcAddr(
          instance_, "xrGetHandMeshFB", (PFN_xrVoidFunction*)(&xrGetHandMeshFB_)));
      IGL_ASSERT(xrGetHandMeshFB_ != nullptr);
    }
  }

  if (refreshRateExtensionSupported_) {
    XR_CHECK(xrGetInstanceProcAddr(
            instance_, "xrGetDisplayRefreshRateFB", (PFN_xrVoidFunction*)(&xrGetDisplayRefreshRateFB_)));
    IGL_ASSERT(xrGetDisplayRefreshRateFB_ != nullptr);
    XR_CHECK(xrGetInstanceProcAddr(
            instance_, "xrEnumerateDisplayRefreshRatesFB", (PFN_xrVoidFunction*)(&xrEnumerateDisplayRefreshRatesFB_)));
    IGL_ASSERT(xrEnumerateDisplayRefreshRatesFB_ != nullptr);
    XR_CHECK(xrGetInstanceProcAddr(
            instance_, "xrRequestDisplayRefreshRateFB", (PFN_xrVoidFunction*)(&xrRequestDisplayRefreshRateFB_)));
    IGL_ASSERT(xrRequestDisplayRefreshRateFB_ != nullptr);
}

  return true;
} // namespace igl::shell::openxr

bool XrApp::createSystem() {
  XrSystemGetInfo systemGetInfo = {
      .type = XR_TYPE_SYSTEM_GET_INFO,
      .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,
  };

  XrResult result;
  XR_CHECK(result = xrGetSystem(instance_, &systemGetInfo, &systemId_));
  if (result != XR_SUCCESS) {
    IGL_LOG_ERROR("Failed to get system.");
    return false;
  }

  XR_CHECK(xrGetSystemProperties(instance_, systemId_, &systemProps_));

  IGL_LOG_INFO(
      "System Properties: Name=%s VendorId=%x", systemProps_.systemName, systemProps_.vendorId);
  IGL_LOG_INFO("System Graphics Properties: MaxWidth=%d MaxHeight=%d MaxLayers=%d",
               systemProps_.graphicsProperties.maxSwapchainImageWidth,
               systemProps_.graphicsProperties.maxSwapchainImageHeight,
               systemProps_.graphicsProperties.maxLayerCount);
  IGL_LOG_INFO("System Tracking Properties: OrientationTracking=%s PositionTracking=%s",
               systemProps_.trackingProperties.orientationTracking ? "True" : "False",
               systemProps_.trackingProperties.positionTracking ? "True" : "False");
  return true;
}

bool XrApp::createPassthrough() {
  if (!passthroughSupported_) {
      return false;
  }
  XrPassthroughCreateInfoFB passthroughInfo{XR_TYPE_PASSTHROUGH_CREATE_INFO_FB};
  passthroughInfo.next = nullptr;
  passthroughInfo.flags = 0u;

  XrResult result;
  XR_CHECK(result = xrCreatePassthroughFB_(session_, &passthroughInfo, &passthrough_));
  if (result != XR_SUCCESS) {
    IGL_LOG_ERROR("xrCreatePassthroughFB failed.");
    return false;
  }

  XrPassthroughLayerCreateInfoFB layerInfo{XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB};
  layerInfo.next = nullptr;
  layerInfo.passthrough = passthrough_;
  layerInfo.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;
  layerInfo.flags = XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB;
  XR_CHECK(result = xrCreatePassthroughLayerFB_(session_, &layerInfo, &passthrougLayer_));
  if (result != XR_SUCCESS) {
    IGL_LOG_ERROR("xrCreatePassthroughLayerFB failed.");
    return false;
  }

  XrPassthroughStyleFB style{XR_TYPE_PASSTHROUGH_STYLE_FB};
  style.next = nullptr;
  style.textureOpacityFactor = 1.0f;
  style.edgeColor = {0.0f, 0.0f, 0.0f, 0.0f};
  XR_CHECK(result = xrPassthroughLayerSetStyleFB_(passthrougLayer_, &style));
  if (result != XR_SUCCESS) {
    IGL_LOG_ERROR("xrPassthroughLayerSetStyleFB failed.");
    return false;
  }

  XR_CHECK(result = xrPassthroughStartFB_(passthrough_));
  if (result != XR_SUCCESS) {
    IGL_LOG_ERROR("xrPassthroughStartFB failed.");
    return false;
  }
  return true;
}

bool XrApp::createHandsTracking() {
  if (!handsTrackingSupported_) {
    return false;
  }
  XrHandTrackerCreateInfoEXT createInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
  createInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
  createInfo.hand = XR_HAND_LEFT_EXT;

  std::array<XrHandTrackingDataSourceEXT, 2> dataSources = {
      XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT,
      XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT,
  };

  XrHandTrackingDataSourceInfoEXT dataSourceInfo{XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT};
  dataSourceInfo.requestedDataSourceCount = static_cast<uint32_t>(dataSources.size());
  dataSourceInfo.requestedDataSources = dataSources.data();

  createInfo.next = &dataSourceInfo;

  XrResult result;
  XR_CHECK(result = xrCreateHandTrackerEXT_(session_, &createInfo, &leftHandTracker_));
  if (result != XR_SUCCESS) {
    IGL_LOG_ERROR("xrCreateHandTrackerEXT (left hand) failed.");
    return false;
  }

  createInfo.hand = XR_HAND_RIGHT_EXT;
  XR_CHECK(result = xrCreateHandTrackerEXT_(session_, &createInfo, &rightHandTracker_));
  if (result != XR_SUCCESS) {
    IGL_LOG_ERROR("xrCreateHandTrackerEXT (right hand) failed.");
    return false;
  }

  return true;
}

void XrApp::updateHandMeshes() {
  if (!handsTrackingMeshSupported_) {
    return;
  }
  auto& handMeshes = shellParams_->handMeshes;

  XrResult result;
  XrHandTrackerEXT trackers[] = {leftHandTracker_, rightHandTracker_};
  for (uint8_t i = 0; i < 2; ++i) {
    XrHandTrackingMeshFB mesh{XR_TYPE_HAND_TRACKING_MESH_FB};
    XR_CHECK(result = xrGetHandMeshFB_(trackers[i], &mesh));
    if (result != XR_SUCCESS) {
      continue;
    }

    IGL_ASSERT(mesh.jointCountOutput <= XR_HAND_JOINT_COUNT_EXT);
    XrPosef jointBindPoses[XR_HAND_JOINT_COUNT_EXT]{};
    XrHandJointEXT jointParents[XR_HAND_JOINT_COUNT_EXT]{};
    float jointRadii[XR_HAND_JOINT_COUNT_EXT]{};

    mesh.jointCapacityInput = mesh.jointCountOutput;
    mesh.vertexCapacityInput = mesh.vertexCountOutput;
    mesh.indexCapacityInput = mesh.indexCountOutput;

    std::vector<XrVector3f> vertexPositions(mesh.vertexCapacityInput);
    std::vector<XrVector3f> vertexNormals(mesh.vertexCapacityInput);
    std::vector<XrVector2f> vertexUVs(mesh.vertexCapacityInput);
    std::vector<XrVector4sFB> vertexBlendIndices(mesh.vertexCapacityInput);
    std::vector<XrVector4f> vertexBlendWeights(mesh.vertexCapacityInput);

    handMeshes[i].indices.resize(mesh.indexCapacityInput);

    mesh.jointBindPoses = jointBindPoses;
    mesh.jointParents = jointParents;
    mesh.jointRadii = jointRadii;
    mesh.vertexPositions = vertexPositions.data();
    mesh.vertexNormals = vertexNormals.data();
    mesh.vertexUVs = vertexUVs.data();
    mesh.vertexBlendIndices = vertexBlendIndices.data();
    mesh.vertexBlendWeights = vertexBlendWeights.data();
    mesh.indices = handMeshes[i].indices.data();

    XR_CHECK(result = xrGetHandMeshFB_(trackers[i], &mesh));
    if (result != XR_SUCCESS) {
      continue;
    }

    handMeshes[i].vertexCountOutput = mesh.vertexCountOutput;
    handMeshes[i].indexCountOutput = mesh.indexCountOutput;
    handMeshes[i].jointCountOutput = mesh.jointCountOutput;
    handMeshes[i].vertexPositions.reserve(mesh.vertexCountOutput);
    handMeshes[i].vertexNormals.reserve(mesh.vertexCountOutput);
    handMeshes[i].vertexUVs.reserve(mesh.vertexCountOutput);
    handMeshes[i].vertexBlendIndices.reserve(mesh.vertexCountOutput);
    handMeshes[i].vertexBlendWeights.reserve(mesh.vertexCountOutput);
    handMeshes[i].jointBindPoses.reserve(mesh.jointCountOutput);

    for (uint32_t j = 0; j < mesh.vertexCountOutput; ++j) {
      handMeshes[i].vertexPositions.emplace_back(glmVecFromXrVec(mesh.vertexPositions[j]));
      handMeshes[i].vertexUVs.emplace_back(glmVecFromXrVec(mesh.vertexUVs[j]));
      handMeshes[i].vertexNormals.emplace_back(glmVecFromXrVec(mesh.vertexNormals[j]));
      handMeshes[i].vertexBlendIndices.emplace_back(glmVecFromXrVec(mesh.vertexBlendIndices[j]));
      handMeshes[i].vertexBlendWeights.emplace_back(glmVecFromXrVec(mesh.vertexBlendWeights[j]));
    }

    for (uint32_t j = 0; j < mesh.jointCountOutput; ++j) {
      handMeshes[i].jointBindPoses.emplace_back(poseFromXrPose(mesh.jointBindPoses[j]));
    }
  }
}

void XrApp::updateHandTracking() {
  if (!handsTrackingSupported_) {
    return;
  }
  auto& handTracking = shellParams_->handTracking;

  XrResult result;
  XrHandTrackerEXT trackers[] = {leftHandTracker_, rightHandTracker_};
  for (uint8_t i = 0; i < 2; ++i) {
    XrHandJointLocationEXT jointLocations[XR_HAND_JOINT_COUNT_EXT];
    XrHandJointVelocityEXT jointVelocities[XR_HAND_JOINT_COUNT_EXT];

    XrHandJointVelocitiesEXT velocities{.type = XR_TYPE_HAND_JOINT_VELOCITIES_EXT,
                                        .next = nullptr,
                                        .jointCount = XR_HAND_JOINT_COUNT_EXT,
                                        .jointVelocities = jointVelocities};

    XrHandJointLocationsEXT locations{.type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
                                      .next = &velocities,
                                      .jointCount = XR_HAND_JOINT_COUNT_EXT,
                                      .jointLocations = jointLocations};

    XrHandJointsMotionRangeInfoEXT motionRangeInfo{XR_TYPE_HAND_JOINTS_MOTION_RANGE_INFO_EXT};
    motionRangeInfo.handJointsMotionRange =
        XR_HAND_JOINTS_MOTION_RANGE_CONFORMING_TO_CONTROLLER_EXT;

    const XrHandJointsLocateInfoEXT locateInfo{.type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
                                               .next = &motionRangeInfo,
                                               .baseSpace = currentSpace_,
                                               .time = currentTimeInNs()};

    handTracking[i].jointPose.resize(XR_HAND_JOINT_COUNT_EXT);
    handTracking[i].jointVelocity.resize(XR_HAND_JOINT_COUNT_EXT);
    handTracking[i].isJointTracked.resize(XR_HAND_JOINT_COUNT_EXT);

    XR_CHECK(result = xrLocateHandJointsEXT_(trackers[i], &locateInfo, &locations));
    if (result != XR_SUCCESS) {
      for (size_t jointIndex = 0; jointIndex < XR_HAND_JOINT_COUNT_EXT; ++jointIndex) {
        handTracking[i].isJointTracked[jointIndex] = false;
      }
      continue;
    }

    if (!locations.isActive) {
      for (size_t jointIndex = 0; jointIndex < XR_HAND_JOINT_COUNT_EXT; ++jointIndex) {
        handTracking[i].isJointTracked[jointIndex] = false;
      }
      continue;
    }

    constexpr XrSpaceLocationFlags isValid =
        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
    for (size_t jointIndex = 0; jointIndex < XR_HAND_JOINT_COUNT_EXT; ++jointIndex) {
      if ((jointLocations[jointIndex].locationFlags & isValid) != 0) {
        handTracking[i].jointPose[jointIndex] = poseFromXrPose(jointLocations[jointIndex].pose);
        handTracking[i].jointVelocity[jointIndex].linear =
            glmVecFromXrVec(jointVelocities[jointIndex].linearVelocity);
        handTracking[i].jointVelocity[jointIndex].angular =
            glmVecFromXrVec(jointVelocities[jointIndex].angularVelocity);
        handTracking[i].isJointTracked[jointIndex] = true;
      } else {
        handTracking[i].isJointTracked[jointIndex] = false;
      }
    }
  }
}

bool XrApp::enumerateViewConfigurations() {
  uint32_t numViewConfigs = 0;
  XR_CHECK(xrEnumerateViewConfigurations(instance_, systemId_, 0, &numViewConfigs, nullptr));

  std::vector<XrViewConfigurationType> viewConfigTypes(numViewConfigs);
  XR_CHECK(xrEnumerateViewConfigurations(
      instance_, systemId_, numViewConfigs, &numViewConfigs, viewConfigTypes.data()));

  IGL_LOG_INFO("Available Viewport Configuration Types: %d", numViewConfigs);
  auto foundViewConfig = false;
  for (auto& viewConfigType : viewConfigTypes) {
    IGL_LOG_INFO("View configuration type %d : %s",
                 viewConfigType,
                 viewConfigType == kSupportedViewConfigType ? "Selected" : "");

    if (viewConfigType != kSupportedViewConfigType) {
      continue;
    }

    // Check properties
    XrViewConfigurationProperties viewConfigProps = {XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
    XR_CHECK(
        xrGetViewConfigurationProperties(instance_, systemId_, viewConfigType, &viewConfigProps));
    IGL_LOG_INFO("FovMutable=%s ConfigurationType %d",
                 viewConfigProps.fovMutable ? "true" : "false",
                 viewConfigProps.viewConfigurationType);

    // Check views
    uint32_t numViewports = 0;
    XR_CHECK(xrEnumerateViewConfigurationViews(
        instance_, systemId_, viewConfigType, 0, &numViewports, nullptr));

    if (!IGL_VERIFY(numViewports == kNumViews)) {
      IGL_LOG_ERROR(
          "numViewports must be %d. Make sure XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO is used.",
          kNumViews);
      return false;
    }

    XR_CHECK(xrEnumerateViewConfigurationViews(
        instance_, systemId_, viewConfigType, numViewports, &numViewports, viewports_.data()));

    for (auto& view : viewports_) {
      (void)view; // doesn't compile in release for unused variable
      IGL_LOG_INFO("Viewport [%d]: Recommended Width=%d Height=%d SampleCount=%d",
                   view,
                   view.recommendedImageRectWidth,
                   view.recommendedImageRectHeight,
                   view.recommendedSwapchainSampleCount);

      IGL_LOG_INFO("Viewport [%d]: Max Width=%d Height=%d SampleCount=%d",
                   view,
                   view.maxImageRectWidth,
                   view.maxImageRectHeight,
                   view.maxSwapchainSampleCount);
    }

    viewConfigProps_ = viewConfigProps;

    foundViewConfig = true;

    break;
  }

  IGL_ASSERT_MSG(
      foundViewConfig, "XrViewConfigurationType %d not found.", kSupportedViewConfigType);

  return true;
}

void XrApp::enumerateReferenceSpaces() {
  uint32_t numRefSpaceTypes = 0;
  XR_CHECK(xrEnumerateReferenceSpaces(session_, 0, &numRefSpaceTypes, nullptr));

  std::vector<XrReferenceSpaceType> refSpaceTypes(numRefSpaceTypes);

  XR_CHECK(xrEnumerateReferenceSpaces(
      session_, numRefSpaceTypes, &numRefSpaceTypes, refSpaceTypes.data()));

  stageSpaceSupported_ =
      std::any_of(std::begin(refSpaceTypes), std::end(refSpaceTypes), [](const auto& type) {
        return type == XR_REFERENCE_SPACE_TYPE_STAGE;
      });
  IGL_LOG_INFO("OpenXR stage reference space is %s",
               stageSpaceSupported_ ? "supported" : "not supported");
}

void XrApp::enumerateBlendModes() {
  uint32_t numBlendModes = 0;
  XR_CHECK(xrEnumerateEnvironmentBlendModes(
      instance_, systemId_, kSupportedViewConfigType, 0, &numBlendModes, nullptr));

  std::vector<XrEnvironmentBlendMode> blendModes(numBlendModes);
  XR_CHECK(xrEnumerateEnvironmentBlendModes(instance_,
                                            systemId_,
                                            kSupportedViewConfigType,
                                            numBlendModes,
                                            &numBlendModes,
                                            blendModes.data()));

  additiveBlendingSupported_ =
      std::any_of(std::begin(blendModes), std::end(blendModes), [](const auto& type) {
        return type == XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
      });
  IGL_LOG_INFO("OpenXR additive blending %s",
               additiveBlendingSupported_ ? "supported" : "not supported");
}

void XrApp::createSwapchainProviders(const std::unique_ptr<igl::IDevice>& device) {
  const size_t numSwapchainProviders = useSinglePassStereo_ ? 1 : kNumViews;
  const size_t numViewsPerSwapchain = useSinglePassStereo_ ? kNumViews : 1;
  swapchainProviders_.reserve(numSwapchainProviders);

  for (size_t i = 0; i < numSwapchainProviders; i++) {
    swapchainProviders_.emplace_back(
        std::make_unique<XrSwapchainProvider>(impl_->createSwapchainProviderImpl(),
                                              platform_,
                                              session_,
                                              viewports_[i],
                                              numViewsPerSwapchain));
    swapchainProviders_.back()->initialize();
  }
}

bool XrApp::initialize(const struct android_app* app) {
  if (initialized_) {
    return false;
  }

  PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
  XR_CHECK(xrGetInstanceProcAddr(
      XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&xrInitializeLoaderKHR));
  if (xrInitializeLoaderKHR) {
    XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid = {
        XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR,
        nullptr,
        app->activity->vm,
        app->activity->clazz,
    };

    XR_CHECK(xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*)&loaderInitializeInfoAndroid));
  }

  if (!checkExtensions()) {
    return false;
  }

  XrInstanceCreateInfoAndroidKHR* instanceCreateInfoAndroid_ptr =
      (XrInstanceCreateInfoAndroidKHR*)impl_->getInstanceCreateExtension();

  if (instanceCreateInfoAndroid_ptr) {
    instanceCreateInfoAndroid_ptr->applicationVM = app->activity->vm;
    instanceCreateInfoAndroid_ptr->applicationActivity = app->activity->clazz;
  }

  if (!createInstance()) {
    return false;
  }

  if (!createSystem()) {
    return false;
  }

  if (!enumerateViewConfigurations()) {
    return false;
  }

  std::unique_ptr<igl::IDevice> device;
  device = impl_->initIGL(instance_, systemId_);
  if (!device) {
    IGL_LOG_ERROR("Failed to initialize IGL");
    return false;
  }

  useSinglePassStereo_ = useSinglePassStereo_ && device->hasFeature(igl::DeviceFeatures::Multiview);

  createShellSession(std::move(device), app->activity->assetManager);

  session_ = impl_->initXrSession(instance_, systemId_, platform_->getDevice());
  if (session_ == XR_NULL_HANDLE) {
    IGL_LOG_ERROR("Failed to initialize graphics system");
    return false;
  }

  // The following are initialization steps that happen after XrSession is created.
  enumerateReferenceSpaces();
  enumerateBlendModes();
  createSwapchainProviders(device);
  createSpaces();
  if (passthroughSupported_ && !createPassthrough()) {
    return false;
  }
  if (handsTrackingSupported_ && !createHandsTracking()) {
    return false;
  }
  if (refreshRateExtensionSupported_) {
    getCurrentRefreshRate();

    if (useMaxRefreshRate_) {
      setMaxRefreshRate();
    }
    else {
      setRefreshRate(desiredSpecificRefreshRate_);
    }
  }
  if (compositionLayerSettingsSupported_ && enableSharpeningAtStartup_) {
    setSharpeningEnabled(true);
    setSharpeningEnabled(false);
    setSharpeningEnabled(true);
    setSharpeningEnabled(false);
    setSharpeningEnabled(true);
  }
  updateHandMeshes();

  IGL_ASSERT(renderSession_ != nullptr);
  renderSession_->initialize();
  initialized_ = true;

  return initialized_;
}

void XrApp::createShellSession(std::unique_ptr<igl::IDevice> device, AAssetManager* assetMgr) {
  platform_ = std::make_shared<igl::shell::PlatformAndroid>(std::move(device));
  IGL_ASSERT(platform_ != nullptr);
  static_cast<igl::shell::ImageLoaderAndroid&>(platform_->getImageLoader())
      .setAssetManager(assetMgr);
  static_cast<igl::shell::FileLoaderAndroid&>(platform_->getFileLoader()).setAssetManager(assetMgr);
  renderSession_ = igl::shell::createDefaultRenderSession(platform_);
  shellParams_->shellControlsViewParams = true;
  shellParams_->renderMode = useSinglePassStereo_ ? RenderMode::SinglePassStereo
                                                  : RenderMode::DualPassStereo;
  shellParams_->viewParams.resize(useSinglePassStereo_ ? 2 : 1);
  renderSession_->setShellParams(*shellParams_);
}

void XrApp::createSpaces() {
  XrReferenceSpaceCreateInfo spaceCreateInfo = {
      XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
      nullptr,
      XR_REFERENCE_SPACE_TYPE_VIEW,
      {{0.0f, 0.0f, 0.0f, 1.0f}},
  };
  XR_CHECK(xrCreateReferenceSpace(session_, &spaceCreateInfo, &headSpace_));

#if USE_LOCAL_AR_SPACE
  spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
#else
  spaceCreateInfo.referenceSpaceType = stageSpaceSupported_ ? XR_REFERENCE_SPACE_TYPE_STAGE
                                                            : XR_REFERENCE_SPACE_TYPE_LOCAL;
#endif
  XR_CHECK(xrCreateReferenceSpace(session_, &spaceCreateInfo, &currentSpace_));
}

void XrApp::handleXrEvents() {
  XrEventDataBuffer eventDataBuffer = {};

  // Poll for events
  for (;;) {
    XrEventDataBaseHeader* baseEventHeader = (XrEventDataBaseHeader*)(&eventDataBuffer);
    baseEventHeader->type = XR_TYPE_EVENT_DATA_BUFFER;
    baseEventHeader->next = nullptr;
    XrResult res;
    XR_CHECK(res = xrPollEvent(instance_, &eventDataBuffer));
    if (res != XR_SUCCESS) {
      break;
    }

    switch (baseEventHeader->type) {
    case XR_TYPE_EVENT_DATA_EVENTS_LOST:
      IGL_LOG_INFO("xrPollEvent: received XR_TYPE_EVENT_DATA_EVENTS_LOST event");
      break;
    case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
      IGL_LOG_INFO("xrPollEvent: received XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING event");
      break;
    case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
      IGL_LOG_INFO("xrPollEvent: received XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED event");
      break;
    case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT: {
      const XrEventDataPerfSettingsEXT* perf_settings_event =
          (XrEventDataPerfSettingsEXT*)(baseEventHeader);
      (void)perf_settings_event; // suppress unused warning
      IGL_LOG_INFO(
          "xrPollEvent: received XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT event: type %d subdomain %d "
          ": level %d -> level %d",
          perf_settings_event->type,
          perf_settings_event->subDomain,
          perf_settings_event->fromLevel,
          perf_settings_event->toLevel);
    } break;
    case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
      IGL_LOG_INFO("xrPollEvent: received XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING event");
      break;
    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
      const XrEventDataSessionStateChanged* session_state_changed_event =
          (XrEventDataSessionStateChanged*)(baseEventHeader);
      IGL_LOG_INFO(
          "xrPollEvent: received XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: %d for session %p at "
          "time %lld",
          session_state_changed_event->state,
          (void*)session_state_changed_event->session,
          session_state_changed_event->time);

      switch (session_state_changed_event->state) {
      case XR_SESSION_STATE_READY:
      case XR_SESSION_STATE_STOPPING:
        handleSessionStateChanges(session_state_changed_event->state);
        break;
      default:
        break;
      }
    } break;
    default:
      IGL_LOG_INFO("xrPollEvent: Unknown event");
      break;
    }
  }
}

void XrApp::handleSessionStateChanges(XrSessionState state) {
  if (state == XR_SESSION_STATE_READY) {
#if !defined(IGL_CMAKE_BUILD)
    assert(resumed_);
#endif // IGL_CMAKE_BUILD
    assert(sessionActive_ == false);

    XrSessionBeginInfo sessionBeginInfo{
        XR_TYPE_SESSION_BEGIN_INFO,
        nullptr,
        viewConfigProps_.viewConfigurationType,
    };

    XrResult result;
    XR_CHECK(result = xrBeginSession(session_, &sessionBeginInfo));

    sessionActive_ = (result == XR_SUCCESS);
    IGL_LOG_INFO("XR session active");
  } else if (state == XR_SESSION_STATE_STOPPING) {
    assert(resumed_ == false);
    assert(sessionActive_);
    XR_CHECK(xrEndSession(session_));
    sessionActive_ = false;
    IGL_LOG_INFO("XR session inactive");
  }
}

XrFrameState XrApp::beginFrame() {
  XrFrameWaitInfo waitFrameInfo = {XR_TYPE_FRAME_WAIT_INFO};

  XrFrameState frameState = {XR_TYPE_FRAME_STATE};

  XR_CHECK(xrWaitFrame(session_, &waitFrameInfo, &frameState));

  XrFrameBeginInfo beginFrameInfo = {XR_TYPE_FRAME_BEGIN_INFO};

  XR_CHECK(xrBeginFrame(session_, &beginFrameInfo));

  XrSpaceLocation loc = {
      loc.type = XR_TYPE_SPACE_LOCATION,
  };
  XR_CHECK(xrLocateSpace(headSpace_, currentSpace_, frameState.predictedDisplayTime, &loc));
  XrPosef headPose = loc.pose;

  XrViewState viewState = {XR_TYPE_VIEW_STATE};

  XrViewLocateInfo projectionInfo = {
      XR_TYPE_VIEW_LOCATE_INFO,
      nullptr,
      viewConfigProps_.viewConfigurationType,
      frameState.predictedDisplayTime,
      headSpace_,
  };

  uint32_t numViews = views_.size();

  XR_CHECK(xrLocateViews(
      session_, &projectionInfo, &viewState, views_.size(), &numViews, views_.data()));

  for (size_t i = 0; i < kNumViews; i++) {
    XrPosef eyePose = views_[i].pose;
    XrPosef_Multiply(&viewStagePoses_[i], &headPose, &eyePose);
    XrPosef viewTransformXrPosef{};
    XrPosef_Invert(&viewTransformXrPosef, &viewStagePoses_[i]);
    XrMatrix4x4f xrMat4{};
    XrMatrix4x4f_CreateFromRigidTransform(&xrMat4, &viewTransformXrPosef);
    viewTransforms_[i] = glm::make_mat4(xrMat4.m);
    cameraPositions_[i] = glm::vec3(eyePose.position.x, eyePose.position.y, eyePose.position.z);
  }

  if (handsTrackingSupported_) {
    updateHandTracking();
  }

  return frameState;
}

namespace {
void copyFov(igl::shell::Fov& dst, const XrFovf& src) {
  dst.angleLeft = src.angleLeft;
  dst.angleRight = src.angleRight;
  dst.angleUp = src.angleUp;
  dst.angleDown = src.angleDown;
}
} // namespace

void XrApp::render() {
  if (useQuadLayerComposition_) {
    shellParams_->clearColorValue = igl::Color{0.0f, 0.0f, 0.0f, 0.0f};
  } else {
    shellParams_->clearColorValue.reset();
  }

  if (useSinglePassStereo_) {
    auto surfaceTextures = swapchainProviders_[0]->getSurfaceTextures();
    for (size_t j = 0; j < shellParams_->viewParams.size(); j++) {
      shellParams_->viewParams[j].viewMatrix = viewTransforms_[j];
      shellParams_->viewParams[j].cameraPosition = cameraPositions_[j];
      copyFov(shellParams_->viewParams[j].fov, views_[j].fov);
    }
    renderSession_->update(std::move(surfaceTextures));
    swapchainProviders_[0]->releaseSwapchainImages();
  } else {
    for (size_t i = 0; i < kNumViews; i++) {
      shellParams_->viewParams[0].viewMatrix = viewTransforms_[i];
      copyFov(shellParams_->viewParams[0].fov, views_[i].fov);
      auto surfaceTextures = swapchainProviders_[i]->getSurfaceTextures();
      renderSession_->update(surfaceTextures);
      swapchainProviders_[i]->releaseSwapchainImages();
    }
  }
}

void XrApp::endFrame(XrFrameState frameState) {
  const auto& appParams = renderSession_->appParams();

  std::array<XrCompositionLayerQuad, kNumViews> quadLayers{};
  if (useQuadLayerComposition_) {
    XrEyeVisibility eye = XR_EYE_VISIBILITY_LEFT;
    for (auto& layer : quadLayers) {
      layer.next = nullptr;
      layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
      layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
      layer.space = currentSpace_;
      layer.eyeVisibility = eye;
      memset(&layer.subImage, 0, sizeof(XrSwapchainSubImage));
#if USE_LOCAL_AR_SPACE
      layer.pose = {{0.f, 0.f, 0.f, 1.f}, {0.f, 0.f, -1.f}};
#else
      layer.pose = {{0.f, 0.f, 0.f, 1.f}, {0.f, 0.f, 0.f}};
#endif
      layer.size = {appParams.sizeX, appParams.sizeY};
      if (eye == XR_EYE_VISIBILITY_LEFT) {
        eye = XR_EYE_VISIBILITY_RIGHT;
      }
    }
  }

  std::array<XrCompositionLayerProjectionView, kNumViews> projectionViews{};
  std::array<XrCompositionLayerDepthInfoKHR, kNumViews> depthInfos{};

  for (size_t i = 0; i < kNumViews; i++) {
    depthInfos[i] = {
        XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR,
        nullptr,
    };
    projectionViews[i] = {
        XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
        &depthInfos[i],
        viewStagePoses_[i],
        views_[i].fov,
    };
    XrRect2Di imageRect = {{0, 0},
                           {
                               (int32_t)viewports_[i].recommendedImageRectWidth,
                               (int32_t)viewports_[i].recommendedImageRectHeight,
                           }};
    auto index = useSinglePassStereo_ ? static_cast<uint32_t>(i) : 0;
    projectionViews[i].subImage = {
        useSinglePassStereo_ ? swapchainProviders_[0]->colorSwapchain()
                             : swapchainProviders_[i]->colorSwapchain(),
        imageRect,
        index,
    };
    depthInfos[i].subImage = {
        useSinglePassStereo_ ? swapchainProviders_[0]->depthSwapchain()
                             : swapchainProviders_[i]->depthSwapchain(),
        imageRect,
        index,
    };
    depthInfos[i].minDepth = appParams.depthParams.minDepth;
    depthInfos[i].maxDepth = appParams.depthParams.maxDepth;
    depthInfos[i].nearZ = appParams.depthParams.nearZ;
    depthInfos[i].farZ = appParams.depthParams.farZ;
    if (useQuadLayerComposition_) {
      quadLayers[i].subImage = projectionViews[i].subImage;
    }
  }

  if (useQuadLayerComposition_) {
    std::array<const XrCompositionLayerBaseHeader*, kNumViews + 1> layers{};
    uint32_t layerIndex = 0;

    XrCompositionLayerPassthroughFB compositionLayer{XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB};
    if (passthroughSupported_) {
      compositionLayer.next = nullptr;
      compositionLayer.layerHandle = passthrougLayer_;
      layers[layerIndex++] = (const XrCompositionLayerBaseHeader*)&compositionLayer;
    }

    for (uint32_t i = 0; i < quadLayers.size(); i++) {
      IGL_ASSERT(layerIndex < layers.size());
      layers[layerIndex++] = (const XrCompositionLayerBaseHeader*)&quadLayers[i];
    }

    const XrFrameEndInfo endFrameInfo = {XR_TYPE_FRAME_END_INFO,
                                         nullptr,
                                         frameState.predictedDisplayTime,
                                         additiveBlendingSupported_
                                             ? XR_ENVIRONMENT_BLEND_MODE_ADDITIVE
                                             : XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
                                         layerIndex,
                                         layers.data()};
    XR_CHECK(xrEndFrame(session_, &endFrameInfo));
  } else {
    XrCompositionLayerProjection projection = {
        XR_TYPE_COMPOSITION_LAYER_PROJECTION,
        nullptr,
        XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT,
        currentSpace_,
        static_cast<uint32_t>(kNumViews),
        projectionViews.data(),
    };

    if (compositionLayerSettingsSupported_) {
      ((XrCompositionLayerBaseHeader*)&projection)->next = &compositionLayerSettings_;
    }

    const XrCompositionLayerBaseHeader* const layers[] = {
        (const XrCompositionLayerBaseHeader*)&projection,
    };
    const XrFrameEndInfo endFrameInfo = {XR_TYPE_FRAME_END_INFO,
                                         nullptr,
                                         frameState.predictedDisplayTime,
                                         additiveBlendingSupported_
                                             ? XR_ENVIRONMENT_BLEND_MODE_ADDITIVE
                                             : XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
                                         1,
                                         layers};
    XR_CHECK(xrEndFrame(session_, &endFrameInfo));
  }
}

void XrApp::update() {
  if (!initialized_ || !resumed_ || !sessionActive_) {
    return;
  }

  auto frameState = beginFrame();
  render();
  endFrame(frameState);
}

float XrApp::getCurrentRefreshRate(){
  if (!session_ || !refreshRateExtensionSupported_ || (currentRefreshRate_ > 0.0f)) {
    return currentRefreshRate_;
  }

  XrResult result = xrGetDisplayRefreshRateFB_(session_, &currentRefreshRate_);

  if (result == XR_SUCCESS) {
    IGL_LOG_INFO("getCurrentRefreshRate success, current Hz = %.2f.", currentRefreshRate_);
  }

  return currentRefreshRate_;
}

float XrApp::getMaxRefreshRate() {
  if (!session_ || !refreshRateExtensionSupported_) {
    return 0.0f;
  }

  const std::vector<float>& supportedRefreshRates = getSupportedRefreshRates();

  if (supportedRefreshRates.empty()) {
    return 0.0f;
  }

  const float maxRefreshRate = supportedRefreshRates.back();
  IGL_LOG_INFO("getMaxRefreshRate Hz = %.2f.",maxRefreshRate);
  return maxRefreshRate;
}

bool XrApp::setRefreshRate(const float refreshRate) {
  if (!session_ || !refreshRateExtensionSupported_
  || (refreshRate == currentRefreshRate_)
  || !isRefreshRateSupported(refreshRate)) {
    return false;
  }

  XrResult result = xrRequestDisplayRefreshRateFB_(session_, refreshRate);

  if (result == XR_SUCCESS) {
    IGL_LOG_INFO("setRefreshRate SUCCESS, changed from %.2f Hz to %.2f Hz", currentRefreshRate_, refreshRate);
    currentRefreshRate_ = refreshRate;

    return true;
  }

  return false;
}

void XrApp::setMaxRefreshRate() {
  if (!session_ || !refreshRateExtensionSupported_) {
    return;
  }

  const float maxRefreshRate = getMaxRefreshRate();

  if (maxRefreshRate > 0.0f) {
    setRefreshRate(maxRefreshRate);
  }
}

bool XrApp::isRefreshRateSupported(const float refreshRate) {
  if (!session_ || !refreshRateExtensionSupported_) {
    return false;
  }

  const std::vector<float>& supportedRefreshRates = getSupportedRefreshRates();
  const bool found_it = (std::find(supportedRefreshRates.begin(), supportedRefreshRates.end(), refreshRate) != supportedRefreshRates.end());
  return found_it;
}

const std::vector<float>& XrApp::getSupportedRefreshRates() {
  if (!session_ || !refreshRateExtensionSupported_) {
    return supportedRefreshRates_;
  }

  if (supportedRefreshRates_.empty()) {
    querySupportedRefreshRates();
  }

  return supportedRefreshRates_;
}

void XrApp::querySupportedRefreshRates() {
  if (!session_ || !refreshRateExtensionSupported_ || !supportedRefreshRates_.empty()) {
    return;
  }

  uint32_t numRefreshRates = 0;
  XrResult result = xrEnumerateDisplayRefreshRatesFB_(session_, 0, &numRefreshRates, nullptr);

  if ((result == XR_SUCCESS) && (numRefreshRates > 0)) {
    supportedRefreshRates_.resize(numRefreshRates);
    result = xrEnumerateDisplayRefreshRatesFB_(session_, numRefreshRates, &numRefreshRates, supportedRefreshRates_.data());

    if (result == XR_SUCCESS) {
      std::sort(supportedRefreshRates_.begin(), supportedRefreshRates_.end());
    }

    for (float refreshRate : supportedRefreshRates_) {
      IGL_LOG_INFO("querySupportedRefreshRates Hz = %.2f.", refreshRate);
    }
  }
}

bool XrApp::isSharpeningEnabled() const {
  return compositionLayerSettingsSupported_ && ((compositionLayerSettings_.layerFlags & XR_COMPOSITION_LAYER_SETTINGS_QUALITY_SHARPENING_BIT_FB) != 0);
}

void XrApp::setSharpeningEnabled(const bool enabled) {
  if (compositionLayerSettingsSupported_) {
    if (enabled) {
      compositionLayerSettings_.layerFlags |= XR_COMPOSITION_LAYER_SETTINGS_QUALITY_SHARPENING_BIT_FB;
    }
    else {
      compositionLayerSettings_.layerFlags &= ~XR_COMPOSITION_LAYER_SETTINGS_QUALITY_SHARPENING_BIT_FB;
    }
    IGL_LOG_INFO("setSharpeningEnabled, Sharpening is now %s", isSharpeningEnabled() ? "ON" : "OFF");
  }
  else if (enabled) {
    IGL_LOG_ERROR("setSharpeningEnabled FAILED, device does not support composition layer settings");
  }
}

} // namespace igl::shell::openxr
