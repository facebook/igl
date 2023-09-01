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

/// Reads back a range of texture data
/// @param device The device the texture was created with
/// @param cmdQueue A command queue to submit any read requests on
/// @param texture The texture to validate
/// @param isRenderTarget True if the texture was the target of a render pass; false otherwise
/// @param range The range of data to validate. Must resolve to a single 2D texture region
/// @param expectedData The expected data in the specified range
/// @param message A message to print when validation fails
void validateTextureRange(IDevice& device,
                          ICommandQueue& cmdQueue,
                          const std::shared_ptr<ITexture>& texture,
                          bool isRenderTarget,
                          const TextureRangeDesc& range,
                          const uint32_t* expectedData,
                          const char* message);

void validateFramebufferTextureRange(IDevice& device,
                                     ICommandQueue& cmdQueue,
                                     const IFramebuffer& framebuffer,
                                     const TextureRangeDesc& range,
                                     const uint32_t* expectedData,
                                     const char* message);
void validateFramebufferTexture(IDevice& device,
                                ICommandQueue& cmdQueue,
                                const IFramebuffer& framebuffer,
                                const uint32_t* expectedData,
                                const char* message);

void validateUploadedTextureRange(IDevice& device,
                                  ICommandQueue& cmdQueue,
                                  const std::shared_ptr<ITexture>& texture,
                                  const TextureRangeDesc& range,
                                  const uint32_t* expectedData,
                                  const char* message);
void validateUploadedTexture(IDevice& device,
                             ICommandQueue& cmdQueue,
                             const std::shared_ptr<ITexture>& texture,
                             const uint32_t* expectedData,
                             const char* message);

} // namespace util
} // namespace tests
} // namespace igl
