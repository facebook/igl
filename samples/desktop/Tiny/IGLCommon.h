/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/IGL.h>
#include <memory>

#define USE_OPENGL_BACKEND 0

#if IGL_BACKEND_OPENGL && !IGL_BACKEND_VULKAN
// no IGL/Vulkan was compiled in, switch to IGL/OpenGL
#undef USE_OPENGL_BACKEND
#define USE_OPENGL_BACKEND 1
#endif

// clang-format off
#if USE_OPENGL_BACKEND
  #include <igl/RenderCommandEncoder.h>
  #include <igl/opengl/RenderCommandEncoder.h>
  #include <igl/opengl/RenderPipelineState.h>
#if IGL_PLATFORM_WIN
    #include <igl/opengl/wgl/Context.h>
    #include <igl/opengl/wgl/Device.h>
    #include <igl/opengl/wgl/HWDevice.h>
    #include <igl/opengl/wgl/PlatformDevice.h>
#elif IGL_PLATFORM_LINUX
    #include <igl/opengl/glx/Context.h>
    #include <igl/opengl/glx/Device.h>
    #include <igl/opengl/glx/HWDevice.h>
    #include <igl/opengl/glx/PlatformDevice.h>
#endif
#else
  #include <igl/vulkan/Common.h>
  #include <igl/vulkan/Device.h>
  #include <igl/vulkan/HWDevice.h>
  #include <igl/vulkan/PlatformDevice.h>
  #include <igl/vulkan/VulkanContext.h>
#endif // USE_OPENGL_BACKEND
// clang-format on

// libc++'s implementation of std::format has a large binary size impact
// (https://github.com/llvm/llvm-project/issues/64180), so avoid it on Android.
#if defined(__cpp_lib_format) && !defined(__ANDROID__)
#include <format>
#define IGL_FORMAT std::format
#else
#include <fmt/core.h>
#define IGL_FORMAT fmt::format
#endif // __cpp_lib_format

namespace igl {
struct DeviceContextSettings {
  bool enableValidation = false;
  bool enableDescriptorIndexing = false;
};

std::unique_ptr<IDevice> createIGLDevice(void* window,
                                         void* display,
                                         void* context,
                                         int width,
                                         int height,
                                         DeviceContextSettings ctxSettings = {});

std::shared_ptr<ITexture> getNativeDrawable(IDevice* device, int winWidth, int winHeight);
std::shared_ptr<ITexture> getNativeDepthDrawable(IDevice* device, int winWidth, int winHeight);

} // namespace igl
