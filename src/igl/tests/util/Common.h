/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <gtest/gtest.h>
#include <string_view>
#include <igl/Device.h>

#if IGL_PLATFORM_IOS || IGL_PLATFORM_MACOSX
#include "simd/simd.h"
#else
#include "simdstub.h"
#endif
namespace igl::tests::util {

constexpr std::string_view kBackendOgl("ogl");
constexpr std::string_view kBackendMtl("metal");
constexpr std::string_view kBackendVul("vulkan");

// Creates an IGL device and a command queue
void createDeviceAndQueue(std::shared_ptr<IDevice>& dev, std::shared_ptr<ICommandQueue>& cq);

void createShaderStages(const std::shared_ptr<IDevice>& dev,
                        std::string_view vertexSource,
                        std::string_view vertexEntryPoint,
                        std::string_view fragmentSource,
                        std::string_view fragmentEntryPoint,
                        std::unique_ptr<IShaderStages>& stages);

void createShaderStages(const std::shared_ptr<IDevice>& dev,
                        std::string_view librarySource,
                        std::string_view vertexEntryPoint,
                        std::string_view fragmentEntryPoint,
                        std::unique_ptr<IShaderStages>& stages);

void createSimpleShaderStages(const std::shared_ptr<IDevice>& dev,
                              std::unique_ptr<IShaderStages>& stages,
                              TextureFormat outputFormat = TextureFormat::Invalid);

} // namespace igl::tests::util
