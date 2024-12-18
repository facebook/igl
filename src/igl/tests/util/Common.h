/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <gtest/gtest.h>
#include <igl/IGL.h>
#if IGL_BACKEND_OPENGL
#include <igl/opengl/IContext.h>
#endif // IGL_BACKEND_OPENGL

#if IGL_PLATFORM_IOS || IGL_PLATFORM_MACOSX
#include "simd/simd.h"
#else
#include "simdstub.h"
#endif
namespace igl::tests::util {

const std::string BACKEND_OGL("ogl");
const std::string BACKEND_MTL("metal");
const std::string BACKEND_VUL("vulkan");

#if IGL_BACKEND_OPENGL
igl::opengl::RenderingAPI getOpenGLRenderingAPI();
#endif // IGL_BACKEND_OPENGL

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

} // namespace igl::tests::util
