/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/tests/util/device/TestDevice.h>

#include <igl/Common.h>
#include <igl/Macros.h>

// clang-format off
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// clang-format on
// @fb-only
#if IGL_METAL_SUPPORTED
#include <igl/tests/util/device/MetalTestDevice.h>
#endif
#if IGL_OPENGL_SUPPORTED
#include <igl/tests/util/device/opengl/TestDevice.h>
#endif
#if IGL_VULKAN_SUPPORTED
#include <igl/tests/util/device/vulkan/TestDevice.h>
#endif
#if IGL_D3D12_SUPPORTED
#include <igl/tests/util/device/d3d12/TestDevice.h>
#endif
// @fb-only
// @fb-only
// @fb-only

namespace igl::tests::util::device {

bool isBackendTypeSupported(BackendType backendType) {
  switch (backendType) {
  case ::igl::BackendType::Invalid:
  case ::igl::BackendType::Custom:
    IGL_DEBUG_ASSERT_NOT_REACHED();
    return false;
  case ::igl::BackendType::Metal:
    return IGL_METAL_SUPPORTED;
  case ::igl::BackendType::OpenGL:
    return IGL_OPENGL_SUPPORTED;
  case ::igl::BackendType::Vulkan:
    return IGL_VULKAN_SUPPORTED;
  case ::igl::BackendType::D3D12:
    return IGL_D3D12_SUPPORTED;
  // @fb-only
    // @fb-only
  }
  IGL_UNREACHABLE_RETURN(false)
}

std::unique_ptr<IDevice> createTestDevice(BackendType backendType, const TestDeviceConfig& config) {
  if (backendType == ::igl::BackendType::Metal) {
#if IGL_METAL_SUPPORTED
    return createMetalTestDevice();
#else
    return nullptr;
#endif
  }
  if (backendType == ::igl::BackendType::OpenGL) {
#if IGL_OPENGL_SUPPORTED
    return opengl::createTestDevice(config.requestedOpenGLBackendVersion);
#else
    return nullptr;
#endif
  }
  if (backendType == ::igl::BackendType::Vulkan) {
#if IGL_VULKAN_SUPPORTED
    return vulkan::createTestDevice(config.enableVulkanValidationLayers);
#else
    return nullptr;
#endif
  }
  if (backendType == ::igl::BackendType::D3D12) {
#if IGL_D3D12_SUPPORTED
    IGL_LOG_INFO("[Tests] Creating D3D12 test device (debug layer: disabled)\n");
    auto dev = d3d12::createTestDevice(false);
    if (!dev) {
      IGL_LOG_ERROR("[Tests] D3D12 test device creation failed\n");
    } else {
      IGL_LOG_INFO("[Tests] D3D12 test device created OK\n");
    }
    return dev;
#else
    return nullptr;
#endif
  }
  // @fb-only
// @fb-only
    // @fb-only
// @fb-only
    // @fb-only
// @fb-only
  // @fb-only
  return nullptr;
}

} // namespace igl::tests::util::device
