/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/Shader.h>
#include <igl/Texture.h>
#include <unordered_map>
#include <vector>

namespace igl {

class IShaderModule;

enum BlendOp : uint8_t {
  BlendOp_Add = 0,
  BlendOp_Subtract,
  BlendOp_ReverseSubtract,
  BlendOp_Min,
  BlendOp_Max
};

enum BlendFactor : uint8_t {
  BlendFactor_Zero = 0,
  BlendFactor_One,
  BlendFactor_SrcColor,
  BlendFactor_OneMinusSrcColor,
  BlendFactor_SrcAlpha,
  BlendFactor_OneMinusSrcAlpha,
  BlendFactor_DstColor,
  BlendFactor_OneMinusDstColor,
  BlendFactor_DstAlpha,
  BlendFactor_OneMinusDstAlpha,
  BlendFactor_SrcAlphaSaturated,
  BlendFactor_BlendColor,
  BlendFactor_OneMinusBlendColor,
  BlendFactor_BlendAlpha,
  BlendFactor_OneMinusBlendAlpha,
  BlendFactor_Src1Color,
  BlendFactor_OneMinusSrc1Color,
  BlendFactor_Src1Alpha,
  BlendFactor_OneMinusSrc1Alpha
};

using ColorWriteBits = uint8_t;

enum ColorWriteMask : ColorWriteBits {
  ColorWriteMask_Disabled = 0,
  ColorWriteMask_Red = 1 << 0,
  ColorWriteMask_Green = 1 << 1,
  ColorWriteMask_Blue = 1 << 2,
  ColorWriteMask_Alpha = 1 << 3,
  ColorWriteMask_All =
      ColorWriteMask_Red | ColorWriteMask_Green | ColorWriteMask_Blue | ColorWriteMask_Alpha,
};

enum PolygonMode : uint8_t {
  PolygonMode_Fill = 0,
  PolygonMode_Line = 1,
};

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

  Byte2Norm,
  Byte4Norm,

  UByte2Norm,
  UByte4Norm,

  Short2Norm,
  Short4Norm,

  UShort2Norm,
  UShort4Norm,

  Int1,
  Int2,
  Int3,
  Int4,

  UInt1,
  UInt2,
  UInt3,
  UInt4,

  HalfFloat1,
  HalfFloat2,
  HalfFloat3,
  HalfFloat4,

  Int_2_10_10_10_REV, // standard format to store normal vectors
};

/**
 * @brief Controls how vertex attribute streams are consumed, per-vertex or per-instance
 */
enum VertexSampleFunction {
  VertexSampleFunction_PerVertex,
  VertexSampleFunction_Instance,
};

/**
 * @brief Generic definition of a vertex attribute stream
 */
struct VertexAttribute {
  /** @brief A buffer which contains this attribute stream */
  uint32_t bufferIndex = 0;
  /** @brief Per-element format */
  VertexAttributeFormat format = VertexAttributeFormat::Float1;
  /** @brief An offset where the first element of this attribute stream starts */
  uintptr_t offset = 0;
  uint32_t location = 0;

  VertexAttribute() = default;
  VertexAttribute(size_t bufferIndex,
                  VertexAttributeFormat format,
                  uintptr_t offset,
                  uint32_t location) :
    bufferIndex(bufferIndex), format(format), offset(offset), location(location) {}
};

/**
 * @brief Defines a binding point for a vertex stream
 */
struct VertexInputBinding {
  uint32_t stride = 0;
  VertexSampleFunction sampleFunction = VertexSampleFunction_PerVertex;
  size_t sampleRate = 1;

  VertexInputBinding() = default;
};

/**
 * @brief Defines input to a vertex shader
 */
struct VertexInputStateDesc {
  size_t numAttributes = 0;
  VertexAttribute attributes[IGL_VERTEX_ATTRIBUTES_MAX];
  size_t numInputBindings = 0;
  VertexInputBinding inputBindings[IGL_VERTEX_BUFFER_MAX];
};

/*
 * @brief An argument of options you pass to a device to get a render pipeline state object.
 */
struct RenderPipelineDesc {
  struct TargetDesc {
    /*
     * @brief Description of render pipeline's color render target
     */
    struct ColorAttachment {
      TextureFormat textureFormat = TextureFormat::Invalid;
      /*
       * @brief Identify which color channels are blended.
       */
      ColorWriteBits colorWriteBits = ColorWriteMask_All;
      bool blendEnabled = false;
      /*
       * @brief Assign the blend operations for RGB and alpha pixel data.
       */
      BlendOp rgbBlendOp = BlendOp::BlendOp_Add;
      BlendOp alphaBlendOp = BlendOp::BlendOp_Add;
      /*
       * @brief Assign the source and destination blend factors.
       */
      BlendFactor srcRGBBlendFactor = BlendFactor_One;
      BlendFactor srcAlphaBlendFactor = BlendFactor_One;
      BlendFactor dstRGBBlendFactor = BlendFactor_Zero;
      BlendFactor dstAlphaBlendFactor = BlendFactor_Zero;
      ColorAttachment() = default;
    };

    /*
     * @brief Array of attachments that store color data
     */
    std::vector<ColorAttachment> colorAttachments;
    /*
     * @brief Pixel format of the attachment that stores depth data
     */
    TextureFormat depthAttachmentFormat = TextureFormat::Invalid;
    /*
     * @brief Pixel format of the attachment that stores stencil data
     */
    TextureFormat stencilAttachmentFormat = TextureFormat::Invalid;
  };

  /*
   * @brief Describes the organization of per-vertex input data passed to a vertex shader function
   */
  igl::VertexInputStateDesc vertexInputState;

  /*
   * @brief Describes the vertex and fragment functions
   */
  std::shared_ptr<ShaderStages> shaderStages;

  TargetDesc targetDesc;
  CullMode cullMode = igl::CullMode_None;
  WindingMode frontFaceWinding = igl::WindingMode_CCW;
  PolygonMode polygonMode = igl::PolygonMode_Fill;
    
  int sampleCount = 1;

  std::string debugName;
};

class IRenderPipelineState {
 public:
  IRenderPipelineState() = default;
  virtual ~IRenderPipelineState() = default;
};

} // namespace igl
