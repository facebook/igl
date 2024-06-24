/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "ITextureAccessor.h"
#include "igl/Framebuffer.h"
#include <igl/CommandQueue.h>
#include <igl/IGL.h>
#include <igl/Texture.h>

namespace iglu::textureaccessor {

class MetalTextureAccessor : public ITextureAccessor {
 public:
  MetalTextureAccessor(std::shared_ptr<igl::ITexture> texture, igl::IDevice& device);

  void requestBytes(igl::ICommandQueue& commandQueue,
                    std::shared_ptr<igl::ITexture> texture = nullptr) override;
  RequestStatus getRequestStatus() override;
  std::vector<unsigned char>& getBytes() override;
  size_t copyBytes(unsigned char* ptr, size_t length) override;

 private:
  std::vector<unsigned char> latestBytesRead_;
  RequestStatus status_ = RequestStatus::NotInitialized;
  size_t textureWidth_ = 0;
  size_t textureHeight_ = 0;
  size_t textureBytesPerRow_ = 0;
  size_t textureBytesPerImage_ = 0;
  std::shared_ptr<igl::IBuffer> readBuffer_ = nullptr;
  std::shared_ptr<igl::ICommandBuffer> lastRequestCommandBuffer = nullptr;
};

} // namespace iglu::textureaccessor
