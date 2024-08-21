/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/openxr/XrPassthrough.h>

#include <shell/openxr/XrLog.h>

namespace igl::shell::openxr {

XrPassthrough::XrPassthrough(XrInstance instance, XrSession session) noexcept :
  instance_(instance), session_(session) {
  XR_CHECK(xrGetInstanceProcAddr(
      instance_, "xrCreatePassthroughFB", (PFN_xrVoidFunction*)(&xrCreatePassthroughFB_)));
  XR_CHECK(xrGetInstanceProcAddr(
      instance_, "xrDestroyPassthroughFB", (PFN_xrVoidFunction*)(&xrDestroyPassthroughFB_)));
  XR_CHECK(xrGetInstanceProcAddr(
      instance_, "xrPassthroughStartFB", (PFN_xrVoidFunction*)(&xrPassthroughStartFB_)));
  XR_CHECK(xrGetInstanceProcAddr(
      instance_, "xrPassthroughPauseFB", (PFN_xrVoidFunction*)(&xrPassthroughPauseFB_)));
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

XrPassthrough::~XrPassthrough() noexcept {
  if (passthroughLayer_ != XR_NULL_HANDLE) {
    xrDestroyPassthroughLayerFB_(passthroughLayer_);
  }

  if (passthrough_ != XR_NULL_HANDLE) {
    xrDestroyPassthroughFB_(passthrough_);
  }
}

// NOLINTNEXTLINE(bugprone-exception-escape)
const std::vector<const char*>& XrPassthrough::getExtensions() noexcept {
  static const std::vector<const char*> kExtensions{XR_FB_PASSTHROUGH_EXTENSION_NAME};
  return kExtensions;
}

bool XrPassthrough::initialize() noexcept {
  const XrPassthroughCreateInfoFB passthroughInfo{
      .type = XR_TYPE_PASSTHROUGH_CREATE_INFO_FB,
      .next = nullptr,
      .flags = XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB,
  };

  XrResult result;
  XR_CHECK(result = xrCreatePassthroughFB_(session_, &passthroughInfo, &passthrough_));
  if (result != XR_SUCCESS) {
    IGL_LOG_ERROR("xrCreatePassthroughFB failed.\n");
    return false;
  }

  const XrPassthroughLayerCreateInfoFB layerInfo{
      .type = XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB,
      .next = nullptr,
      .passthrough = passthrough_,
      .flags = XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB,
      .purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB,
  };

  XR_CHECK(result = xrCreatePassthroughLayerFB_(session_, &layerInfo, &passthroughLayer_));
  if (result != XR_SUCCESS) {
    IGL_LOG_ERROR("xrCreatePassthroughLayerFB failed.\n");
    return false;
  }

  const XrPassthroughStyleFB style{.type = XR_TYPE_PASSTHROUGH_STYLE_FB,
                                   .next = nullptr,
                                   .textureOpacityFactor = 1.0f,
                                   .edgeColor = {0.0f, 0.0f, 0.0f, 0.0f}};

  XR_CHECK(result = xrPassthroughLayerSetStyleFB_(passthroughLayer_, &style));
  if (result != XR_SUCCESS) {
    IGL_LOG_ERROR("xrPassthroughLayerSetStyleFB failed.\n");
    return false;
  }

  compositionLayer_.next = nullptr;
  compositionLayer_.layerHandle = passthroughLayer_;

  return true;
}

void XrPassthrough::setEnabled(bool enabled) noexcept {
  if (enabled_ == enabled) {
    return;
  }
  enabled_ = enabled;

  XrResult result;
  if (enabled_) {
    XR_CHECK(result = xrPassthroughStartFB_(passthrough_));
    if (result != XR_SUCCESS) {
      IGL_LOG_ERROR("xrPassthroughStartFB failed.\n");
    }
  } else {
    XR_CHECK(result = xrPassthroughPauseFB_(passthrough_));
    if (result != XR_SUCCESS) {
      IGL_LOG_ERROR("xrPassthroughPauseFB failed.\n");
    }
  }
}

void XrPassthrough::injectLayer(std::vector<const XrCompositionLayerBaseHeader*>& layers) noexcept {
  layers.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&compositionLayer_));
}

} // namespace igl::shell::openxr
