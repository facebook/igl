/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#include "Drawable.h"

#include <utility>

namespace iglu::drawable {

Drawable::Drawable(std::shared_ptr<vertexdata::VertexData> vertexData,
                   std::shared_ptr<material::Material> material) :
  vertexData_(std::move(vertexData)), material_(std::move(material)) {}

void Drawable::draw(igl::IDevice& device,
                    igl::IRenderCommandEncoder& commandEncoder,
                    const igl::RenderPipelineDesc& pipelineDesc,
                    size_t pushConstantsDataSize,
                    const void* pushConstantsData) {
  // Assumption: _vertexData and _material are immutable
  const size_t pipelineDescHash = std::hash<igl::RenderPipelineDesc>()(pipelineDesc);
  if (!pipelineState_ || pipelineDescHash != lastPipelineDescHash_) {
    igl::RenderPipelineDesc mutablePipelineDesc = pipelineDesc;
    vertexData_->populatePipelineDescriptor(mutablePipelineDesc);
    material_->populatePipelineDescriptor(mutablePipelineDesc);

    pipelineState_ = device.createRenderPipeline(mutablePipelineDesc, nullptr);
    lastPipelineDescHash_ = pipelineDescHash;
  }

  commandEncoder.bindRenderPipelineState(pipelineState_);

  material_->bind(device, *pipelineState_, commandEncoder);

  if (pushConstantsData && pushConstantsDataSize) {
    commandEncoder.bindPushConstants(pushConstantsData, pushConstantsDataSize);
  }

  vertexData_->draw(commandEncoder);
}

} // namespace iglu::drawable
