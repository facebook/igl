/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "MetalTextureAccessor.h"
#include "ITextureAccessor.h"
#include "igl/Buffer.h"
#include "igl/Texture.h"
#include "igl/metal/Buffer.h"
#include "igl/metal/CommandBuffer.h"
#include "igl/metal/Texture.h"
#include <igl/metal/Buffer.h>

#if defined(IGL_CMAKE_BUILD)
#include <igl/IGLSafeC.h>
#else
#include <secure_lib/secure_string.h>
#endif

namespace iglu {
namespace textureaccessor {

MetalTextureAccessor::MetalTextureAccessor(std::shared_ptr<igl::ITexture> texture,
                                           igl::IDevice& device) :
  ITextureAccessor(std::move(texture)) {
  auto& iglMetalTexture = static_cast<igl::metal::Texture&>(*texture_);
  IGL_ASSERT(iglMetalTexture.get() != nullptr);

  const auto dimensions = iglMetalTexture.getDimensions();
  textureWidth_ = dimensions.width;
  textureHeight_ = dimensions.height;

  const auto& properties = iglMetalTexture.getProperties();
  textureBytesPerRow_ = properties.getBytesPerRow(textureWidth_);
  textureBytesPerImage_ = properties.getBytesPerRange(iglMetalTexture.getFullRange());

  latestBytesRead_.resize(textureBytesPerImage_);

  igl::BufferDesc readBufferDesc;
  readBufferDesc.storage = igl::ResourceStorage::Shared;
  readBufferDesc.length = textureBytesPerImage_;
  igl::Result res;
  readBuffer_ = device.createBuffer(readBufferDesc, &res);
  IGL_ASSERT(res.isOk());
  IGL_ASSERT(static_cast<igl::metal::Buffer&>(*readBuffer_).get() != nullptr);
};

void MetalTextureAccessor::requestBytes(igl::ICommandQueue& commandQueue,
                                        std::shared_ptr<igl::ITexture> texture) {
  if (texture) {
    IGL_ASSERT(textureWidth_ == texture->getDimensions().width &&
               textureHeight_ == texture->getDimensions().height);
    texture_ = std::move(texture);
  }

  auto metalTexture = static_cast<igl::metal::Texture&>(*texture_).get();
  IGL_ASSERT(metalTexture != nullptr);
  auto metalReadBuffer = static_cast<igl::metal::Buffer&>(*readBuffer_).get();

  igl::Result res;
  igl::CommandBufferDesc desc;
  auto iglMtlCommandBuffer = commandQueue.createCommandBuffer(desc, &res);
  IGL_ASSERT(res.isOk());
  auto metalCmdBuffer = static_cast<igl::metal::CommandBuffer&>(*iglMtlCommandBuffer).get();

  id<MTLBlitCommandEncoder> blitEncoder = [metalCmdBuffer blitCommandEncoder];

  [blitEncoder copyFromTexture:metalTexture
                   sourceSlice:0
                   sourceLevel:0
                  sourceOrigin:MTLOriginMake(0, 0, 0)
                    sourceSize:MTLSizeMake(textureWidth_, textureHeight_, 1)
                      toBuffer:metalReadBuffer
             destinationOffset:0
        destinationBytesPerRow:textureBytesPerRow_
      destinationBytesPerImage:textureBytesPerImage_];

  [blitEncoder endEncoding];

  lastRequestCommandBuffer = iglMtlCommandBuffer;
  status_ = RequestStatus::InProgress;

  [metalCmdBuffer addCompletedHandler:^(id<MTLCommandBuffer> _cb) {
    checked_memcpy_robust(latestBytesRead_.data(),
                          latestBytesRead_.size(),
                          metalReadBuffer.contents,
                          metalReadBuffer.length,
                          textureBytesPerImage_);
    status_ = RequestStatus::Ready;
  }];
  [metalCmdBuffer commit];
}

RequestStatus MetalTextureAccessor::getRequestStatus() {
  return status_;
};

std::vector<unsigned char>& MetalTextureAccessor::getBytes() {
  if (status_ == RequestStatus::InProgress) {
    lastRequestCommandBuffer->waitUntilCompleted();
  }
  return latestBytesRead_;
}

} // namespace textureaccessor
} // namespace iglu
