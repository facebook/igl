/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <glm/gtc/packing.hpp>

namespace igl::tests::util {

struct Half {
 public:
  Half() {}
  explicit Half(float f) {
    data_ = glm::packHalf1x16(f);
  }
  Half operator=(float f) {
    data_ = glm::packHalf1x16(f);
    return *this;
  }
  operator float() const {
    return glm::unpackHalf1x16(data_);
  }

 private:
  uint16_t data_ = 0;
};

} // namespace igl::tests::util
