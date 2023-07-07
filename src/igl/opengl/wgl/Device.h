/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/opengl/Device.h>
#include <igl/opengl/wgl/PlatformDevice.h>

namespace igl {
namespace opengl {
namespace wgl {

class Device final : public igl::opengl::Device {
 public:
  explicit Device(std::unique_ptr<IContext> context);
  virtual ~Device() override;

  const PlatformDevice& getPlatformDevice() const noexcept override;

 protected:
  PlatformDevice platformDevice_;
};

} // namespace wgl
} // namespace opengl
} // namespace igl
