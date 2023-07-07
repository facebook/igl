/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Drawable.h"

#include <utility>

namespace iglu {
namespace drawable {

Drawable::Drawable(std::shared_ptr<vertexdata::VertexData> vertexData,
                   std::shared_ptr<material::Material> material) :
  _vertexData(std::move(vertexData)), _material(std::move(material)) {}

void Drawable::draw(igl::IDevice& device,
                    igl::IRenderCommandEncoder& commandEncoder,
                    const igl::RenderPipelineDesc& pipelineDesc) {
  // Assumption: _vertexData and _material are immutable
  size_t pipelineDescHash = std::hash<igl::RenderPipelineDesc>()(pipelineDesc);
  if (!_pipelineState || pipelineDescHash != _lastPipelineDescHash) {
    igl::RenderPipelineDesc mutablePipelineDesc = pipelineDesc;
    _vertexData->populatePipelineDescriptor(mutablePipelineDesc);
    _material->populatePipelineDescriptor(mutablePipelineDesc);

    _pipelineState = device.createRenderPipeline(mutablePipelineDesc, nullptr);
    _lastPipelineDescHash = pipelineDescHash;
  }

  commandEncoder.bindRenderPipelineState(_pipelineState);

  _material->bind(device, *_pipelineState, commandEncoder);

  _vertexData->draw(commandEncoder);
}

} // namespace drawable
} // namespace iglu
