/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/NameHandle.h>
#include <igl/RenderPipelineReflection.h>
#include <igl/Shader.h>
#include <igl/Texture.h>
#include <unordered_map>
#include <vector>

namespace igl {

class IShaderModule;
class IVertexInputState;

/**
 * @brief BlendOp determines how to combine and weight the source and destination fragment values.
 * Some blend operations multiply the source values by
 * a source blend factor (SBF), multiply the destination values by a destination blend factor (DBF),
 * and then combine the results using addition or subtraction. Other blend operations use either a
 * minimum or maximum function to determine the result.
 *
 * Add : Add portions of both source and destination pixel values.
 * Subtract : Subtract a portion of the destination pixel values from a portion of the source.
 * ReverseSubtract : Subtract a portion of the source values from a portion of the destination pixel
 * values.
 * Min : Minimum of the source and destination pixel values.
 * Max : Maximum of the source and
 * destination pixel values.
 */
enum class BlendOp : uint8_t { Add = 0, Subtract, ReverseSubtract, Min, Max };

/**
 * @brief The source and destination blend factors are often needed to complete specification of a
 * blend operation. In most cases, the blend factor for both RGB values (F(rgb)) and alpha values
 * (F(a)) are similar to one another, but in some cases, such as SrcAlphaSaturated, the blend factor
 * is slightly different. Four blend factors (BlendColor, OneMinusBlendColor, BlendAlpha, and
 * OneMinusBlendAlpha) refer to a constant blend color value that is set by the
 * setBlendColor(red:green:blue:alpha:) method of RenderCommandEncoder.
 *
 * Zero : Blend factor of zero.
 * One : Blend factor of one.
 * SrcColor : Blend factor of source values.
 * OneMinusSrcColor : Blend factor of one minus source values.
 * SrcAlpha : Blend factor of source alpha
 * OneMinusSrcAlpha : Blend factor of one minus source alpha.
 * DstColor : Blend factor of destination values.
 * OneMinusDstColor : Blend factor of one minus destination values.
 * DstAlpha : Blend factor of destination alpha.
 * OneMinusDstAlpha : Blend factor of one minus destination alpha.
 * SrcAlphaSaturated : Blend factor of the minimum of
 *                    either source alpha or one minus destination alpha.
 * BlendColor : Blend factor of RGB values.
 * OneMinusBlendColor : Blend factor of one minus RGB values.
 * BlendAlpha : Blend factor of alpha value.
 * OneMinusBlendAlpha : Blend factor of one minus alpha value.
 * Src1Color : Blend factor of source values. This option supports dual-source blending
 *             and reads from the second color output of the fragment function.
 * OneMinusSrc1Color : Blend factor of one minus source values. This option supports dual-source
 *                     blending and reads from the second color output of the fragment function.
 * Src1Alpha : Blend factor of source alpha. This option supports dual-source blending
 *             and reads from the second color output of the fragment function.
 * OneMinusSrc1Alpha : Blend factor of one minus source alpha. This option supports dual-source
 *                     blending and reads from the second color output of the fragment function.
 */
enum class BlendFactor : uint8_t {
  Zero = 0,
  One,
  SrcColor,
  OneMinusSrcColor,
  SrcAlpha,
  OneMinusSrcAlpha,
  DstColor,
  OneMinusDstColor,
  DstAlpha,
  OneMinusDstAlpha,
  SrcAlphaSaturated,
  BlendColor,
  OneMinusBlendColor,
  BlendAlpha,
  OneMinusBlendAlpha,
  Src1Color,
  OneMinusSrc1Color,
  Src1Alpha,
  OneMinusSrc1Alpha
};

/*
 * @brief Values used to specify a mask to permit or restrict writing to color channels of a color
 * value. The values red, green, blue, and alpha select one color channel each, and they can be
 * bitwise combined.
 */
using ColorWriteMask = uint8_t;
enum ColorWriteBits : uint8_t {
  ColorWriteBitsDisabled = 0,
  ColorWriteBitsRed = 1 << 0,
  ColorWriteBitsGreen = 1 << 1,
  ColorWriteBitsBlue = 1 << 2,
  ColorWriteBitsAlpha = 1 << 3,
  ColorWriteBitsAll =
      ColorWriteBitsRed | ColorWriteBitsGreen | ColorWriteBitsBlue | ColorWriteBitsAlpha,
};

/*
 * @brief Control polygon rasterization modes
 * Fill : Polygons are rendered using standard polygon rasterization rules
 * Line : Polygon edges are drawn as line segments
 */
enum class PolygonFillMode : uint8_t {
  Fill = 0,
  Line = 1,
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
      ColorWriteMask colorWriteMask = ColorWriteBitsAll;
      bool blendEnabled = false;
      /*
       * @brief Assign the blend operations for RGB and alpha pixel data.
       */
      BlendOp rgbBlendOp = BlendOp::Add;
      BlendOp alphaBlendOp = BlendOp::Add;
      /*
       * @brief Assign the source and destination blend factors.
       */
      BlendFactor srcRGBBlendFactor = BlendFactor::One;
      BlendFactor srcAlphaBlendFactor = BlendFactor::One;
      BlendFactor dstRGBBlendFactor = BlendFactor::Zero;
      BlendFactor dstAlphaBlendFactor = BlendFactor::Zero;
      ColorAttachment() = default;
      bool operator==(const ColorAttachment& other) const;
      bool operator!=(const ColorAttachment& other) const;
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
    bool operator==(const TargetDesc& other) const;
    bool operator!=(const TargetDesc& other) const;
  };

  /*
   * @brief Describes the primitive topology for this graphics pipeline
   */
  PrimitiveType topology = PrimitiveType::Triangle;

  /*
   * @brief Describes the organization of per-vertex input data passed to a vertex shader function
   */
  std::shared_ptr<IVertexInputState> vertexInputState;

  /*
   * @brief Describes the vertex and fragment functions
   */
  std::shared_ptr<IShaderStages> shaderStages;

  TargetDesc targetDesc;
  CullMode cullMode = igl::CullMode::Disabled;
  WindingMode frontFaceWinding = igl::WindingMode::CounterClockwise;
  PolygonFillMode polygonFillMode = igl::PolygonFillMode::Fill;

  /*
   * GL Only: Mapping of Texture Unit <-> Sampler Name
   * Texture unit should be < IGL_TEXTURE_SAMPLERS_MAX
   */
  std::unordered_map<size_t, igl::NameHandle> vertexUnitSamplerMap;
  std::unordered_map<size_t, igl::NameHandle> fragmentUnitSamplerMap;

  /*
   * GL Only: Mapping of Uniform Block Binding points <-> Uniform Block Names
   * Uniform Block Binding Point should be < IGL_UNIFORM_BLOCKS_BINDING_MAX
   * Names are a pair as, depending on shader implementation, OpenGL reflection
   * may find a block by its block name or its instance name.
   *
   * This should only be populated if explicit binding is not supported or used.
   */
  std::unordered_map<size_t, std::pair<igl::NameHandle, igl::NameHandle>> uniformBlockBindingMap;

  int sampleCount = 1;

  igl::NameHandle debugName;

  bool operator==(const RenderPipelineDesc& other) const;
  bool operator!=(const RenderPipelineDesc& other) const;
};

class IRenderPipelineState {
 public:
  explicit IRenderPipelineState(const RenderPipelineDesc& desc) : desc_(desc) {}
  virtual ~IRenderPipelineState() = default;

  virtual std::shared_ptr<IRenderPipelineReflection> renderPipelineReflection() = 0;
  virtual void setRenderPipelineReflection(
      const IRenderPipelineReflection& renderPipelineReflection) = 0;

  virtual int getIndexByName(const NameHandle& /* name */, ShaderStage /* stage */) const {
    return -1;
  }

  virtual int getIndexByName(const std::string& /* name */, ShaderStage /* stage */) const {
    return -1;
  }

  const RenderPipelineDesc& getRenderPipelineDesc() const {
    return desc_;
  }

 protected:
  const RenderPipelineDesc desc_{};
};

} // namespace igl

/// Hashing function declarations
///
namespace std {

template<>
struct hash<igl::RenderPipelineDesc> {
  size_t operator()(igl::RenderPipelineDesc const& /*key*/) const;
};

template<>
struct hash<igl::RenderPipelineDesc::TargetDesc> {
  size_t operator()(igl::RenderPipelineDesc::TargetDesc const& /*key*/) const;
};

template<>
struct hash<igl::RenderPipelineDesc::TargetDesc::ColorAttachment> {
  size_t operator()(igl::RenderPipelineDesc::TargetDesc::ColorAttachment const& /*key*/) const;
};

} // namespace std
