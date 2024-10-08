/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <igl/opengl/TextureBuffer.h>
#include <igl/opengl/ViewTextureTarget.h>
#include <igl/opengl/webgl/Context.h>
#include <igl/opengl/webgl/Device.h>
#include <igl/opengl/webgl/PlatformDevice.h>

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

  TextureDesc desc = {static_cast<size_t>(width),
                      static_cast<size_t>(height),
                      1, // depth
                      1, // numLayers
                      1, // numSamples
                      TextureDesc::TextureUsageBits::Attachment,
                      1, // numMipLevels
                      TextureType::TwoD,
                      TextureFormat::RGBA_UNorm8};
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
  return t == Type || opengl::PlatformDevice::isType(t);
}

} // namespace igl::opengl::webgl
