/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#include "Material.h"

#include <utility>

namespace iglu::material {

Material::Material(igl::IDevice& device, std::string name) : name(std::move(name)) {
  setDepthTestConfig(device, _depthTestConfig);
}

std::shared_ptr<ShaderProgram> Material::shaderProgram() const {
  return _shaderProgram;
}

void Material::setShaderProgram(igl::IDevice& device,
                                const std::shared_ptr<ShaderProgram>& program) {
  _shaderProgram = program;
  _shaderUniforms =
      std::make_unique<ShaderUniforms>(device, _shaderProgram->renderPipelineReflection());
}

ShaderUniforms& Material::shaderUniforms() const {
  return *_shaderUniforms;
}

DepthTestConfig Material::depthTestConfig() const {
  return _depthTestConfig;
}

void Material::setDepthTestConfig(igl::IDevice& device, const DepthTestConfig& config) {
  _depthTestConfig = config;

  igl::DepthStencilStateDesc depthDesc;
  depthDesc.compareFunction = (_depthTestConfig != DepthTestConfig::Disable)
                                  ? igl::CompareFunction::Less
                                  : igl::CompareFunction::AlwaysPass;
  depthDesc.isDepthWriteEnabled = (_depthTestConfig == DepthTestConfig::Enable);
  _depthState = device.createDepthStencilState(depthDesc, nullptr);
}

void Material::populatePipelineDescriptor(igl::RenderPipelineDesc& pipelineDesc) const {
  // Assumption: 'blendMode' only applies to the first color attachment
  if (!pipelineDesc.targetDesc.colorAttachments.empty()) {
    auto& colorAttachment = pipelineDesc.targetDesc.colorAttachments[0];
    if (blendMode == BlendMode::Opaque()) {
      colorAttachment.blendEnabled = false;
    } else {
      colorAttachment.blendEnabled = true;
      colorAttachment.srcRGBBlendFactor = blendMode.srcRGB;
      colorAttachment.dstRGBBlendFactor = blendMode.dstRGB;
      colorAttachment.rgbBlendOp = blendMode.opRGB;
      colorAttachment.srcAlphaBlendFactor = blendMode.srcAlpha;
      colorAttachment.dstAlphaBlendFactor = blendMode.dstAlpha;
      colorAttachment.alphaBlendOp = blendMode.opAlpha;
    }
  }

  pipelineDesc.cullMode = cullMode;

  _shaderProgram->populatePipelineDescriptor(pipelineDesc);
}

void Material::bind(igl::IDevice& device,
                    igl::IRenderPipelineState& pipelineState,
                    igl::IRenderCommandEncoder& commandEncoder) {
  _shaderUniforms->bind(device, pipelineState, commandEncoder);
  commandEncoder.bindDepthStencilState(_depthState);
}

} // namespace iglu::material
