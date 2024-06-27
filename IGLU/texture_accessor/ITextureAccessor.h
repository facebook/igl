/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/CommandQueue.h>
#include <igl/IGL.h>
#include <igl/Texture.h>

namespace iglu::textureaccessor {

enum class RequestStatus : uint8_t {
  Ready = 0,
  NotInitialized,
  InProgress,
};

/// Interface for getting CPU access to GPU texture data
class ITextureAccessor {
 public:
  ITextureAccessor(std::shared_ptr<igl::ITexture> texture) : texture_(std::move(texture)) {}

  virtual ~ITextureAccessor() = default;

  // Start reading data from the ITexture GPU resource, to be accessed later
  // Receive an optional texture an input. It MUST be the same size as previous texture
  virtual void requestBytes(igl::ICommandQueue& commandQueue,
                            std::shared_ptr<igl::ITexture> texture = nullptr) = 0;

  // Get the status of the request. Returns RequestStatus::Ready if requestBytes() has finished
  // reading texture data.
  virtual RequestStatus getRequestStatus() = 0;

  /**
    Get the texture bytes read by requestBytes(). If there is an in-progress read, we will
    synchronously wait for it to complete and then return the data.
  */
  virtual std::vector<unsigned char>& getBytes() = 0;

  // copy data into preallocated buffer, returns copied data in bytes
  virtual size_t copyBytes(unsigned char* ptr, size_t length) = 0;

  // Synchronously read the bytes of the ITexture. This is not recommended; using requestBytes() and
  // getBytes() is more performant when getBytes() is called later.
  // Receive an optional texture an input. It MUST be the same size as previous texture
  std::vector<unsigned char>& requestAndGetBytesSync(
      igl::ICommandQueue& commandQueue,
      std::shared_ptr<igl::ITexture> texture = nullptr) {
    requestBytes(commandQueue, std::move(texture));
    return getBytes();
  }

  // Synchronously read the bytes of the ITexture. This is not recommended; using requestBytes() and
  // copyBytes() is more performant when copyBytes() is called later.
  // Receive an optional texture an input. It MUST be the same size as previous texture

  size_t requestAndCopyBytesSync(igl::ICommandQueue& commandQueue,
                                 unsigned char* ptr,
                                 size_t length,
                                 std::shared_ptr<igl::ITexture> texture = nullptr) {
    requestBytes(commandQueue, std::move(texture));
    return copyBytes(ptr, length);
  }

  [[nodiscard]] std::shared_ptr<igl::ITexture> getTexture() const {
    return texture_;
  }

 protected:
  std::shared_ptr<igl::ITexture> texture_;
};

} // namespace iglu::textureaccessor
