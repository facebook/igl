/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#define IGL_USE_CUSTOM_HALF 1

#if IGL_USE_CUSTOM_HALF
#include <glm/gtc/packing.hpp>

namespace igl::tests::util {
struct Half {
 public:
  constexpr Half() noexcept = default;

  explicit Half(float f) {
    data_ = glm::packHalf1x16(f);
  }
  Half& operator=(float f) {
    data_ = glm::packHalf1x16(f);
    return *this;
  }

  operator float() const {
    return glm::unpackHalf1x16(data_);
  }

 private:
  uint16_t data_ = 0;

  template<typename charT, typename traits>
  friend std::basic_ostream<charT, traits>& operator<<(std::basic_ostream<charT, traits>& /*out*/,
                                                       Half /*arg*/);
};

template<typename charT, typename traits>
std::basic_ostream<charT, traits>& operator<<(std::basic_ostream<charT, traits>& out, Half arg) {
  return out << glm::unpackHalf1x16(arg.data_);
}

using TestHalf = Half;
} // namespace igl::tests::util
#else
#include <half/half.hpp>

namespace igl::tests::util {
using TestHalf = half_float::half;
} // namespace igl::tests::util
#endif
