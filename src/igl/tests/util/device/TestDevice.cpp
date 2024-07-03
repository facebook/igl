/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/tests/util/device/TestDevice.h>

#include <igl/Common.h>
#include <igl/Macros.h>

#if (IGL_PLATFORM_IOS || IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST) && IGL_BACKEND_METAL
#define IGL_METAL_SUPPORTED 1
#else
#define IGL_METAL_SUPPORTED 0
#endif

#if !IGL_PLATFORM_MACCATALYST && IGL_BACKEND_OPENGL
#define IGL_OPENGL_SUPPORTED 1
#else
#define IGL_OPENGL_SUPPORTED 0
#endif

#if (IGL_PLATFORM_WIN || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOS || IGL_PLATFORM_LINUX) && \
    IGL_BACKEND_VULKAN && !defined(IGL_UNIT_TESTS_NO_VULKAN)
#define IGL_VULKAN_SUPPORTED 1
#else
#define IGL_VULKAN_SUPPORTED 0
#endif

#if IGL_METAL_SUPPORTED
#include "metal/TestDevice.h"
#endif
#if IGL_OPENGL_SUPPORTED
#include "opengl/TestDevice.h"
#endif
#if IGL_VULKAN_SUPPORTED
#include "vulkan/TestDevice.h"
#endif

namespace igl::tests::util::device {

bool isBackendTypeSupported(::igl::BackendType backendType) {
  switch (backendType) {
  case ::igl::BackendType::Invalid:
    IGL_ASSERT_NOT_REACHED();
    return false;
  case ::igl::BackendType::Metal:
    return IGL_METAL_SUPPORTED;
  case ::igl::BackendType::OpenGL:
    return IGL_OPENGL_SUPPORTED;
  case ::igl::BackendType::Vulkan:
    return IGL_VULKAN_SUPPORTED;
  // @fb-only
    // @fb-only
  }
  IGL_UNREACHABLE_RETURN(false)
}

std::shared_ptr<::igl::IDevice> createTestDevice(::igl::BackendType backendType,
                                                 const std::string& backendApi,
                                                 bool enableValidation) {
  if (backendType == ::igl::BackendType::Metal) {
#if IGL_METAL_SUPPORTED
    return metal::createTestDevice();
#else
    return nullptr;
#endif
  }
  if (backendType == ::igl::BackendType::OpenGL) {
#if IGL_OPENGL_SUPPORTED
    return opengl::createTestDevice(backendApi);
#else
    return nullptr;
#endif
  }
  if (backendType == ::igl::BackendType::Vulkan) {
#if IGL_VULKAN_SUPPORTED
    return vulkan::createTestDevice(enableValidation);
#else
    return nullptr;
#endif
  }
  return nullptr;
}

} // namespace igl::tests::util::device
