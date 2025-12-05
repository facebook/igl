/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <igl/d3d12/Device.h>

namespace igl::tests::util::device::d3d12 {

/**
 * Create and return an igl::d3d12::Device that is suitable for running tests against.
 * This creates a headless device without a swapchain, suitable for unit testing.
 */
std::unique_ptr<igl::d3d12::Device> createTestDevice(bool enableDebugLayer = true);

} // namespace igl::tests::util::device::d3d12
