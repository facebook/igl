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
 * MultiSample                Supports multisample textures
 * MultiSampleResolve         Supports GPU multisampled texture resolve
 * TextureFilterAnisotropic   Supports Anisotropic texture filtering
 */
enum class DeviceFeatures {
  MultiSample,
  MultiSampleResolve,
  TextureFilterAnisotropic,
};
// clang-format on

/**
 * @brief DeviceFeatureLimits provides specific limitations on certain features supported on the
 * device
 *
 * MaxDimensionCube             Maximum cube map dimensions
 * MaxDimension1D2D             Maximum texture dimensions
 * MaxSamples                   Maximum number of samples
 * MaxPushConstantBytes         Maximum number of bytes for Push Constants
 * MaxUniformBufferBytes        Maximum number of bytes for a uniform buffer
 */
enum class DeviceFeatureLimits {
  MaxDimension1D2D,
  MaxDimensionCube,
  MaxSamples,
  MaxPushConstantBytes,
  MaxUniformBufferBytes,
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

 protected:
  virtual ~ICapabilities() = default;
};

} // namespace igl
