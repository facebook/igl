/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/uniform/CollectionEncoder.h>

#include <IGLU/uniform/Collection.h>
#include <IGLU/uniform/Encoder.h>

namespace iglu::uniform {

CollectionEncoder::CollectionEncoder(igl::BackendType backendType) : backendType_(backendType) {}

void CollectionEncoder::operator()(
    const Collection& collection,
    igl::IRenderCommandEncoder& commandEncoder,
    uint8_t bindTarget,
    const std::vector<igl::NameHandle>& uniformNames) const noexcept {
  const Encoder uniformEncoder(backendType_);
  for (const auto& name : uniformNames) {
    uniformEncoder(commandEncoder, bindTarget, collection.get(name));
  }
}

} // namespace iglu::uniform
