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
#include <igl/opengl/GLIncludes.h>

namespace iglu::textureaccessor {

class OpenGLTextureAccessor : public ITextureAccessor {
 public:
  OpenGLTextureAccessor(std::shared_ptr<igl::ITexture> texture, igl::IDevice& device);

  void requestBytes(igl::ICommandQueue& commandQueue,
                    std::shared_ptr<igl::ITexture> texture = nullptr) override;
  RequestStatus getRequestStatus() override;
  std::vector<unsigned char>& getBytes() override;
  size_t copyBytes(unsigned char* ptr, size_t length) override;

 private:
  std::vector<unsigned char> latestBytesRead_;
  RequestStatus status_ = RequestStatus::NotInitialized;
  std::shared_ptr<igl::IFramebuffer> frameBuffer_;
  size_t textureWidth_ = 0;
  size_t textureHeight_ = 0;
  size_t textureBytesPerImage_ = 0;

  GLuint pboId_ = 0;
  GLsync sync_ = nullptr;
  bool dataCopied_ = false;
  bool asyncReadbackSupported_ = false;
  bool textureAttached_ = false;
};

} // namespace iglu::textureaccessor
