/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/state_pool/RenderPipelineStatePool.h>

#include <igl/Device.h>

using namespace igl;

namespace iglu::state_pool {

///--------------------------------------
/// MARK: - RenderPipelineStatePool

std::shared_ptr<igl::IRenderPipelineState> RenderPipelineStatePool::createStateObject(
    igl::IDevice& dev,
    const igl::RenderPipelineDesc& desc,
    igl::Result* outResult) {
  return dev.createRenderPipeline(desc, outResult);
}

///--------------------------------------
/// MARK: - CountedRenderPipelineStatePool

CountedRenderPipelineStatePool::CountedRenderPipelineStatePool(uint8_t compactDelay) noexcept :
  compactDelay_(compactDelay) {}

std::shared_ptr<igl::IRenderPipelineState> CountedRenderPipelineStatePool::getOrCreate(
    igl::IDevice& dev,
    const igl::RenderPipelineDesc& desc,
    igl::Result* outResult) {
  auto stateIt = cache_.find(desc);
  if (stateIt != cache_.end()) {
    Result::setOk(outResult);
    // Reset the counter, we are using things.
    stateIt->second.second = 0;
    return stateIt->second.first;
  }

  auto state = dev.createRenderPipeline(desc, outResult);
  if (state) {
    cache_.emplace(desc, std::make_pair(state, 0));
    Result::setOk(outResult);
    return state;
  }
  return nullptr;
}

void CountedRenderPipelineStatePool::compact() {
  auto it = cache_.begin();
  while (it != cache_.end()) {
    // Only care about compacting the ones that own uniquely
    if (it->second.first.use_count() == 1) {
      auto& itemDelay = it->second.second;
      itemDelay += 1;
      if (itemDelay > compactDelay_) {
        it = cache_.erase(it);
        continue;
      }
    }
    it++;
  }
}

} // namespace iglu::state_pool
