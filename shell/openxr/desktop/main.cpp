/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Common.h>

#include "XrApp.h"
using namespace igl::shell::openxr::desktop;

#if defined(USE_VULKAN_BACKEND)
#include "vulkan/XrAppImplVulkan.h"
#endif

XrInstance gInstance_;
XrInstance getXrInstance();

XrInstance getXrInstance() {
  return gInstance_;
}

int main(int argc, const char* argv[]) {
#if defined(USE_VULKAN_BACKEND)
  auto xrApp = std::make_unique<XrApp>(std::make_unique<XrAppImplVulkan>());
#endif
  if (!xrApp->initialize(nullptr)) {
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
