/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <igl/Macros.h>
#include <igl/Texture.h>

namespace igl {

// clang-format off
/**
 * @brief DeviceFeatures denotes the different kinds of specific features that are supported in the
 * device. Note that this can differ across devices based on vendor support.
 *
 * BindBytes                  Supports binding bytes (temporary buffers), e.g., setVertexBytes on Metal
 * BindUniform                Supports binding individual uniform values
 * BufferDeviceAddress        Supports buffer device address (bindless buffers)
 * BufferNoCopy               Supports creating buffers that use previously allocated memory
 * BufferRing                 Supports creating ring buffers with memory for each swapchain image
 * Compute                    Supports compute
 * DepthCompare               Supports setting depth compare function
 * DepthShaderRead            Supports reading depth texture from a shader
 * DrawIndexedIndirect        Supports IRenderCommandEncoder::drawIndexedIndirect
 * ExplicitBinding,           Supports uniforms block explicit binding in shaders
 * ExplicitBindingExt,        Supports uniforms block explicit binding in shaders via an extension
 * MapBufferRange             Supports mapping buffer data into client address space
 * MinMaxBlend                Supports Min and Max blend operations
 * MultipleRenderTargets      Supports MRT - Multiple Render Targets
 * MultiSample                Supports multisample textures
 * MultiSampleResolve         Supports GPU multisampled texture resolve
 * Multiview                  Supports multiview
 * PushConstants              Supports push constants(Vulkan)
 * ReadWriteFramebuffer       Supports separate FB reading/writing binding
 * SamplerMinMaxLod           Supports constraining the min and max texture LOD when sampling
 * ShaderLibrary              Supports shader libraries
 * ShaderTextureLod           Supports explicit control of Lod in the shader
 * ShaderTextureLodExt        Supports explicit control of Lod in the shader via an extension
 * SRGB                       Supports sRGB Textures and FrameBuffer
 * StandardDerivative         Supports Standard Derivative function in shader
 * StandardDerivativeExt      Supports Standard Derivative function in shader via an extension
 * Texture2DArray             Supports 2D array textures
 * Texture3D                  Supports 3D textures
 * TextureArrayExt            Supports array textures via an extension
 * TextureBindless            Supports bindless textures
 * TextureExternalImage       Supports reading external images in the shader
 * TextureFilterAnisotropic   Supports Anisotropic texture filtering
 * TextureFloat               Supports full float texture format
 * TextureFormatRG            Supports RG Texture
 * TextureFormatRGB           Supports packed RGB texture formats
 * TextureHalfFloat           Supports half float texture format
 * TextureNotPot              Supports non power-of-two textures
 * TexturePartialMipChain     Supports mip chains that do not go all the way to 1x1
 * UniformBlocks,             Supports uniform blocks
 * ValidationLayersEnabled,   Validation layers are enabled
 */
enum class DeviceFeatures {
  BindBytes = 0,
  BindUniform,
  BufferDeviceAddress,
  BufferNoCopy,
  BufferRing,
  Compute,
  DepthCompare,
  DepthShaderRead,
  DrawIndexedIndirect,
  ExplicitBinding,
  ExplicitBindingExt,
  MapBufferRange,
  MinMaxBlend,
  MultipleRenderTargets,
  MultiSample,
  MultiSampleResolve,
  Multiview,
  PushConstants,
  ReadWriteFramebuffer,
  SamplerMinMaxLod,
  ShaderLibrary,
  ShaderTextureLod,
  ShaderTextureLodExt,
  SRGB,
  SRGBWriteControl,
  StandardDerivative,
  StandardDerivativeExt,
  Texture2DArray,
  TextureArrayExt,
  Texture3D,
  TextureBindless,
  TextureExternalImage,
  TextureFilterAnisotropic,
  TextureFloat,
  TextureFormatRG,
  TextureFormatRGB,
  TextureHalfFloat,
  TextureNotPot,
  TexturePartialMipChain,
  UniformBlocks,
  ValidationLayersEnabled,
};
// clang-format on

/**
 * @brief DeviceRequirement denotes capturing specific requirements for a feature to be enabled.
 * These should be used in combination with DeviceFeatures to understand how to take advantage of
 * the feature.
 * For example, using the StandardDerivate feature with an OpenGL ES 2.0 device requires using an
 * extension whereas it can be used without an extension for an OpenGL or OpenGL ES 3+ device.
 * If a device returns true for hasFeature(DeviceFeatures::ShaderTextureLod) and returns true for
 * hasRequirement(DeviceRequirement::ShaderTextureLodExtReq), then shader code wishing to use the
 * feature must do so via an extension.
 *
 * ExplicitBindingExtReq        Using explicit binding in a shader requires using an extension
 * ShaderTextureLodExtReq       Using Texture LOD in a shader requires using an extension
 * StandardDerivativeExtReq     Standard derivatives in a shader requires using an extension
 * TextureArrayExtReq           Texture array support requires an extension
 * TextureFormatRGExtReq        RG texture format support requires an extension
 */
enum class DeviceRequirement {
  ExplicitBindingExtReq,
  ShaderTextureLodExtReq,
  StandardDerivativeExtReq,
  TextureArrayExtReq,
  TextureFormatRGExtReq,
};

/**
 * @brief DeviceFeatureLimits provides specific limitations on certain features supported on the
 * device
 *
 * BufferAlignment              Required byte alignment for buffer data
 * BufferNoCopyAlignment        Required byte alignment for no copy buffer data
 * MaxBindBytesBytes            Maximum number of bytes that can be bound with bindBytes
 * MaxCubeMapDimension          Maximum cube map dimensions
 * MaxFragmentUniformVectors    Maximum fragment uniform vectors
 * MaxMultisampleCount          Maximum number of samples
 * MaxPushConstantBytes         Maximum number of bytes for Push Constants
 * MaxTextureDimension1D2D      Maximum texture dimensions
 * MaxUniformBufferBytes        Maximum number of bytes for a uniform buffer
 * MaxVertexUniformVectors      Maximum vertex uniform vectors
 * PushConstantsAlignment       Required byte alignment for push constants data
 */
enum class DeviceFeatureLimits {
  BufferAlignment = 0,
  BufferNoCopyAlignment,
  MaxBindBytesBytes,
  MaxCubeMapDimension,
  MaxFragmentUniformVectors,
  MaxMultisampleCount,
  MaxPushConstantBytes,
  MaxTextureDimension1D2D,
  MaxUniformBufferBytes,
  MaxVertexUniformVectors,
  PushConstantsAlignment,
  ShaderStorageBufferOffsetAlignment,
};

/**
 * @brief ShaderFamily provides specific shader family usage
 *
 * Unknown       Unspecified shader family usage
 * Glsl          Regular GL shader language
 * GlslEs        GL Embedded Systems shader language
 * Metal         Metal API (macOS, iOS, etc.)
 * SpirV         Standard Portable Intermediate Representation open standard format
 */
enum class ShaderFamily : uint8_t { Unknown, Glsl, GlslEs, Metal, SpirV };

/**
 * @brief ShaderVersion provides information on the shader family type and version
 */
struct ShaderVersion {
  ShaderFamily family = ShaderFamily::Unknown;
  uint8_t majorVersion = 0;
  uint8_t minorVersion = 0;
  uint8_t extra = 0;

  bool operator==(const ShaderVersion& other) const {
    return family == other.family && majorVersion == other.majorVersion &&
           minorVersion == other.minorVersion && extra == other.extra;
  }

  bool operator!=(const ShaderVersion& other) const {
    return !(*this == other);
  }
};

/**
 * @brief ICapabilities defines the capabilities interface. Currently, it is IDevice
 * which implements this interface.
 */
class ICapabilities {
 public:
  /**
   * @brief TextureFormatCapabilityBits provides specific texture format usage
   *
   * Unsupported       The format is not supported
   * Sampled           Can be used as read-only texture in vertex/fragment shaders
   * SampledFiltered   The texture can be filtered during sampling
   * Filter            The texture can be filtered during sampling
   * Storage           Can be used as read/write storage texture in vertex/fragment/compute shaders
   * Attachment        The texture can be used as a render target
   * SampledAttachment The texture can be both sampled in shaders AND used as a render target
   * All               All capabilities are supported
   *
   * @remark SampledAttachment is NOT the same as Sampled | Attachment.
   */
  enum TextureFormatCapabilityBits : uint8_t {
    Unsupported = 0,
    Sampled = 1 << 0,
    SampledFiltered = 1 << 1,
    Storage = 1 << 2,
    Attachment = 1 << 3,
    SampledAttachment = 1 << 4,
    All = Sampled | SampledFiltered | Storage | Attachment | SampledAttachment
  };

  using TextureFormatCapabilities = uint8_t;

  /**
   * @brief This function indicates if a device feature is supported at all.
   *
   * @param feature The device feature specified
   * @return True,  If feature is supported
   *         False, Otherwise
   */
  virtual bool hasFeature(DeviceFeatures feature) const = 0;

  /**
   * @brief This function indicates if a device requirement is at all present.
   *
   * @param requirement The device requirement specified
   * @return True,      If requirement is present
   *         False,     Otherwise
   */
  virtual bool hasRequirement(DeviceRequirement requirement) const = 0;

  /**
   * @brief This function gets capabilities of a specified texture format
   *
   * @param format The texture format
   * @return TextureFormatCapabilities
   */
  virtual TextureFormatCapabilities getTextureFormatCapabilities(TextureFormat format) const = 0;

  /**
   * @brief This function gets device feature limits and return additional error code in 'result'.
   *
   * @param featureLimits The device feature limits
   * @param result        The error code returned.
   * -1 means an error occurred, greater than 0 means valid results are acquired
   *
   * @return True,        If there are feature limits are present
   *         False,       Otherwise
   */
  virtual bool getFeatureLimits(DeviceFeatureLimits featureLimits, size_t& result) const = 0;

  /**
   * @brief Gets the latest shader language version supported by this device.
   * @return ShaderVersion
   */
  virtual ShaderVersion getShaderVersion() const = 0;

 protected:
  virtual ~ICapabilities() = default;
};

inline bool contains(ICapabilities::TextureFormatCapabilities value,
                     ICapabilities::TextureFormatCapabilityBits flag) {
  return (value & flag) == flag;
}

} // namespace igl
