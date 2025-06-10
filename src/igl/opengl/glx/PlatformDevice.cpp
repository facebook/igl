/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only
// @fb-only

#include <igl/opengl/ViewTextureTarget.h>
#include <igl/opengl/glx/Context.h>
#include <igl/opengl/glx/Device.h>
#include <igl/opengl/glx/PlatformDevice.h>

namespace igl::opengl::glx {

PlatformDevice::PlatformDevice(Device& owner) : opengl::PlatformDevice(owner) {}

std::shared_ptr<ITexture> PlatformDevice::createTextureFromNativeDrawable(uint32_t width,
                                                                          uint32_t height,
                                                                          Result* outResult) {
  auto* context = static_cast<Context*>(getSharedContext().get());
  if (context == nullptr) {
    Result::setResult(outResult, Result::Code::InvalidOperation, "No GLX context found!");
    return nullptr;
  }

  if (drawableTexture_ && width_ == width && height_ == height) {
    Result::setResult(outResult, Result::Code::Ok);
    return drawableTexture_;
  }

  const auto desc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                       static_cast<size_t>(width),
                                       static_cast<size_t>(height),
                                       TextureDesc::TextureUsageBits::Attachment,
                                       "NativeDrawable");
  auto texture = std::make_shared<ViewTextureTarget>(getContext(), desc.format);

  Result subResult = texture->create(desc, true);
  Result::setResult(outResult, subResult.code, subResult.message);
  if (!subResult.isOk()) {
    return nullptr;
  }

  drawableTexture_ = std::move(texture);
  width_ = width;
  height_ = height;

  if (auto resourceTracker = owner_.getResourceTracker()) {
    drawableTexture_->initResourceTracker(resourceTracker);
  }

  return drawableTexture_;
}

std::shared_ptr<ITexture> PlatformDevice::createTextureFromNativeDepth(uint32_t width,
                                                                       uint32_t height,
                                                                       Result* outResult) {
  // generate depth with new width and height
  auto* context = static_cast<Context*>(getSharedContext().get());
  if (!context) {
    Result::setResult(outResult, Result::Code::InvalidOperation, "No GLXs context found!");
    return nullptr;
  }

  if (depthTexture_ && width_ == width && height_ == height) {
    Result::setResult(outResult, Result::Code::Ok);
    return depthTexture_;
  }

  const auto desc = TextureDesc::new2D(TextureFormat::S8_UInt_Z24_UNorm,
                                       static_cast<size_t>(width),
                                       static_cast<size_t>(height),
                                       TextureDesc::TextureUsageBits::Attachment,
                                       "NativeDepth");

  auto texture = std::make_shared<ViewTextureTarget>(getContext(), desc.format);
  IGL_DEBUG_ASSERT(texture);
  const Result subResult = texture->create(desc, true);
  Result::setResult(outResult, subResult.code, subResult.message);
  if (!subResult.isOk()) {
    return nullptr;
  }

  depthTexture_ = std::move(texture);
  width_ = width;
  height_ = height;

  if (auto resourceTracker = owner_.getResourceTracker()) {
    depthTexture_->initResourceTracker(resourceTracker);
  }

  return depthTexture_;
}

bool PlatformDevice::isType(PlatformDeviceType t) const noexcept {
  return t == Type || opengl::PlatformDevice::isType(t);
}

} // namespace igl::opengl::glx
