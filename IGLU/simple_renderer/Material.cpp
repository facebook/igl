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
  setDepthTestConfig(device, depthTestConfig_);
}

std::shared_ptr<ShaderProgram> Material::shaderProgram() const {
  return shaderProgram_;
}

void Material::setShaderProgram(igl::IDevice& device,
                                const std::shared_ptr<ShaderProgram>& program) {
  shaderProgram_ = program;
  shaderUniforms_ =
      std::make_unique<ShaderUniforms>(device, shaderProgram_->renderPipelineReflection());
}

ShaderUniforms& Material::shaderUniforms() const {
  return *shaderUniforms_;
}

DepthTestConfig Material::depthTestConfig() const {
  return depthTestConfig_;
}

void Material::setDepthTestConfig(igl::IDevice& device, const DepthTestConfig& config) {
  depthTestConfig_ = config;

  igl::DepthStencilStateDesc depthDesc{
      .compareFunction = (depthTestConfig_ != DepthTestConfig::Disable)
                             ? igl::CompareFunction::Less
                             : igl::CompareFunction::AlwaysPass,
      .isDepthWriteEnabled = (depthTestConfig_ == DepthTestConfig::Enable),
  };
  depthState_ = device.createDepthStencilState(depthDesc, nullptr);
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

  shaderProgram_->populatePipelineDescriptor(pipelineDesc);
}

void Material::bind(igl::IDevice& device,
                    igl::IRenderPipelineState& pipelineState,
                    igl::IRenderCommandEncoder& commandEncoder) {
  shaderUniforms_->bind(device, pipelineState, commandEncoder);
  commandEncoder.bindDepthStencilState(depthState_);
}

} // namespace iglu::material
