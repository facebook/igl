/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <optional>
#include <igl/Common.h>
#include <igl/Device.h>
#include <igl/DeviceFeatures.h>

#if (IGL_PLATFORM_IOS || IGL_PLATFORM_MACOSX || IGL_PLATFORM_MACCATALYST) && IGL_BACKEND_METAL
#define IGL_METAL_SUPPORTED 1
#else
#define IGL_METAL_SUPPORTED 0
#endif

#if !IGL_PLATFORM_MACCATALYST && IGL_BACKEND_OPENGL && !defined(IGL_TESTS_NO_OPENGL)
#define IGL_OPENGL_SUPPORTED 1
#else
#define IGL_OPENGL_SUPPORTED 0
#endif

#if (IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX) && \
    IGL_BACKEND_VULKAN && !defined(IGL_UNIT_TESTS_NO_VULKAN)
#define IGL_VULKAN_SUPPORTED 1
#else
#define IGL_VULKAN_SUPPORTED 0
#endif

#if IGL_PLATFORM_WINDOWS && defined(IGL_BACKEND_ENABLE_D3D12) && !defined(IGL_UNIT_TESTS_NO_D3D12)
#define IGL_D3D12_SUPPORTED 1
#else
#define IGL_D3D12_SUPPORTED 0
#endif

namespace igl::tests::util::device {

struct TestDeviceConfig {
  std::optional<BackendVersion> requestedOpenGLBackendVersion{};
  bool enableVulkanValidationLayers = true;
};

/**
 Returns whether or not the specified backend type is supported for test devices.
 */
bool isBackendTypeSupported(BackendType backendType);

#if IGL_D3D12_SUPPORTED
constexpr BackendType kDefaultBackendType = BackendType::D3D12;
#elif IGL_OPENGL_SUPPORTED
constexpr BackendType kDefaultBackendType = BackendType::OpenGL;
#elif IGL_VULKAN_SUPPORTED
constexpr BackendType kDefaultBackendType = BackendType::Vulkan;
#elif IGL_METAL_SUPPORTED
constexpr BackendType kDefaultBackendType = BackendType::Metal;
#else
constexpr BackendType kDefaultBackendType = BackendType::Invalid;
#endif

/**
 Create and return an igl::Device that is suitable for running tests against for the specified
 backend.
 For OpenGL, a backendApi value of "2.0" will return a GLES2 context. All other values will return a
 GLES3 context.
 */
std::unique_ptr<IDevice> createTestDevice(BackendType backendType = kDefaultBackendType,
                                          const TestDeviceConfig& config = {});

} // namespace igl::tests::util::device
