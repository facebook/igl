/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#include "ParametricVertexData.h"

#include <igl/IGL.h>

namespace iglu::vertexdata {

// Assumption: <name, location> for OpenGL and Metal, respectively
static const std::pair<const char*, int> s_attrPosition("a_position", 0);
static const std::pair<const char*, int> s_attrUV("a_uv", 1);

namespace Quad {

igl::VertexInputStateDesc inputStateDesc() {
  igl::VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 2;
  inputDesc.attributes[0] = igl::VertexAttribute(0,
                                                 igl::VertexAttributeFormat::Float3,
                                                 offsetof(VertexPosUv, position),
                                                 s_attrPosition.first,
                                                 s_attrPosition.second);
  inputDesc.attributes[1] = igl::VertexAttribute(0,
                                                 igl::VertexAttributeFormat::Float2,
                                                 offsetof(VertexPosUv, uv),
                                                 s_attrUV.first,
                                                 s_attrUV.second);
  inputDesc.numInputBindings = 1;
  inputDesc.inputBindings[0].stride = sizeof(VertexPosUv);
  return inputDesc;
}

std::shared_ptr<VertexData> create(igl::IDevice& device,
                                   iglu::simdtypes::float2 posMin,
                                   iglu::simdtypes::float2 posMax,
                                   iglu::simdtypes::float2 uvMin,
                                   iglu::simdtypes::float2 uvMax) {
  // - UV origin: bottom left
  // - Vertex layout:
  // 0 -- 2
  // |    |
  // |    |
  // 1 -- 3
  const VertexPosUv vertexData[] = {
      {{posMin[0], posMax[1], 0.0}, {uvMin[0], uvMax[1]}},
      {{posMin[0], posMin[1], 0.0}, {uvMin[0], uvMin[1]}},
      {{posMax[0], posMax[1], 0.0}, {uvMax[0], uvMax[1]}},
      {{posMax[0], posMin[1], 0.0}, {uvMax[0], uvMin[1]}},
  };
  const uint16_t indexData[] = {0, 1, 2, 3};

  const igl::BufferDesc vbDesc(
      igl::BufferDesc::BufferTypeBits::Vertex, vertexData, sizeof(vertexData));
  const igl::BufferDesc ibDesc(
      igl::BufferDesc::BufferTypeBits::Index, indexData, sizeof(indexData));

  const igl::VertexInputStateDesc inputDesc = inputStateDesc();
  const std::shared_ptr<igl::IVertexInputState> vertexInput =
      device.createVertexInputState(inputDesc, nullptr);

  PrimitiveDesc primitiveDesc;
  primitiveDesc.numEntries = sizeof(indexData) / sizeof(indexData[0]);

  std::shared_ptr<VertexData> vertData =
      std::make_shared<VertexData>(vertexInput,
                                   device.createBuffer(vbDesc, nullptr),
                                   device.createBuffer(ibDesc, nullptr),
                                   igl::IndexFormat::UInt16,
                                   primitiveDesc,
                                   igl::PrimitiveType::TriangleStrip);
  return vertData;
}

} // namespace Quad

namespace RenderToTextureQuad {

igl::VertexInputStateDesc inputStateDesc() {
  return Quad::inputStateDesc();
}

std::shared_ptr<VertexData> create(igl::IDevice& device,
                                   iglu::simdtypes::float2 posMin,
                                   iglu::simdtypes::float2 posMax,
                                   iglu::simdtypes::float2 uvMin,
                                   iglu::simdtypes::float2 uvMax) {
  iglu::simdtypes::float2 uvMin_adjusted = uvMin;
  iglu::simdtypes::float2 uvMax_adjusted = uvMax;

  // Here's how to think about the conventions that led to this workaround.
  //
  // Summary of conventions:
  // - In OpenGL, all origins (texture, framebuffer, clip) are the bottom left corner.
  // - In Metal, texture and framebuffer origins are the top left corner.
  // - The conventions in this library follow OpenGL. For example:
  //   - This file creates VertexData with UV origin at the bottom left.
  //   - The first pixel in a texture is expected to be the bottom left.
  //   - ForwardRenderPass assumes the viewport (framebuffer space) origin is the bottom left.
  // - We are forced to have our own conventions because the graphics APIs have their own
  //   conventions that aren't compatible with each other. Furthermore, we must correct for them.
  //
  // Correcting for discrepancies across graphics APIs:
  // - Although we can modify texture content, texture coordinates, viewport and clip space at
  //   will to handle discrepancies across graphics APIs, none provide a way to alter the origin
  //   of a framebuffer.
  // - When a framebuffer color attachment is used a shader program input, things break. Our
  //   convention is to use bottom left origin for texture data, which matches the origin of OpenGL
  //   framebuffer; all good. However, in Metal, the origin of framebuffer attachments is top left,
  //   so our color attachment texture is flipped in relation to textures loaded from images.
  // - If we followed Metal's coordinate conventions instead, we'd have the same problem but in
  //   OpenGL. This problem can't be avoided because the APIs don't allow us to compensate for
  //   our conventions, no matter what they are.
  //
  // This workaround doesn't cover use cases like:
  // - Imported 3D meshes
  // - Shaders that use a combination of color attachments and image-based textures as input
  // A general solution to this problem would involve being able to tell shader code about the
  // orientation of every input texture and a strict set of conventions in shader code to ensure
  // texture sampling accounts for that information.
  //
  // Some external resources I found useful for understanding this issue:
  // - https://veldrid.dev/articles/backend-differences.html
  // - http://hacksoflife.blogspot.com/2019/04/keeping-blue-side-up-coordinate.html
  //
  if (device.getBackendType() == igl::BackendType::Metal) {
    uvMin_adjusted[1] = 1.0f - uvMin_adjusted[1];
    uvMax_adjusted[1] = 1.0f - uvMax_adjusted[1];
  }

  return Quad::create(device, posMin, posMax, uvMin_adjusted, uvMax_adjusted);
}

} // namespace RenderToTextureQuad

} // namespace iglu::vertexdata
