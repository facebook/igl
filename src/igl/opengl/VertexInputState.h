/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_map>
#include <vector>
#include <igl/VertexInputState.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>

namespace igl {
class ICommandBuffer;
namespace opengl {

// This structure stores OGL-specific attribute info. Location is specifically
// not included because it is shader-dependent
struct OGLAttribute {
  std::string name;
  GLsizei stride = 0;
  uintptr_t bufferOffset = 0;
  GLint numComponents = 0;
  GLenum componentType = GL_FLOAT;
  GLboolean normalized = 0u;
  VertexSampleFunction sampleFunction = VertexSampleFunction::PerVertex;
  size_t sampleRate = 1;

  OGLAttribute() = default;
};

class VertexInputState final : public IVertexInputState {
  friend class Device;

 public:
  Result create(const VertexInputStateDesc& desc);

  // Returns a list of attributes associated with the buffer. An empty vector
  // will be returned the bufferIndex is invalid
  const std::vector<OGLAttribute>& getAssociatedAttributes(size_t bufferIndex);

  // Returns a read-only reference to the bufferOGLAttribMap_
  [[nodiscard]] const std::unordered_map<size_t, std::vector<OGLAttribute>>& getBufferAttribMap()
      const {
    return bufferOGLAttribMap_;
  }

 private:
  std::unordered_map<size_t, std::vector<OGLAttribute>> bufferOGLAttribMap_;
};

} // namespace opengl
} // namespace igl
