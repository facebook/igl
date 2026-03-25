/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#import <Metal/Metal.h>
#include <igl/metal/Device.h>

namespace igl::metal::ios {

// @fb-only
class Device final : public metal::Device {
 public:
  explicit Device(id<MTLDevice> device);
  ~Device() override = default;

  std::shared_ptr<IFramebuffer> createFramebuffer(const FramebufferDesc& desc,
                                                  Result* outResult) override;
};

} // namespace igl::metal::ios
