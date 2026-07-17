/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <igl/opengl/webgl/PlatformDevice.h>

#include <igl/opengl/TextureBuffer.h>
#include <igl/opengl/ViewTextureTarget.h>
#include <igl/opengl/webgl/Context.h>
#include <igl/opengl/webgl/Device.h>

namespace igl::opengl::webgl {

PlatformDevice::PlatformDevice(Device& owner) : opengl::PlatformDevice(owner) {}

std::shared_ptr<ITexture> PlatformDevice::createTextureFromNativeDrawable(int width,
                                                                          int height,
                                                                          Result* outResult) {
  if (drawableTexture_ && drawableTexture_->getWidth() == width &&
      drawableTexture_->getHeight() == height) {
    return drawableTexture_;
  }

  auto context = static_cast<igl::opengl::webgl::Context*>(&getContext());
  context->setCanvasBufferSize(width, height);

  const TextureDesc desc = {.width = static_cast<uint32_t>(width),
                            .height = static_cast<uint32_t>(height),
                            .depth = 1,
                            .numLayers = 1,
                            .numSamples = 1,
                            .usage = TextureDesc::TextureUsageBits::Attachment,
                            .numMipLevels = 1,
                            .type = TextureType::TwoD,
                            .format = TextureFormat::RGBA_UNorm8};
  drawableTexture_ = std::make_shared<ViewTextureTarget>(getContext(), desc.format);

  Result result = drawableTexture_->create(desc, true);
  if (outResult) {
    *outResult = result;
  }
  if (auto resourceTracker = owner_.getResourceTracker()) {
    drawableTexture_->initResourceTracker(resourceTracker);
  }
  return drawableTexture_;
}

bool PlatformDevice::isType(PlatformDeviceType t) const noexcept {
  return t == kType || opengl::PlatformDevice::isType(t);
}

} // namespace igl::opengl::webgl
