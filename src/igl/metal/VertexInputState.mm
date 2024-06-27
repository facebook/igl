/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/VertexInputState.h>

namespace igl::metal {

metal::VertexInputState::VertexInputState(MTLVertexDescriptor* value) : value_(value) {}

MTLVertexFormat VertexInputState::convertAttributeFormat(VertexAttributeFormat value) {
  switch (value) {
  case VertexAttributeFormat::Float1:
    return MTLVertexFormatFloat;
  case VertexAttributeFormat::Float2:
    return MTLVertexFormatFloat2;
  case VertexAttributeFormat::Float3:
    return MTLVertexFormatFloat3;
  case VertexAttributeFormat::Float4:
    return MTLVertexFormatFloat4;

  case VertexAttributeFormat::Byte2:
    return MTLVertexFormatChar2;
  case VertexAttributeFormat::Byte3:
    return MTLVertexFormatChar3;
  case VertexAttributeFormat::Byte4:
    return MTLVertexFormatChar4;
  case VertexAttributeFormat::UByte2:
    return MTLVertexFormatUChar2;
  case VertexAttributeFormat::UByte3:
    return MTLVertexFormatUChar3;
  case VertexAttributeFormat::UByte4:
    return MTLVertexFormatUChar4;

  case VertexAttributeFormat::Short2:
    return MTLVertexFormatShort2;
  case VertexAttributeFormat::Short3:
    return MTLVertexFormatShort3;
  case VertexAttributeFormat::Short4:
    return MTLVertexFormatShort4;
  case VertexAttributeFormat::UShort2:
    return MTLVertexFormatUShort2;
  case VertexAttributeFormat::UShort3:
    return MTLVertexFormatUShort3;
  case VertexAttributeFormat::UShort4:
    return MTLVertexFormatUShort4;

  case VertexAttributeFormat::Byte1Norm:
    return MTLVertexFormatCharNormalized;
  case VertexAttributeFormat::Byte2Norm:
    return MTLVertexFormatChar2Normalized;
  case VertexAttributeFormat::Byte3Norm:
    return MTLVertexFormatChar3Normalized;
  case VertexAttributeFormat::Byte4Norm:
    return MTLVertexFormatChar4Normalized;

  case VertexAttributeFormat::UByte1Norm:
    return MTLVertexFormatUCharNormalized;
  case VertexAttributeFormat::UByte2Norm:
    return MTLVertexFormatUChar2Normalized;
  case VertexAttributeFormat::UByte3Norm:
    return MTLVertexFormatUChar3Normalized;
  case VertexAttributeFormat::UByte4Norm:
    return MTLVertexFormatUChar4Normalized;

  case VertexAttributeFormat::Short1Norm:
    return MTLVertexFormatShortNormalized;
  case VertexAttributeFormat::Short2Norm:
    return MTLVertexFormatShort2Normalized;
  case VertexAttributeFormat::Short3Norm:
    return MTLVertexFormatShort3Normalized;
  case VertexAttributeFormat::Short4Norm:
    return MTLVertexFormatShort4Normalized;

  case VertexAttributeFormat::UShort1Norm:
    return MTLVertexFormatUShortNormalized;
  case VertexAttributeFormat::UShort2Norm:
    return MTLVertexFormatUShort2Normalized;
  case VertexAttributeFormat::UShort3Norm:
    return MTLVertexFormatUShort3Normalized;
  case VertexAttributeFormat::UShort4Norm:
    return MTLVertexFormatUShort4Normalized;

  case VertexAttributeFormat::Byte1:
  case VertexAttributeFormat::UByte1:
  case VertexAttributeFormat::Short1:
  case VertexAttributeFormat::UShort1:
    return MTLVertexFormatInvalid;

  case VertexAttributeFormat::Int1:
    return MTLVertexFormatInt;
  case VertexAttributeFormat::Int2:
    return MTLVertexFormatInt2;
  case VertexAttributeFormat::Int3:
    return MTLVertexFormatInt3;
  case VertexAttributeFormat::Int4:
    return MTLVertexFormatInt4;

  case VertexAttributeFormat::UInt1:
    return MTLVertexFormatUInt;
  case VertexAttributeFormat::UInt2:
    return MTLVertexFormatUInt2;
  case VertexAttributeFormat::UInt3:
    return MTLVertexFormatUInt3;
  case VertexAttributeFormat::UInt4:
    return MTLVertexFormatUInt4;

  case VertexAttributeFormat::HalfFloat1:
    return MTLVertexFormatHalf;
  case VertexAttributeFormat::HalfFloat2:
    return MTLVertexFormatHalf2;
  case VertexAttributeFormat::HalfFloat3:
    return MTLVertexFormatHalf3;
  case VertexAttributeFormat::HalfFloat4:
    return MTLVertexFormatHalf4;

  case VertexAttributeFormat::Int_2_10_10_10_REV:
    return MTLVertexFormatInt1010102Normalized;

    // The 'default' case below is commented out, so that compiler will catch and error
    // on any unhandled cases.

    // default:
    //  return MTLVertexFormatInvalid;
  }

  return MTLVertexFormatInvalid;
}

MTLVertexStepFunction VertexInputState::convertSampleFunction(VertexSampleFunction value) {
  switch (value) {
  case VertexSampleFunction::Constant:
    return MTLVertexStepFunctionConstant;
  case VertexSampleFunction::PerVertex:
    return MTLVertexStepFunctionPerVertex;
  case VertexSampleFunction::Instance:
    return MTLVertexStepFunctionPerInstance;
  }
}
} // namespace igl::metal
