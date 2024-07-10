/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/SamplerState.h>

using namespace igl;

namespace igl::metal {

SamplerState::SamplerState(id<MTLSamplerState> value) : value_(value) {}

MTLSamplerMinMagFilter SamplerState::convertMinMagFilter(SamplerMinMagFilter value) {
  switch (value) {
  case SamplerMinMagFilter::Nearest:
    return MTLSamplerMinMagFilterNearest;
  case SamplerMinMagFilter::Linear:
    return MTLSamplerMinMagFilterLinear;
  }
}

MTLSamplerMipFilter SamplerState::convertMipFilter(SamplerMipFilter value) {
  switch (value) {
  case SamplerMipFilter::Disabled:
    return MTLSamplerMipFilterNotMipmapped;
  case SamplerMipFilter::Nearest:
    return MTLSamplerMipFilterNearest;
  case SamplerMipFilter::Linear:
    return MTLSamplerMipFilterLinear;
  }
}

MTLSamplerAddressMode SamplerState::convertAddressMode(SamplerAddressMode value) {
  switch (value) {
  case SamplerAddressMode::Repeat:
    return MTLSamplerAddressModeRepeat;
  case SamplerAddressMode::Clamp:
    return MTLSamplerAddressModeClampToEdge;
  case SamplerAddressMode::MirrorRepeat:
    return MTLSamplerAddressModeMirrorRepeat;
  }
}

bool SamplerState::isYUV() const noexcept {
  // Not supported in this API.
  return false;
}

} // namespace igl::metal
