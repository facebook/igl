/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/VertexArrayObject.h>

#include <cstdlib>
#include <igl/opengl/Device.h>
#include <igl/opengl/Errors.h>
#include <string>
#include <vector>

namespace igl {
namespace opengl {

Result VertexArrayObject::create() {
  getContext().genVertexArrays(1, &vertexAttriuteObject_);
  return Result();
}

void VertexArrayObject::bind() const {
  getContext().bindVertexArray(vertexAttriuteObject_);
}

void VertexArrayObject::unbind() const {
  getContext().bindVertexArray(0);
}

VertexArrayObject::~VertexArrayObject() {
  getContext().deleteVertexArrays(1, &vertexAttriuteObject_);
}
} // namespace opengl
} // namespace igl
