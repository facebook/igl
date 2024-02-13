/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <gtest/gtest.h>
#include <igl/IGL.h>
#include <igl/opengl/IContext.h>

#if IGL_PLATFORM_IOS || IGL_PLATFORM_MACOS
#include "simd/simd.h"
#else
#include "simdstub.h"
#endif
namespace igl {
namespace tests {
namespace util {

const std::string BACKEND_OGL("ogl");
const std::string BACKEND_MTL("metal");
const std::string BACKEND_VUL("vulkan");

igl::opengl::RenderingAPI getOpenGLRenderingAPI();

// Creates an IGL device and a command queue
void createDeviceAndQueue(std::shared_ptr<IDevice>& dev, std::shared_ptr<ICommandQueue>& cq);

void createShaderStages(const std::shared_ptr<IDevice>& dev,
                        const char* vertexSource,
                        const char* vertexEntryPoint,
                        const char* fragmentSource,
                        const char* fragmentEntryPoint,
                        std::unique_ptr<IShaderStages>& stages);

void createShaderStages(const std::shared_ptr<IDevice>& dev,
                        const char* librarySource,
                        const char* vertexEntryPoint,
                        const char* fragmentEntryPoint,
                        std::unique_ptr<IShaderStages>& stages);

void createSimpleShaderStages(const std::shared_ptr<IDevice>& dev,
                              std::unique_ptr<IShaderStages>& stages,
                              TextureFormat outputFormat = TextureFormat::Invalid);

} // namespace util
} // namespace tests
} // namespace igl
