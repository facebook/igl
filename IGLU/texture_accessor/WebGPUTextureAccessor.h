/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "ITextureAccessor.h"

#include <igl/CommandQueue.h>
#include <igl/IGL.h>
#include <igl/Texture.h>
#if IGL_BACKEND_WEBGPU
#include <igl/webgpu/Readback.h>
#endif

namespace iglu::textureaccessor {

/// Reads mip 0 of the texture top row first. requestBytes() only submits the copy, so
/// getRequestStatus() can be polled where waiting is not possible (browser builds without JSPI).
class WebGPUTextureAccessor : public ITextureAccessor {
 public:
  WebGPUTextureAccessor(std::shared_ptr<igl::ITexture> texture, igl::IDevice& device);

  void requestBytes(igl::ICommandQueue& commandQueue,
                    std::shared_ptr<igl::ITexture> texture = nullptr) override;
  RequestStatus getRequestStatus() override;
  std::vector<unsigned char>& getBytes() override;
  size_t copyBytes(unsigned char* ptr, size_t length) override;

 private:
  void finishRequest(bool wait);

  std::vector<unsigned char> latestBytesRead_;
  RequestStatus status_ = RequestStatus::NotInitialized;
#if IGL_BACKEND_WEBGPU
  const igl::webgpu::WebGPUContext* ctx_ = nullptr;
  igl::webgpu::AsyncTextureReadback readback_;
#endif
};

} // namespace iglu::textureaccessor
