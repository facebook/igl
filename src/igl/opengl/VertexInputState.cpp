/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/VertexInputState.h>

#include <cstdlib>
#include <string>
#include <vector>
#include <igl/opengl/Device.h>
#include <igl/opengl/Errors.h>

namespace igl::opengl {

//
// A utility function to convert an IGL attribute to an OGL attribute
//
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
static void toOGLAttribute(const VertexAttribute& attrib,
                           GLint& numComponents,
                           GLenum& componentType,
                           GLboolean& normalized) {
  // NOLINTEND(bugprone-easily-swappable-parameters)
  switch (attrib.format) {
  case VertexAttributeFormat::Float1:
    numComponents = 1;
    componentType = GL_FLOAT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::Float2:
    numComponents = 2;
    componentType = GL_FLOAT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::Float3:
    numComponents = 3;
    componentType = GL_FLOAT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::Float4:
    numComponents = 4;
    componentType = GL_FLOAT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::Byte1:
    numComponents = 1;
    componentType = GL_BYTE;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::Byte2:
    numComponents = 2;
    componentType = GL_BYTE;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::Byte3:
    numComponents = 3;
    componentType = GL_BYTE;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::Byte4:
    numComponents = 4;
    componentType = GL_BYTE;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::UByte1:
    numComponents = 1;
    componentType = GL_UNSIGNED_BYTE;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::UByte2:
    numComponents = 2;
    componentType = GL_UNSIGNED_BYTE;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::UByte3:
    numComponents = 3;
    componentType = GL_UNSIGNED_BYTE;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::UByte4:
    numComponents = 4;
    componentType = GL_UNSIGNED_BYTE;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::Short1:
    numComponents = 1;
    componentType = GL_SHORT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::Short2:
    numComponents = 2;
    componentType = GL_SHORT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::Short3:
    numComponents = 3;
    componentType = GL_SHORT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::Short4:
    numComponents = 4;
    componentType = GL_SHORT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::UShort1:
    numComponents = 1;
    componentType = GL_UNSIGNED_SHORT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::UShort2:
    numComponents = 2;
    componentType = GL_UNSIGNED_SHORT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::UShort3:
    numComponents = 3;
    componentType = GL_UNSIGNED_SHORT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::UShort4:
    numComponents = 4;
    componentType = GL_UNSIGNED_SHORT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::Byte1Norm:
    numComponents = 1;
    componentType = GL_BYTE;
    normalized = GL_TRUE;
    break;

  case VertexAttributeFormat::Byte2Norm:
    numComponents = 2;
    componentType = GL_BYTE;
    normalized = GL_TRUE;
    break;

  case VertexAttributeFormat::Byte3Norm:
    numComponents = 3;
    componentType = GL_BYTE;
    normalized = GL_TRUE;
    break;

  case VertexAttributeFormat::Byte4Norm:
    numComponents = 4;
    componentType = GL_BYTE;
    normalized = GL_TRUE;
    break;

  case VertexAttributeFormat::UByte1Norm:
    numComponents = 1;
    componentType = GL_UNSIGNED_BYTE;
    normalized = GL_TRUE;
    break;

  case VertexAttributeFormat::UByte2Norm:
    numComponents = 2;
    componentType = GL_UNSIGNED_BYTE;
    normalized = GL_TRUE;
    break;

  case VertexAttributeFormat::UByte3Norm:
    numComponents = 3;
    componentType = GL_UNSIGNED_BYTE;
    normalized = GL_TRUE;
    break;

  case VertexAttributeFormat::UByte4Norm:
    numComponents = 4;
    componentType = GL_UNSIGNED_BYTE;
    normalized = GL_TRUE;
    break;

  case VertexAttributeFormat::Short1Norm:
    numComponents = 1;
    componentType = GL_SHORT;
    normalized = GL_TRUE;
    break;

  case VertexAttributeFormat::Short2Norm:
    numComponents = 2;
    componentType = GL_SHORT;
    normalized = GL_TRUE;
    break;

  case VertexAttributeFormat::Short3Norm:
    numComponents = 3;
    componentType = GL_SHORT;
    normalized = GL_TRUE;
    break;

  case VertexAttributeFormat::Short4Norm:
    numComponents = 4;
    componentType = GL_SHORT;
    normalized = GL_TRUE;
    break;

  case VertexAttributeFormat::UShort1Norm:
    numComponents = 1;
    componentType = GL_UNSIGNED_SHORT;
    normalized = GL_TRUE;
    break;

  case VertexAttributeFormat::UShort2Norm:
    numComponents = 2;
    componentType = GL_UNSIGNED_SHORT;
    normalized = GL_TRUE;
    break;

  case VertexAttributeFormat::UShort3Norm:
    numComponents = 3;
    componentType = GL_UNSIGNED_SHORT;
    normalized = GL_TRUE;
    break;

  case VertexAttributeFormat::UShort4Norm:
    numComponents = 4;
    componentType = GL_UNSIGNED_SHORT;
    normalized = GL_TRUE;
    break;

  case VertexAttributeFormat::Int1:
    numComponents = 1;
    componentType = GL_INT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::Int2:
    numComponents = 2;
    componentType = GL_INT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::Int3:
    numComponents = 3;
    componentType = GL_INT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::Int4:
    numComponents = 4;
    componentType = GL_INT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::UInt1:
    numComponents = 1;
    componentType = GL_UNSIGNED_INT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::UInt2:
    numComponents = 2;
    componentType = GL_UNSIGNED_INT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::UInt3:
    numComponents = 3;
    componentType = GL_UNSIGNED_INT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::UInt4:
    numComponents = 4;
    componentType = GL_UNSIGNED_INT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::HalfFloat1:
    numComponents = 1;
    componentType = GL_HALF_FLOAT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::HalfFloat2:
    numComponents = 2;
    componentType = GL_HALF_FLOAT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::HalfFloat3:
    numComponents = 3;
    componentType = GL_HALF_FLOAT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::HalfFloat4:
    numComponents = 4;
    componentType = GL_HALF_FLOAT;
    normalized = GL_FALSE;
    break;

  case VertexAttributeFormat::Int_2_10_10_10_REV:
    numComponents = 4;
    componentType = GL_INT_2_10_10_10_REV;
    normalized = GL_TRUE;
    break;

    // Purposely not have a default case so we can catch missing enum at build
    // time. The current assumption is all IGL attribute has a corresponding
    // GL attribute
  }
}

Result VertexInputState::create(const VertexInputStateDesc& desc) {
  if (desc.numAttributes == 0) {
    return Result();
  }

  if (desc.numInputBindings == 1) {
    // All the attributed should have the same bufferIndex
    const int bufferIndex = desc.attributes[0].bufferIndex;
    for (int i = 1; i < desc.numAttributes; i++) {
      if (desc.attributes[i].bufferIndex != bufferIndex) {
        return Result{
            Result::Code::ArgumentInvalid,
            "numInputBindings is 1; So all the attributes must have the same bufferIndex"};
      }
    }
  }

  // Process the incoming attributes and associate them with buffers
  for (size_t i = 0; i < desc.numAttributes; i++) {
    OGLAttribute attribInfo;

    const size_t bufferIndex = desc.attributes[i].bufferIndex;

    attribInfo.name = desc.attributes[i].name;
    attribInfo.stride = desc.inputBindings[bufferIndex].stride;
    attribInfo.bufferOffset = desc.attributes[i].offset;

    toOGLAttribute(desc.attributes[i],
                   attribInfo.numComponents,
                   attribInfo.componentType,
                   attribInfo.normalized);

    attribInfo.sampleFunction = desc.inputBindings[bufferIndex].sampleFunction;
    attribInfo.sampleRate = desc.inputBindings[bufferIndex].sampleRate;

    bufferOGLAttribMap_[bufferIndex].push_back(attribInfo);
  }

  return Result();
}

const std::vector<OGLAttribute>& VertexInputState::getAssociatedAttributes(size_t bufferIndex) {
  return bufferOGLAttribMap_[bufferIndex];
}

} // namespace igl::opengl
