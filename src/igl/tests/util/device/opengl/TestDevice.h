/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <optional>
#include <igl/DeviceFeatures.h>

namespace igl {
class IDevice;
}

namespace igl::tests::util::device::opengl {

/**
 Create and return an igl::Device that is suitable for running tests against.
 */
std::shared_ptr<IDevice> createTestDevice(std::optional<BackendVersion> requestedVersion = {});

} // namespace igl::tests::util::device::opengl
