/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/D3D12ReflectionUtils.h>

namespace igl::d3d12::ReflectionUtils {

igl::UniformType mapUniformType(const D3D12_SHADER_TYPE_DESC& td) {
  if ((td.Class == D3D_SVC_MATRIX_ROWS || td.Class == D3D_SVC_MATRIX_COLUMNS) && td.Rows == 4 &&
      td.Columns == 4) {
    return igl::UniformType::Mat4x4;
  }
  if (td.Type == D3D_SVT_FLOAT) {
    if (td.Class == D3D_SVC_SCALAR)
      return igl::UniformType::Float;
    if (td.Class == D3D_SVC_VECTOR) {
      switch (td.Columns) {
      case 2:
        return igl::UniformType::Float2;
      case 3:
        return igl::UniformType::Float3;
      case 4:
        return igl::UniformType::Float4;
      default:
        return igl::UniformType::Invalid;
      }
    }
  }
  return igl::UniformType::Invalid;
}

} // namespace igl::d3d12::ReflectionUtils
