/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>

namespace igl {
class ICommandBuffer;
namespace opengl {

class VertexArrayObject final : public WithContext {
  friend class Device;

 public:
  explicit VertexArrayObject(IContext& context) : WithContext(context) {}
  ~VertexArrayObject() override;

  Result create();
  void bind() const;
  void unbind() const;

 private:
  GLuint vertexAttriuteObject_ = ~0;
};
} // namespace opengl
} // namespace igl
