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
