/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/netservice/Stream.h>

#import <Foundation/Foundation.h>

namespace igl::shell::netservice {

// ----------------------------------------------------------------------------

struct StreamAdapterApple final {
  ~StreamAdapterApple();

  bool initialize(Stream* owner, NSStream* stream) noexcept;

  void open() noexcept;
  [[nodiscard]] Stream::Status status() const noexcept;
  void close() noexcept;

  Stream* stream() const noexcept {
    return owner_;
  }

  NSStream* nsStream() const noexcept {
    return stream_;
  }

 private:
  Stream* owner_ = nullptr; // weak reference
  NSStream* stream_ = nil;
  id<NSStreamDelegate> delegate_ = nil;
};

// ----------------------------------------------------------------------------

struct InputStreamApple final : InputStream {
  void open() noexcept final {
    streamAdapter_.open();
  }

  [[nodiscard]] Status status() const noexcept final {
    return streamAdapter_.status();
  }

  void close() noexcept final {
    streamAdapter_.close();
  }

  int read(uint8_t* outBuffer, size_t maxLength) const noexcept final;
  bool getBuffer(uint8_t*& outBuffer, size_t& outLength) const noexcept final;
  [[nodiscard]] bool hasBytesAvailable() const noexcept final;

  bool initialize(NSInputStream* stream) noexcept {
    return streamAdapter_.initialize(this, stream);
  }

 private:
  NSInputStream* inputStream() const noexcept {
    return (NSInputStream*)streamAdapter_.nsStream();
  }

  StreamAdapterApple streamAdapter_;
};

// ----------------------------------------------------------------------------

struct OutputStreamApple final : OutputStream {
  void open() noexcept final {
    streamAdapter_.open();
  }

  [[nodiscard]] Status status() const noexcept final {
    return streamAdapter_.status();
  }

  void close() noexcept final {
    streamAdapter_.close();
  }

  int write(const uint8_t* buffer, size_t maxLength) noexcept final;
  [[nodiscard]] bool hasSpaceAvailable() const noexcept final;

  bool initialize(NSOutputStream* stream) noexcept {
    return streamAdapter_.initialize(this, stream);
  }

 private:
  NSOutputStream* outputStream() const noexcept {
    return (NSOutputStream*)streamAdapter_.nsStream();
  }

  StreamAdapterApple streamAdapter_;
};

// ----------------------------------------------------------------------------

} // namespace igl::shell::netservice
