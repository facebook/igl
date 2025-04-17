/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <vector>
#include <igl/Common.h>
#include <igl/NameHandle.h>

namespace igl {
class IRenderCommandEncoder;
} // namespace igl

namespace iglu::uniform {

struct Collection;

// CollectionEncoder
//
// Submits uniforms corresponding to uniformNames in the source collection
//
class CollectionEncoder {
 public:
  explicit CollectionEncoder(igl::BackendType backendType);

  void operator()(const Collection& collection,
                  igl::IRenderCommandEncoder& commandEncoder,
                  uint8_t bindTarget,
                  const std::vector<igl::NameHandle>& uniformNames) const noexcept;

 private:
  igl::BackendType backendType_;
};

} // namespace iglu::uniform
