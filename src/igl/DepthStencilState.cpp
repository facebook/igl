/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/DepthStencilState.h>

#include <cstring>
#include <unordered_map>

using namespace igl;

bool StencilStateDesc::operator!=(const StencilStateDesc& other) const {
  return !(*this == other);
}

bool StencilStateDesc::operator==(const StencilStateDesc& other) const {
  return stencilCompareFunction == other.stencilCompareFunction && writeMask == other.writeMask &&
         readMask == other.readMask &&
         depthStencilPassOperation == other.depthStencilPassOperation &&
         depthFailureOperation == other.depthFailureOperation &&
         stencilFailureOperation == other.stencilFailureOperation;
}

size_t std::hash<igl::StencilStateDesc>::operator()(const igl::StencilStateDesc& key) const {
  size_t hash = std::hash<int>()(static_cast<int>(EnumToValue(key.stencilCompareFunction)));
  hash ^= std::hash<int>()(static_cast<int>(key.writeMask));
  hash ^= std::hash<int>()(static_cast<int>(key.readMask));
  hash ^= std::hash<int>()(static_cast<int>(EnumToValue(key.depthStencilPassOperation)));
  hash ^= std::hash<int>()(static_cast<int>(EnumToValue(key.depthFailureOperation)));
  hash ^= std::hash<int>()(static_cast<int>(EnumToValue(key.stencilFailureOperation)));
  return hash;
}

bool DepthStencilStateDesc::operator!=(const DepthStencilStateDesc& other) const {
  return !(*this == other);
}

bool DepthStencilStateDesc::operator==(const DepthStencilStateDesc& other) const {
  return frontFaceStencil == other.frontFaceStencil && compareFunction == other.compareFunction &&
         backFaceStencil == other.backFaceStencil &&
         isDepthWriteEnabled == other.isDepthWriteEnabled;
}

size_t std::hash<igl::DepthStencilStateDesc>::operator()(
    const igl::DepthStencilStateDesc& key) const {
  size_t hash = std::hash<igl::StencilStateDesc>()(key.frontFaceStencil);
  hash ^= std::hash<igl::StencilStateDesc>()(key.backFaceStencil);
  hash ^= std::hash<int>()(static_cast<int>(EnumToValue(key.compareFunction)));
  hash ^= std::hash<bool>()(static_cast<bool>(key.isDepthWriteEnabled));
  return hash;
}
