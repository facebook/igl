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
namespace tests::util::device::metal {

/**
 Create and return an igl::Device that is suitable for running tests against.
 */
std::shared_ptr<IDevice> createTestDevice();

} // namespace tests::util::device::metal
} // namespace igl
