/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Uniform.h>

#include <igl/Common.h>

namespace igl {

size_t sizeForUniformType(UniformType type) {
  switch (type) {
  case UniformType::Invalid:
    return 0;
  case UniformType::Float:
    return sizeof(float);
  case UniformType::Float2:
    return sizeof(float[2]);
  case UniformType::Float3:
    return sizeof(float[3]);
  case UniformType::Float4:
    return sizeof(float[4]);
  case UniformType::Boolean:
    return sizeof(bool);
  case UniformType::Int:
    return sizeof(int32_t);
  case UniformType::Int2:
    return sizeof(int32_t[2]);
  case UniformType::Int3:
    return sizeof(int32_t[3]);
  case UniformType::Int4:
    return sizeof(int32_t[4]);
  case UniformType::Mat2x2:
    return sizeof(float[2][2]);
  case UniformType::Mat3x3:
    return sizeof(float[3][3]);
  case UniformType::Mat4x4:
    return sizeof(float[4][4]);
  default:
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED(); // missing enum case
    return 0;
  }
}

// Returns the size of the underlying element
size_t sizeForUniformElementType(UniformType type) {
  switch (type) {
  case UniformType::Invalid:
    return 0;

  case UniformType::Float:
  case UniformType::Float2:
  case UniformType::Float3:
  case UniformType::Float4:
  case UniformType::Mat2x2:
  case UniformType::Mat3x3:
  case UniformType::Mat4x4:
    return sizeof(float);

  case UniformType::Boolean:
    return sizeof(bool);

  case UniformType::Int:
  case UniformType::Int2:
  case UniformType::Int3:
  case UniformType::Int4:
    return sizeof(int32_t);

  default:
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED(); // missing enum case
    return 0;
  }
}

} // namespace igl
