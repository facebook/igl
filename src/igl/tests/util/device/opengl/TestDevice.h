/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/opengl/IContext.h>
#include <memory>
#include <string>

namespace igl {
class IDevice;
namespace tests::util::device::opengl {

/**
 Returns the OpenGL Rendering API.
 "2.0" -> GLES2, anything else: GLES3
 */
igl::opengl::RenderingAPI getOpenGLRenderingAPI(const std::string& backendApi = "");

/**
 Create and return an igl::Device that is suitable for running tests against.
 */
std::shared_ptr<IDevice> createTestDevice(const std::string& backendApi = "");

} // namespace tests::util::device::opengl
} // namespace igl
