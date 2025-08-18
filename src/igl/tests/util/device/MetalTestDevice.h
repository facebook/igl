/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>

namespace igl {
class IDevice;
} // namespace igl

namespace igl::tests::util::device {

/**
 Create and return an igl::Device that is suitable for running tests against for the specified
 backend.
 For OpenGL, a backendApi value of "2.0" will return a GLES2 context. All other values will return a
 GLES3 context.
 */
std::unique_ptr<IDevice> createMetalTestDevice();

} // namespace igl::tests::util::device
