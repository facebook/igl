/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#ifndef IGL_NULLABLE
#define IGL_NULLABLE
#endif

namespace igl {

// Non-ref counted handles; based on:
// https://enginearchitecture.realtimerendering.com/downloads/reac2023_modern_mobile_rendering_at_hypehype.pdf
// https://github.com/corporateshark/lightweightvk/blob/main/lvk/LVK.h
template<typename ObjectType>
class Handle final {
 public:
  Handle() noexcept = default;

  [[nodiscard]] bool empty() const noexcept {
    return gen_ == 0;
  }
  [[nodiscard]] bool valid() const noexcept {
    return gen_ != 0;
  }
  [[nodiscard]] uint32_t index() const noexcept {
    return index_;
  }
  [[nodiscard]] uint32_t gen() const noexcept {
    return gen_;
  }
  [[nodiscard]] void* IGL_NULLABLE indexAsVoid() const noexcept {
    return reinterpret_cast<void*>( // NOLINT(performance-no-int-to-ptr)
        static_cast<ptrdiff_t>(index_)); // NOLINT(performance-no-int-to-ptr)
  }
  bool operator==(const Handle<ObjectType>& other) const noexcept {
    return index_ == other.index_ && gen_ == other.gen_;
  }
  bool operator!=(const Handle<ObjectType>& other) const noexcept {
    return index_ != other.index_ || gen_ != other.gen_;
  }
  // allow conditions 'if (handle)'
  explicit operator bool() const noexcept {
    return gen_ != 0;
  }

 private:
  Handle(uint32_t index, uint32_t gen) noexcept : index_(index), gen_(gen) {}

  template<typename ObjectType_, typename ImplObjectType>
  friend class Pool;

  uint32_t index_ = 0; // the index of this handle within a Pool
  uint32_t gen_ = 0; // the generation of this handle to prevent the ABA Problem
};

static_assert(sizeof(Handle<class Foo>) == sizeof(uint64_t));

} // namespace igl
