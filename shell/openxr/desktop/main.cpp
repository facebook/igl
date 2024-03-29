/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Common.h>

#include <shell/openxr/XrApp.h>

using namespace igl::shell::openxr;

#if USE_VULKAN_BACKEND || IGL_BACKEND_VULKAN
#include "vulkan/XrAppImplVulkan.h"
// @fb-only
// @fb-only
#endif

XrInstance gInstance_;
XrInstance getXrInstance();

XrInstance getXrInstance() {
  return gInstance_;
}

int main(int argc, const char* argv[]) {
#if USE_VULKAN_BACKEND || IGL_BACKEND_VULKAN
  auto xrApp = std::make_unique<XrApp>(std::make_unique<desktop::XrAppImplVulkan>());
// @fb-only
  // @fb-only
#endif
  if (!xrApp->initialize(nullptr, {})) {
    return 1;
  }

  gInstance_ = xrApp->instance();
  xrApp->setResumed(true);

  while (true) {
    xrApp->handleXrEvents();
    if (!xrApp->sessionActive()) {
      continue;
    }

    xrApp->update();
  }

  return 0;
}
