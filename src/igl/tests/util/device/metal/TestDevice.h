/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <igl/metal/Device.h>

namespace igl::tests::util::device::metal {

/**
 Create and return an igl::Device that is suitable for running tests against.
 */
std::shared_ptr<igl::metal::Device> createTestDevice();

} // namespace igl::tests::util::device::metal
