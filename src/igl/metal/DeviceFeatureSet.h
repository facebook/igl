/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/DeviceFeatures.h>
#include <igl/metal/PlatformDevice.h>

namespace igl {
namespace metal {

struct DeviceFeatureDesc {
  DeviceFeatureDesc() = default;
  DeviceFeatureDesc(MTLFeatureSet featureSet, size_t gpuFamily, size_t gpuVersion) :
    featureSet(featureSet), gpuFamily(gpuFamily), gpuVersion(gpuVersion) {}
  MTLFeatureSet featureSet;
  size_t gpuFamily;
  size_t gpuVersion;
  bool isValid;
};

class DeviceFeatureSet final {
 public:
  explicit DeviceFeatureSet(id<MTLDevice> device) {
    getFeatureSet(device);
  }
  ~DeviceFeatureSet() = default;

  bool hasFeature(DeviceFeatures feature) const;

  bool hasRequirement(DeviceRequirement requirement) const;

  bool getFeatureLimits(DeviceFeatureLimits featureLimits, size_t& result) const;

  ICapabilities::TextureFormatCapabilities getTextureFormatCapabilities(TextureFormat format) const;

 private:
  void getFeatureSet(id<MTLDevice> device);

  // find the highest supported feature set
  void findHighestFeatureSet(id<MTLDevice> device,
                             const std::vector<DeviceFeatureDesc>& featureSet,
                             DeviceFeatureDesc& result);

  DeviceFeatureDesc deviceFeatureDesc_;
  size_t maxMultisampleCount_;
  size_t maxBufferLength_;
};

} // namespace metal
} // namespace igl
