/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/TextureBuffer.h>
#include <igl/opengl/ViewTextureTarget.h>
#include <igl/opengl/wgl/Context.h>
#include <igl/opengl/wgl/Device.h>
#include <igl/opengl/wgl/PlatformDevice.h>

namespace igl {
namespace opengl {
namespace wgl {

PlatformDevice::PlatformDevice(Device& owner) : opengl::PlatformDevice(owner) {}

std::shared_ptr<ITexture> PlatformDevice::createTextureFromNativeDrawable(Result* outResult) {
  RECT curDimension;

  auto context = static_cast<Context*>(getSharedContext().get());
  if (context == nullptr) {
    Result::setResult(outResult, Result::Code::InvalidOperation, "No WGL context found!");
    return nullptr;
  }

  GetClientRect(WindowFromDC(context->getDeviceContext()), &curDimension);

  if (drawableTexture_ && EqualRect(&dimension_, &curDimension)) {
    return drawableTexture_;
  }

  CopyRect(&dimension_, &curDimension);

  const auto desc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                       (size_t)(dimension_.right - dimension_.left),
                                       (size_t)(dimension_.bottom - dimension_.top),
                                       TextureDesc::TextureUsageBits::Attachment,
                                       "NativeDrawable");
  auto texture = std::make_shared<ViewTextureTarget>(getContext(), desc.format);

  Result subResult = texture->create(desc, true);
  Result::setResult(outResult, subResult.code, subResult.message);
  if (!subResult.isOk()) {
    return nullptr;
  }
  drawableTexture_ = std::move(texture);
  if (auto resourceTracker = owner_.getResourceTracker()) {
    drawableTexture_->initResourceTracker(resourceTracker);
  }

  return drawableTexture_;
}

std::shared_ptr<ITexture> PlatformDevice::createTextureFromNativeDepth(int width,
                                                                       int height,
                                                                       Result* outResult) {
  if (drawableTexture_ && drawableTexture_->getWidth() == width &&
      drawableTexture_->getHeight() == height) {
    Result::setResult(outResult, Result::Code::Ok);
    return drawableTexture_;
  }

  // generate depth with new width and height
  auto context = static_cast<Context*>(getSharedContext().get());
  if (!context) {
    Result::setResult(outResult, Result::Code::InvalidOperation, "No EGL context found!");
    return nullptr;
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

  if (auto resourceTracker = owner_.getResourceTracker()) {
    texture->initResourceTracker(resourceTracker);
  }

  return texture;
}

bool PlatformDevice::isType(PlatformDeviceType t) const noexcept {
  return t == Type || opengl::PlatformDevice::isType(t);
}

} // namespace wgl
} // namespace opengl
} // namespace igl
