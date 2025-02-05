/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <shell/shared/netservice/Stream.h>

//@class NSInputStream;
//@class NSOutputStream;
//@class NSNetService;
//@class NSStream;
//@protocol NSNetServiceDelegate;
//@protocol NSStreamDelegate;

namespace igl::shell::netservice {

// ----------------------------------------------------------------------------

struct StreamAdapterZeroMQ final {
  ~StreamAdapterZeroMQ();

  bool initialize(Stream* owner, NSStream* stream) noexcept;

  void open() noexcept;
  void close() noexcept;

  Stream* stream() const noexcept {
    return owner_;
  }

  NSStream* nsStream() const noexcept {
    return stream_;
  }

 private:
  Stream* owner_ = nullptr; // weak ref
  // NSStream* stream_ = nil;
  // id<NSStreamDelegate> delegate_ = nil;
};

// ----------------------------------------------------------------------------

struct InputStreamZeroMQ final : InputStream {
  void open() noexcept override {
    streamAdapter_.open();
  }
  void close() noexcept override {
    streamAdapter_.close();
  }
  int read(uint8_t* outBuffer, size_t maxLength) const noexcept override;
  bool getBuffer(uint8_t*& outBuffer, size_t& outLength) const noexcept override;
  bool hasBytesAvailable() const noexcept override;

  bool initialize(NSInputStream* stream) noexcept {
    return streamAdapter_.initialize(this, stream);
  }

 private:
  StreamAdapterZeroMQ streamAdapter_;
};

// ----------------------------------------------------------------------------

struct OutputStreamZeroMQ final : OutputStream {
  void open() noexcept override {
    streamAdapter_.open();
  }
  void close() noexcept override {
    streamAdapter_.close();
  }
  int write(const uint8_t* buffer, size_t maxLength) noexcept override;
  bool hasSpaceAvailable() const noexcept override;

  bool initialize(NSOutputStream* stream) noexcept {
    return streamAdapter_.initialize(this, stream);
  }

 private:
  StreamAdapterZeroMQ streamAdapter_;
};

// ----------------------------------------------------------------------------

} // namespace igl::shell::netservice
