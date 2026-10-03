/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "WebGPUTextureAccessor.h"

#include <algorithm>
#include <igl/Macros.h>
#if IGL_BACKEND_WEBGPU
#include <igl/webgpu/Device.h>
#include <igl/webgpu/PlatformDevice.h>
#include <igl/webgpu/Texture.h>
#endif

namespace iglu::textureaccessor {

WebGPUTextureAccessor::WebGPUTextureAccessor(std::shared_ptr<igl::ITexture> texture,
                                             igl::IDevice& device) :
  ITextureAccessor(std::move(texture)) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  latestBytesRead_.resize(texture_->getProperties().getBytesPerRange(texture_->getFullRange()));
#if IGL_BACKEND_WEBGPU
  const auto* platformDevice = device.getPlatformDevice<igl::webgpu::PlatformDevice>();
  IGL_DEBUG_ASSERT(platformDevice != nullptr);
  if (platformDevice != nullptr) {
    ctx_ = &platformDevice->getContext();
  }
#else
  (void)device;
#endif
}

void WebGPUTextureAccessor::requestBytes(igl::ICommandQueue& /*commandQueue*/,
                                         std::shared_ptr<igl::ITexture> texture) {
  IGL_PROFILER_FUNCTION();
  if (texture) {
    IGL_DEBUG_ASSERT(texture_->getDimensions().width == texture->getDimensions().width &&
                     texture_->getDimensions().height == texture->getDimensions().height);
    texture_ = std::move(texture);
    // The new texture may have a different format.
    latestBytesRead_.resize(texture_->getProperties().getBytesPerRange(texture_->getFullRange()));
  }
#if IGL_BACKEND_WEBGPU
  if (ctx_ == nullptr) {
    return;
  }
  // Submitted to the device's only queue, so the copy runs after all work submitted before.
  const auto& webgpuTexture = static_cast<const igl::webgpu::Texture&>(*texture_);
  const auto dimensions = texture_->getDimensions();
  const igl::Result result =
      readback_.begin(*ctx_,
                      {
                          .texture = webgpuTexture.getWGPUTexture(),
                          .mipLevel = webgpuTexture.getBaseMipLevel(),
                          .layer = webgpuTexture.getBaseLayer(),
                          .width = dimensions.width,
                          .height = dimensions.height,
                          // 0 for compressed formats, which then fail.
                          .bytesPerTexel = igl::webgpu::getCopyBytesPerTexel(
                              webgpuTexture.getWGPUFormat(), WGPUTextureAspect_All),
                          .flipVertically = false,
                      });
  if (!result.isOk()) {
    IGL_LOG_ERROR("WebGPUTextureAccessor: %s\n", result.message.c_str());
    status_ = RequestStatus::NotInitialized;
    return;
  }
  status_ = RequestStatus::InProgress;
#endif
}

void WebGPUTextureAccessor::finishRequest(bool wait) {
#if IGL_BACKEND_WEBGPU
  if (status_ != RequestStatus::InProgress) {
    return;
  }
  if (wait) {
    const igl::Result result = readback_.wait();
    if (!result.isOk()) {
      IGL_LOG_ERROR("WebGPUTextureAccessor: %s\n", result.message.c_str());
    }
  } else if (!readback_.poll()) {
    return;
  }
  const igl::Result result = readback_.copyTo(latestBytesRead_.data(), latestBytesRead_.size());
  if (!result.isOk()) {
    // Same status as a request that could not start: the bytes are not from this request.
    IGL_LOG_ERROR("WebGPUTextureAccessor: %s\n", result.message.c_str());
    status_ = RequestStatus::NotInitialized;
    return;
  }
  status_ = RequestStatus::Ready;
#else
  (void)wait;
#endif
}

RequestStatus WebGPUTextureAccessor::getRequestStatus() {
  finishRequest(/*wait=*/false);
  return status_;
}

std::vector<unsigned char>& WebGPUTextureAccessor::getBytes() {
  IGL_PROFILER_FUNCTION();
  finishRequest(/*wait=*/true);
  return latestBytesRead_;
}

size_t WebGPUTextureAccessor::copyBytes(unsigned char* ptr, size_t length) {
  IGL_PROFILER_FUNCTION();
  if (length < latestBytesRead_.size()) {
    return 0;
  }
  const bool requested = status_ == RequestStatus::InProgress;
  finishRequest(/*wait=*/true);
  if (requested && status_ != RequestStatus::Ready) {
    // The readback failed; the bytes are not from this request.
    return 0;
  }
  std::copy(latestBytesRead_.begin(), latestBytesRead_.end(), ptr);
  return latestBytesRead_.size();
}

} // namespace iglu::textureaccessor
