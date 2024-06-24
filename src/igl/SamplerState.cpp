/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/SamplerState.h>

#include <igl/Assert.h>
#include <igl/Common.h>

namespace igl {

bool SamplerStateDesc::operator==(const SamplerStateDesc& rhs) const {
  return (minFilter == rhs.minFilter) && (magFilter == rhs.magFilter) &&
         (mipFilter == rhs.mipFilter) && (addressModeU == rhs.addressModeU) &&
         (addressModeV == rhs.addressModeV) && (addressModeW == rhs.addressModeW) &&
         (depthCompareFunction == rhs.depthCompareFunction) && (mipLodMin == rhs.mipLodMin) &&
         (mipLodMax == rhs.mipLodMax) && (maxAnisotropic == rhs.maxAnisotropic) &&
         (depthCompareEnabled == rhs.depthCompareEnabled) && (yuvFormat == rhs.yuvFormat);
}

bool SamplerStateDesc::operator!=(const SamplerStateDesc& rhs) const {
  return !operator==(rhs);
}

} // namespace igl

size_t std::hash<igl::SamplerStateDesc>::operator()(const igl::SamplerStateDesc& key) const {
  IGL_ASSERT_MSG(key.maxAnisotropic >= 1 && key.maxAnisotropic <= 16,
                 "[IGL] SamplerStateDesc::maxAnisotropic is out of range: %su",
                 key.maxAnisotropic);
  IGL_ASSERT_MSG(
      key.mipLodMin < 16, "[IGL] SamplerStateDesc::mipLodMin is out of range: %su", key.mipLodMin);
  IGL_ASSERT_MSG(key.mipLodMax < 16 && key.mipLodMin <= key.mipLodMax,
                 "[IGL] SamplerStateDesc::mipLodMax is out of range: %su",
                 key.mipLodMax);

  // clang-format off
  const size_t hash = igl::EnumToValue(key.minFilter) |         // 0,1: 1 bit field
                igl::EnumToValue(key.magFilter) << 1 |    // 0,1: 1 bit field
                igl::EnumToValue(key.mipFilter) << 2 |    // 0,1: 1 bit field
                igl::EnumToValue(key.addressModeU) << 4 | // 0,1,2: 2 bit field
                igl::EnumToValue(key.addressModeV) << 6 | // 0,1,2: 2 bit field
                igl::EnumToValue(key.addressModeW) << 8 | // 0,1,2: 2 bit field
                (key.maxAnisotropic - 1) << 10 | // subtract 1 so it fits 4 bits
                key.mipLodMin << 14 |                     // [0, 15]: 4 bit field
                key.mipLodMax << 18 |                     // [0, 15]: 4 bit field
                igl::EnumToValue(key.depthCompareFunction) << 22 | // [0, 7]: 3 bit field
                (key.depthCompareEnabled ? 1u : 0u) << 25 | // 0,1: 1 bit field
                igl::EnumToValue(key.yuvFormat) << 26;      // 0,255: 8 bit field
  // clang-format on

  return hash;
}
