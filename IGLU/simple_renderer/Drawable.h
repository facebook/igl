/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#pragma once

#include <IGLU/simple_renderer/Material.h>
#include <IGLU/simple_renderer/VertexData.h>
#include <memory>

namespace iglu::drawable {

/// A drawable aggregates all the data and configurations for a single draw call.
///
class Drawable final {
 public:
  /// Binds all relevant states and issues a draw call on 'commandEncoder'.
  /// It takes a render pipeline descriptor as input, which is expected to be
  /// populated with accurate framebuffer information; all other "draw call"
  /// related configurations will be handled internally.
  void draw(igl::IDevice& device,
            igl::IRenderCommandEncoder& commandEncoder,
            const igl::RenderPipelineDesc& pipelineDesc,
            size_t pushConstantsDataSize = 0,
            const void* pushConstantsData = nullptr);

  /// A Drawable is "immutable" in that there's no API to modify its inputs after
  /// creation. They're lightweight objects and should be recreated instead of updated.
  Drawable(std::shared_ptr<vertexdata::VertexData> vertexData,
           std::shared_ptr<material::Material> material);
  ~Drawable() = default;

 private:
  std::shared_ptr<vertexdata::VertexData> _vertexData;
  std::shared_ptr<material::Material> _material;

  std::shared_ptr<igl::IRenderPipelineState> _pipelineState;
  size_t _lastPipelineDescHash = 0;
};

} // namespace iglu::drawable
