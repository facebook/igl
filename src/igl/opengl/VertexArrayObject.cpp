/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/VertexArrayObject.h>

#include <cstdlib>
#include <string>

namespace igl::opengl {

Result VertexArrayObject::create() {
  getContext().genVertexArrays(1, &vertexAttriuteObject_);
  if (vertexAttriuteObject_ == 0) {
    return Result(Result::Code::RuntimeError, "Failed to create vertex array object ID");
  }
  return Result();
}

void VertexArrayObject::bind() const {
  getContext().bindVertexArray(vertexAttriuteObject_);
}

void VertexArrayObject::unbind() const {
  getContext().bindVertexArray(0);
}

bool VertexArrayObject::isValid() const {
  return vertexAttriuteObject_ != 0;
}

VertexArrayObject::~VertexArrayObject() {
  getContext().deleteVertexArrays(1, &vertexAttriuteObject_);
}
} // namespace igl::opengl
