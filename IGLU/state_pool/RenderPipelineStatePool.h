/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/state_pool/StatePool.h>
#include <igl/RenderPipelineState.h>

namespace igl {
class IDevice;
} // namespace igl

namespace iglu::state_pool {

class RenderPipelineStatePool
  : public LRUStatePool<igl::RenderPipelineDesc, igl::IRenderPipelineState> {
 public:
 private:
  std::shared_ptr<igl::IRenderPipelineState> createStateObject(igl::IDevice& dev,
                                                               const igl::RenderPipelineDesc& desc,
                                                               igl::Result* outResult) override;
};

/// Version of render pipeline state pool that does reference and "use count"ing.
/// Compacts and removes the cached pipeline states on expiration of use, if there
class CountedRenderPipelineStatePool final
  : public IStatePool<igl::RenderPipelineDesc, igl::IRenderPipelineState> {
 public:
  /// Create pipeline state pool, with a specified delay after which - a given pipeline
  /// state is no longer being pooled.
  explicit CountedRenderPipelineStatePool(uint8_t compactDelay) noexcept;

  std::shared_ptr<igl::IRenderPipelineState> getOrCreate(igl::IDevice& dev,
                                                         const igl::RenderPipelineDesc& desc,
                                                         igl::Result* outResult) override;

  /// Compact the render pipeline state, triggering deletion of elements in the
  /// pool that are expired.
  void compact();

 private:
  const uint8_t compactDelay_;
  std::unordered_map<igl::RenderPipelineDesc,
                     std::pair<std::shared_ptr<igl::IRenderPipelineState>, uint32_t>>
      cache_;
};

} // namespace iglu::state_pool
