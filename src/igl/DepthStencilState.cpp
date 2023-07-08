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
  return stencilCompareOp == other.stencilCompareOp && writeMask == other.writeMask &&
         readMask == other.readMask && depthStencilPassOp == other.depthStencilPassOp &&
         depthFailureOp == other.depthFailureOp && stencilFailureOp == other.stencilFailureOp;
}

bool DepthStencilStateDesc::operator!=(const DepthStencilStateDesc& other) const {
  return !(*this == other);
}

bool DepthStencilStateDesc::operator==(const DepthStencilStateDesc& other) const {
  return frontFaceStencil == other.frontFaceStencil && compareOp == other.compareOp &&
         backFaceStencil == other.backFaceStencil &&
         isDepthWriteEnabled == other.isDepthWriteEnabled;
}
