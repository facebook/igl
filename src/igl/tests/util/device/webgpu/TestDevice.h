/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <igl/webgpu/Device.h>

namespace igl::tests::util::device::webgpu {

/**
 Create and return an igl::Device that is suitable for running tests against.
 */
std::unique_ptr<igl::webgpu::Device> createTestDevice(
    const igl::webgpu::WebGPUContextDesc& desc = {});

} // namespace igl::tests::util::device::webgpu
