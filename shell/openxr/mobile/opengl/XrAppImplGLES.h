/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/openxr/XrPlatform.h>

#include <shell/openxr/impl/XrAppImpl.h>

namespace igl::shell::openxr::mobile {

class XrAppImplGLES final : public impl::XrAppImpl {
 public:
  [[nodiscard]] std::vector<const char*> getXrRequiredExtensions() const override;
  [[nodiscard]] std::vector<const char*> getXrOptionalExtensions() const override;

  [[nodiscard]] std::unique_ptr<igl::IDevice> initIGL(XrInstance instance,
                                                      XrSystemId systemId) override;
  [[nodiscard]] XrSession initXrSession(XrInstance instance,
                                        XrSystemId systemId,
                                        igl::IDevice& device) override;
  [[nodiscard]] std::unique_ptr<impl::XrSwapchainProviderImpl> createSwapchainProviderImpl()
      const override;

 private:
#if IGL_WGL
  XrGraphicsRequirementsOpenGLKHR graphicsRequirements_ = {
      .type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR,
  };
#else
  XrGraphicsRequirementsOpenGLESKHR graphicsRequirements_ = {
      .type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR,
  };
#endif // IGL_WGL
};
} // namespace igl::shell::openxr::mobile
