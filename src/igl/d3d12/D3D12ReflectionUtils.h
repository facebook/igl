/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Uniform.h>
#include <igl/d3d12/D3D12Headers.h>

namespace igl::d3d12::ReflectionUtils {

/**
 * Maps D3D12 shader type descriptor to IGL uniform type
 *
 * Supported types:
 * - float (D3D_SVT_FLOAT + D3D_SVC_SCALAR) → UniformType::Float
 * - float2/3/4 (D3D_SVT_FLOAT + D3D_SVC_VECTOR) → UniformType::Float2/3/4
 * - float4x4 (D3D_SVC_MATRIX_ROWS/COLUMNS, 4x4) → UniformType::Mat4x4
 *
 * All other types (int, uint, bool, matrices other than 4x4, etc.) map to UniformType::Invalid
 *
 * @param td D3D12 shader type descriptor from reflection
 * @return Corresponding IGL UniformType, or UniformType::Invalid for unsupported types
 */
igl::UniformType mapUniformType(const D3D12_SHADER_TYPE_DESC& td);

} // namespace igl::d3d12::ReflectionUtils
