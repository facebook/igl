/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/opengl/Device.h>
#include <igl/opengl/PlatformDevice.h>

namespace igl {
namespace opengl {
namespace empty {

class Device final : public opengl::Device {
 public:
  explicit Device(std::unique_ptr<IContext> context);

  const PlatformDevice& getPlatformDevice() const noexcept override;

 private:
  PlatformDevice platformDevice_;
};

} // namespace empty
} // namespace opengl
} // namespace igl
