/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/openxr/XrHands.h>

#include <shell/openxr/XrLog.h>

#include <chrono>
#include <cstdint>

namespace igl::shell::openxr {

namespace {
inline glm::quat glmQuatFromXrQuat(const XrQuaternionf& quat) noexcept {
  return {quat.w, quat.x, quat.y, quat.z};
}

inline glm::vec4 glmVecFromXrVec(const XrVector4f& vec) noexcept {
  return {vec.x, vec.y, vec.z, vec.w};
}

inline glm::vec4 glmVecFromXrVec(const XrVector4sFB& vec) noexcept {
  return {vec.x, vec.y, vec.z, vec.w};
}

inline glm::vec3 glmVecFromXrVec(const XrVector3f& vec) noexcept {
  return {vec.x, vec.y, vec.z};
}

inline glm::vec2 glmVecFromXrVec(const XrVector2f& vec) noexcept {
  return {vec.x, vec.y};
}

inline Pose poseFromXrPose(const XrPosef& pose) noexcept {
  return Pose{
      .orientation = glmQuatFromXrQuat(pose.orientation),
      .position = glmVecFromXrVec(pose.position),
  };
}

inline int64_t currentTimeInNs() {
  const auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
}
} // namespace

XrHands::XrHands(XrInstance instance, XrSession session, bool handMeshSupported) noexcept :
  instance_(instance), session_(session), handMeshSupported_(handMeshSupported) {
  XR_CHECK(xrGetInstanceProcAddr(
      instance_, "xrCreateHandTrackerEXT", (PFN_xrVoidFunction*)(&xrCreateHandTrackerEXT_)));
  IGL_ASSERT(xrCreateHandTrackerEXT_ != nullptr);
  XR_CHECK(xrGetInstanceProcAddr(
      instance_, "xrDestroyHandTrackerEXT", (PFN_xrVoidFunction*)(&xrDestroyHandTrackerEXT_)));
  IGL_ASSERT(xrDestroyHandTrackerEXT_ != nullptr);
  XR_CHECK(xrGetInstanceProcAddr(
      instance_, "xrLocateHandJointsEXT", (PFN_xrVoidFunction*)(&xrLocateHandJointsEXT_)));
  IGL_ASSERT(xrLocateHandJointsEXT_ != nullptr);

  if (handMeshSupported) {
    XR_CHECK(xrGetInstanceProcAddr(
        instance_, "xrGetHandMeshFB", (PFN_xrVoidFunction*)(&xrGetHandMeshFB_)));
    IGL_ASSERT(xrGetHandMeshFB_ != nullptr);
  }
}

XrHands::~XrHands() noexcept {
  if (leftHandTracker_ != XR_NULL_HANDLE) {
    xrDestroyHandTrackerEXT_(leftHandTracker_);
  }
  if (rightHandTracker_ != XR_NULL_HANDLE) {
    xrDestroyHandTrackerEXT_(rightHandTracker_);
  }
}

// NOLINTNEXTLINE(bugprone-exception-escape)
const std::vector<const char*>& XrHands::getExtensions() noexcept {
  static const std::vector<const char*> kExtensions{
      XR_EXT_HAND_TRACKING_EXTENSION_NAME,
      XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME,
  };
  return kExtensions;
}

bool XrHands::initialize() noexcept {
  std::array<XrHandTrackingDataSourceEXT, 2> dataSources = {
      XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT,
      XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT,
  };

  const XrHandTrackingDataSourceInfoEXT dataSourceInfo{
      .type = XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT,
      .requestedDataSourceCount = static_cast<uint32_t>(dataSources.size()),
      .requestedDataSources = dataSources.data()};

  XrHandTrackerCreateInfoEXT createInfo{
      .type = XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT,
      .next = &dataSourceInfo,
      .hand = XR_HAND_LEFT_EXT,
      .handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT,
  };

  XrResult result;
  XR_CHECK(result = xrCreateHandTrackerEXT_(session_, &createInfo, &leftHandTracker_));
  if (result != XR_SUCCESS) {
    IGL_LOG_ERROR("xrCreateHandTrackerEXT (left hand) failed.\n");
    return false;
  }

  createInfo.hand = XR_HAND_RIGHT_EXT;
  XR_CHECK(result = xrCreateHandTrackerEXT_(session_, &createInfo, &rightHandTracker_));
  if (result != XR_SUCCESS) {
    IGL_LOG_ERROR("xrCreateHandTrackerEXT (right hand) failed.\n");
    return false;
  }

  return true;
}

void XrHands::updateMeshes(std::array<HandMesh, 2>& handMeshes) noexcept {
  if (!handMeshSupported_) {
    return;
  }

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

void XrHands::updateTracking(XrSpace currentSpace,
                             std::array<HandTracking, 2>& handTracking) noexcept {
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
                                               .baseSpace = currentSpace,
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

} // namespace igl::shell::openxr
