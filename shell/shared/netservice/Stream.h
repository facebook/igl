/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <functional>

namespace igl::shell::netservice {

// ----------------------------------------------------------------------------

struct Stream {
  enum class Event : uint8_t {
    None = 0,
    OpenCompleted,
    HasBytesAvailable,
    HasSpaceAvailable,
    ErrorOccurred,
    EndEncountered,
  };

  enum class Status : uint8_t {
    AtEnd,
    Closed,
    Error,
    NotOpen,
    Open,
    Opening,
    Reading,
    Writing,
  };

  using Observer = std::function<void(Stream& sender, Event event)>;

  virtual ~Stream() = default;

  virtual void open() noexcept = 0;
  [[nodiscard]] virtual Status status() const noexcept = 0;
  virtual void close() noexcept = 0;

  [[nodiscard]] const Observer& observer() const noexcept {
    return observer_;
  }

  void setObserver(Observer observer) noexcept {
    observer_ = std::move(observer);
  }

 private:
  Observer observer_;
};

// ----------------------------------------------------------------------------

struct InputStream : Stream {
  virtual int read(uint8_t* outBuffer, size_t maxLength) const noexcept = 0;
  virtual bool getBuffer(uint8_t*& outBuffer, size_t& outLength) const noexcept = 0;
  virtual bool hasBytesAvailable() const noexcept = 0;
};

// ----------------------------------------------------------------------------

struct OutputStream : Stream {
  virtual int write(const uint8_t* buffer, size_t maxLength) noexcept = 0;
  [[nodiscard]] virtual bool hasSpaceAvailable() const noexcept = 0;
};

// ----------------------------------------------------------------------------

} // namespace igl::shell::netservice
