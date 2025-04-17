/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/renderSession/ShaderProvider.h>
#include <igl/Device.h>
#include <igl/Shader.h>
#include <igl/ShaderCreator.h>

namespace igl::shell {
std::unique_ptr<IShaderStages> createRenderPipelineStages(
    const igl::IDevice& device,
    const IShaderProvider& vertShaderProvider,
    const IShaderProvider& fragShaderProvider);

std::unique_ptr<IShaderStages> createComputePipelineStages(
    const igl::IDevice& device,
    const IShaderProvider& compShaderProvider);
} // namespace igl::shell
