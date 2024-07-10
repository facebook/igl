/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <Metal/Metal.h>
#include <igl/SamplerState.h>

namespace igl::metal {

class SamplerState final : public ISamplerState {
 public:
  explicit SamplerState(id<MTLSamplerState> value);
  IGL_INLINE id<MTLSamplerState> get() const {
    return value_;
  }

  static MTLSamplerMinMagFilter convertMinMagFilter(SamplerMinMagFilter value);
  static MTLSamplerMipFilter convertMipFilter(SamplerMipFilter value);
  static MTLSamplerAddressMode convertAddressMode(SamplerAddressMode value);

  /**
   * @brief Returns true if this sampler is a YUV sampler.
   */
  [[nodiscard]] bool isYUV() const noexcept override;

 private:
  id<MTLSamplerState> value_;
};

} // namespace igl::metal
