/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/DeviceFeatures.h>
#include <igl/metal/PlatformDevice.h>

namespace igl::metal {

class DeviceFeatureSet final {
 public:
  explicit DeviceFeatureSet(id<MTLDevice> device);
  ~DeviceFeatureSet() = default;

  [[nodiscard]] bool hasFeature(DeviceFeatures feature) const;

  [[nodiscard]] bool hasRequirement(DeviceRequirement requirement) const;

  bool getFeatureLimits(DeviceFeatureLimits featureLimits, size_t& result) const;

  [[nodiscard]] ICapabilities::TextureFormatCapabilities getTextureFormatCapabilities(
      TextureFormat format) const;

 private:
  // Apple GPU family as defined by MTLGPUFamily and the docs:
  //   https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
  // Apple2: iOS: A8
  // Apple3: iOS: A9, A10
  // Apple4: iOS: A11
  // Apple5: iOS: A12
  // Apple6: iOS: A13
  // Apple7: iOS: A14         Mac: M1
  // Apple8: iOS: A15, A16    Mac: M2
  // Apple9: iOS: A16         Mac: M3
  //
  // MTLGPUFamily enum isn't available until macos(10.15), ios(13.0)
  // also MTLGPUFamily includes enum values that don't directly correspond to a GPU
  // so we use an integer representation that maps to Apple GPU family 2+
  size_t gpuFamily_;

  size_t maxMultisampleCount_;
  size_t maxBufferLength_;
  bool supports32BitFloatFiltering_ = false;
};

} // namespace igl::metal
