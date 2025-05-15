/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <utility>
#include <igl/Common.h>

namespace igl {

/**
 * @brief Represents vertex attribute data types for both scalar and vector values
 */
enum class VertexAttributeFormat {
  Float1 = 0,
  Float2,
  Float3,
  Float4,

  Byte1,
  Byte2,
  Byte3,
  Byte4,

  UByte1,
  UByte2,
  UByte3,
  UByte4,

  Short1,
  Short2,
  Short3,
  Short4,

  UShort1,
  UShort2,
  UShort3,
  UShort4,

  // Normalized variants
  Byte1Norm,
  Byte2Norm,
  Byte3Norm,
  Byte4Norm,

  UByte1Norm,
  UByte2Norm,
  UByte3Norm,
  UByte4Norm,

  Short1Norm,
  Short2Norm,
  Short3Norm,
  Short4Norm,

  UShort1Norm,
  UShort2Norm,
  UShort3Norm,
  UShort4Norm,

  Int1,
  Int2,
  Int3,
  Int4,

  UInt1,
  UInt2,
  UInt3,
  UInt4,

  // packed formats
  HalfFloat1,
  HalfFloat2,
  HalfFloat3,
  HalfFloat4,

  Int_2_10_10_10_REV, // standard format to store normal vectors
};

/**
 * @brief Controls how vertex attribute streams are consumed, per-vertex or per-instance
 */
enum class VertexSampleFunction {
  Constant,
  PerVertex,
  Instance,

  // Missing Tessellation support
};

/**
 * @brief Generic definition of a vertex attribute stream
 */
struct VertexAttribute {
  /** @brief A buffer which contains this attribute stream */
  size_t bufferIndex = 0;
  /** @brief Per-element format */
  VertexAttributeFormat format = VertexAttributeFormat::Float1;
  /** @brief An offset where the first element of this attribute stream starts */
  uintptr_t offset = 0;
  std::string name; // GLES Only
  int location = -1; // Metal only

  bool operator==(const VertexAttribute& other) const;
  bool operator!=(const VertexAttribute& other) const;
};

/**
 * @brief Defines a binding point for a vertex stream
 */
struct VertexInputBinding {
  size_t stride = 0;
  VertexSampleFunction sampleFunction = VertexSampleFunction::PerVertex;
  size_t sampleRate = 1;

  bool operator==(const VertexInputBinding& other) const;
  bool operator!=(const VertexInputBinding& other) const;
};

/**
 * @brief Defines input to a vertex shader
 */
struct VertexInputStateDesc {
  size_t numAttributes = 0;
  VertexAttribute attributes[IGL_VERTEX_ATTRIBUTES_MAX];
  size_t numInputBindings = 0;
  VertexInputBinding inputBindings[IGL_BUFFER_BINDINGS_MAX];
  static size_t sizeForVertexAttributeFormat(VertexAttributeFormat format);

  bool operator==(const VertexInputStateDesc& other) const;
};

/**
 * @brief Represents input to a vertex shader in a form of an object which can be used with
 * RenderPipelineState
 */
class IVertexInputState {
 public:
 protected:
  IVertexInputState() = default;
  virtual ~IVertexInputState() = default;
};

} // namespace igl

// Hashing function declarations
namespace std {
template<>
struct hash<igl::VertexInputStateDesc> {
  size_t operator()(const igl::VertexInputStateDesc& /*key*/) const;
};

template<>
struct hash<igl::VertexInputBinding> {
  size_t operator()(const igl::VertexInputBinding& /*key*/) const;
};

template<>
struct hash<igl::VertexAttribute> {
  size_t operator()(const igl::VertexAttribute& /*key*/) const;
};

} // namespace std
