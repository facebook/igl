/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/openxr/XrPlatform.h>
#include <shell/shared/renderSession/Hands.h>

#include <array>
#include <vector>

namespace igl::shell::openxr {

class XrHands final {
 public:
  XrHands(XrInstance instance, XrSession session, bool handMeshSupported) noexcept;
  ~XrHands() noexcept;

  [[nodiscard]] static const std::vector<const char*>& getExtensions() noexcept;

  [[nodiscard]] bool initialize() noexcept;

  void updateTracking(XrSpace currentSpace, std::array<HandTracking, 2>& handTracking) noexcept;
  void updateMeshes(std::array<HandMesh, 2>& handMeshes) noexcept;

 private:
  const XrInstance instance_;
  const XrSession session_;
  const bool handMeshSupported_;

  // API
  PFN_xrCreateHandTrackerEXT xrCreateHandTrackerEXT_ = nullptr;
  PFN_xrDestroyHandTrackerEXT xrDestroyHandTrackerEXT_ = nullptr;
  PFN_xrLocateHandJointsEXT xrLocateHandJointsEXT_ = nullptr;
  PFN_xrGetHandMeshFB xrGetHandMeshFB_ = nullptr;

  XrHandTrackerEXT leftHandTracker_ = XR_NULL_HANDLE;
  XrHandTrackerEXT rightHandTracker_ = XR_NULL_HANDLE;
};

} // namespace igl::shell::openxr
