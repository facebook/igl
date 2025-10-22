/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/RenderPipelineState.h>
#include <igl/RenderPipelineReflection.h>
#include <igl/NameHandle.h>
#include <igl/d3d12/VertexInputState.h>

namespace igl::d3d12 {

RenderPipelineState::RenderPipelineState(const RenderPipelineDesc& desc,
                                         Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState,
                                         Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature)
    : IRenderPipelineState(desc),
      pipelineState_(std::move(pipelineState)),
      rootSignature_(std::move(rootSignature)) {
  // Cache the vertex stride from the vertex input state binding (slot 0) if available
  const auto& vis = desc.vertexInputState;
  if (vis) {
    // Try backend downcast to extract VertexInputStateDesc
    if (auto* d3dVis = dynamic_cast<const igl::d3d12::VertexInputState*>(vis.get())) {
      const auto& d = d3dVis->getDesc();
      if (d.numInputBindings > 0) {
        vertexStride_ = static_cast<uint32_t>(d.inputBindings[0].stride);
      }
    }
  }
}

std::shared_ptr<IRenderPipelineReflection> RenderPipelineState::renderPipelineReflection() {
  if (!reflection_) {
    struct DummyReflection final : public IRenderPipelineReflection {
      DummyReflection() {
        // Minimal entries to satisfy IGLU imgui and simple materials
        BufferArgDesc ub;
        ub.name = igl::genNameHandle("vertexBuffer");
        ub.bufferAlignment = 256; // D3D12 CBV alignment
        ub.bufferDataSize = 64;    // one mat4
        ub.bufferIndex = 0;
        ub.shaderStage = ShaderStage::Vertex;
        ub.isUniformBlock = true;

        BufferArgDesc::BufferMemberDesc mProj;
        mProj.name = igl::genNameHandle("projectionMatrix");
        mProj.type = UniformType::Mat4x4;
        mProj.offset = 0;
        mProj.arrayLength = 1;
        ub.members.push_back(mProj);
        uniformBuffers_.push_back(std::move(ub));

        TextureArgDesc tex;
        tex.name = "texture";
        tex.type = TextureType::TwoD;
        tex.textureIndex = 0;
        tex.shaderStage = ShaderStage::Fragment;
        textures_.push_back(std::move(tex));

        SamplerArgDesc samp;
        samp.name = "texture";
        samp.samplerIndex = 0;
        samp.shaderStage = ShaderStage::Fragment;
        samplers_.push_back(std::move(samp));
      }

      const std::vector<BufferArgDesc>& allUniformBuffers() const override { return uniformBuffers_; }
      const std::vector<SamplerArgDesc>& allSamplers() const override { return samplers_; }
      const std::vector<TextureArgDesc>& allTextures() const override { return textures_; }

      std::vector<BufferArgDesc> uniformBuffers_;
      std::vector<SamplerArgDesc> samplers_;
      std::vector<TextureArgDesc> textures_;
    };
    reflection_ = std::make_shared<DummyReflection>();
  }
  return reflection_;
}

void RenderPipelineState::setRenderPipelineReflection(
    const IRenderPipelineReflection& /*renderPipelineReflection*/) {}

int RenderPipelineState::getIndexByName(const igl::NameHandle& /*name*/,
                                        ShaderStage /*stage*/) const {
  return -1;
}

int RenderPipelineState::getIndexByName(const std::string& /*name*/,
                                        ShaderStage /*stage*/) const {
  return -1;
}

} // namespace igl::d3d12
