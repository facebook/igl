/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#include "ShaderProgram.h"

#include <utility>

#include <igl/NameHandle.h>

namespace iglu::material {

#define CHECK_RESULT(res, outResPtr)              \
  if (!res.isOk()) {                              \
    if (outResPtr != nullptr) {                   \
      *outResPtr = res;                           \
    } else {                                      \
      IGL_DEBUG_ABORT("%s", res.message.c_str()); \
    }                                             \
    return;                                       \
  }

ShaderProgram::ShaderProgram(igl::IDevice& device,
                             std::shared_ptr<igl::IShaderModule> vertexShader,
                             std::shared_ptr<igl::IShaderModule> fragmentShader,
                             std::shared_ptr<igl::IVertexInputState> vis,
                             igl::Result* outResult) {
  igl::Result result;
  _shaderStages = igl::ShaderStagesCreator::fromRenderModules(
      device, std::move(vertexShader), std::move(fragmentShader), &result);
  CHECK_RESULT(result, outResult);
  init(device, std::move(vis), outResult);
}

ShaderProgram::ShaderProgram(igl::IDevice& device,
                             std::shared_ptr<igl::IShaderStages> shaderStages,
                             std::shared_ptr<igl::IVertexInputState> vis,
                             igl::Result* outResult) :
  _shaderStages(std::move(shaderStages)) {
  init(device, std::move(vis), outResult);
}

void ShaderProgram::init(igl::IDevice& device,
                         std::shared_ptr<igl::IVertexInputState> vis,
                         igl::Result* outResult) {
  igl::Result result;

  igl::RenderPipelineDesc pipelineDesc;
  pipelineDesc.shaderStages = _shaderStages;
  pipelineDesc.vertexInputState = std::move(vis);
  pipelineDesc.targetDesc.colorAttachments.resize(1);
  pipelineDesc.targetDesc.colorAttachments[0].textureFormat = igl::TextureFormat::RGBA_UNorm8;
  auto pipelineState = device.createRenderPipeline(pipelineDesc, &result);
  CHECK_RESULT(result, outResult);
  // Note that the check above might early return!
  _reflection = pipelineState->renderPipelineReflection();
}

const igl::IRenderPipelineReflection& ShaderProgram::renderPipelineReflection() const {
  return *_reflection;
}

void ShaderProgram::populatePipelineDescriptor(igl::RenderPipelineDesc& pipelineDesc) const {
  pipelineDesc.shaderStages = _shaderStages;
  for (const auto& entry : _reflection->allTextures()) {
    pipelineDesc.fragmentUnitSamplerMap[entry.textureIndex] = igl::genNameHandle(entry.name);
  }
}

} // namespace iglu::material
