/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/openxr/XrPlatform.h>

#include <vector>

namespace igl::shell::openxr {

class XrPassthrough final {
 public:
  XrPassthrough(XrInstance instance, XrSession session) noexcept;
  ~XrPassthrough() noexcept;

  [[nodiscard]] static const std::vector<const char*>& getExtensions() noexcept;

  [[nodiscard]] bool initialize() noexcept;

  void setEnabled(bool enabled) noexcept;

  void injectLayer(std::vector<const XrCompositionLayerBaseHeader*>& layers) noexcept;

 private:
  const XrInstance instance_;
  const XrSession session_;

  // API
  PFN_xrCreatePassthroughFB xrCreatePassthroughFB_ = nullptr;
  PFN_xrDestroyPassthroughFB xrDestroyPassthroughFB_ = nullptr;
  PFN_xrPassthroughStartFB xrPassthroughStartFB_ = nullptr;
  PFN_xrPassthroughPauseFB xrPassthroughPauseFB_ = nullptr;
  PFN_xrCreatePassthroughLayerFB xrCreatePassthroughLayerFB_ = nullptr;
  PFN_xrDestroyPassthroughLayerFB xrDestroyPassthroughLayerFB_ = nullptr;
  PFN_xrPassthroughLayerSetStyleFB xrPassthroughLayerSetStyleFB_ = nullptr;

  XrPassthroughFB passthrough_ = XR_NULL_HANDLE;
  XrPassthroughLayerFB passthroughLayer_ = XR_NULL_HANDLE;
  XrCompositionLayerPassthroughFB compositionLayer_{XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB};

  bool enabled_ = false;
};

} // namespace igl::shell::openxr
