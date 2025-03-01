/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <memory>
#include <string>

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

namespace igl {
class IDevice;
namespace tests::util::device {

/**
 Returns whether or not the specified backend type is supported for test devices.
 */
bool isBackendTypeSupported(BackendType backendType);

/**
 Create and return an igl::Device that is suitable for running tests against for the specified
 backend.
 For OpenGL, a backendApi value of "2.0" will return a GLES2 context. All other values will return a
 GLES3 context.
 */
std::shared_ptr<IDevice> createTestDevice(BackendType backendType,
                                          const std::string& backendApi = "",
                                          bool enableValidation = true);

} // namespace tests::util::device
} // namespace igl
