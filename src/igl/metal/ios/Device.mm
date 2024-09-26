/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/ios/Device.h>

#include <igl/metal/ios/Framebuffer.h>

namespace igl::metal::ios {

Device::Device(id<MTLDevice> device) : metal::Device(device) {}

std::shared_ptr<IFramebuffer> Device::createFramebuffer(const FramebufferDesc& desc,
                                                        Result* outResult) {
  auto resource = std::make_shared<Framebuffer>(desc);
  if (getResourceTracker()) {
    resource->initResourceTracker(getResourceTracker(), desc.debugName);
  }
  Result::setOk(outResult);
  return resource;
}

} // namespace igl::metal::ios
