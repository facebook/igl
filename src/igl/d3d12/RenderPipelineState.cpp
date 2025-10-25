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
#include <igl/d3d12/ShaderModule.h>
#include <d3dcompiler.h>

namespace igl::d3d12 {

RenderPipelineState::RenderPipelineState(const RenderPipelineDesc& desc,
                                         Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState,
                                         Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature)
    : IRenderPipelineState(desc),
      pipelineState_(std::move(pipelineState)),
      rootSignature_(std::move(rootSignature)) {
  // Convert IGL primitive topology to D3D12 primitive topology
  switch (desc.topology) {
    case PrimitiveType::Point:
      primitiveTopology_ = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
      IGL_LOG_INFO("RenderPipelineState: Set topology to POINTLIST\n");
      break;
    case PrimitiveType::Line:
      primitiveTopology_ = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
      IGL_LOG_INFO("RenderPipelineState: Set topology to LINELIST\n");
      break;
    case PrimitiveType::LineStrip:
      primitiveTopology_ = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
      IGL_LOG_INFO("RenderPipelineState: Set topology to LINESTRIP\n");
      break;
    case PrimitiveType::Triangle:
      primitiveTopology_ = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
      IGL_LOG_INFO("RenderPipelineState: Set topology to TRIANGLELIST\n");
      break;
    case PrimitiveType::TriangleStrip:
      primitiveTopology_ = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
      IGL_LOG_INFO("RenderPipelineState: Set topology to TRIANGLESTRIP\n");
      break;
  }

  // Cache the vertex stride from the vertex input state binding (slot 0) if available
  const auto& vis = desc.vertexInputState;
  if (vis) {
    // Try backend downcast to extract VertexInputStateDesc
    if (auto* d3dVis = dynamic_cast<const igl::d3d12::VertexInputState*>(vis.get())) {
      const auto& d = d3dVis->getDesc();
      if (d.numInputBindings > 0) {
        vertexStride_ = static_cast<uint32_t>(d.inputBindings[0].stride);
        // Cache per-slot strides
        for (size_t s = 0; s < d.numInputBindings && s < IGL_BUFFER_BINDINGS_MAX; ++s) {
          vertexStrides_[s] = static_cast<uint32_t>(d.inputBindings[s].stride);
        }
        // If attributes reference slots beyond numInputBindings or strides are zero,
        // derive reasonable defaults so sessions that bind to slot 1 still work.
        size_t maxSlot = 0;
        for (size_t i = 0; i < d.numAttributes; ++i) {
          if (d.attributes[i].bufferIndex > maxSlot) {
            maxSlot = d.attributes[i].bufferIndex;
          }
        }
        // Helper to compute a minimal stride per slot from attributes (max end offset among attrs in that slot)
        auto computeStrideForSlot = [&](size_t slot) -> uint32_t {
          size_t maxEnd = 0;
          for (size_t i = 0; i < d.numAttributes; ++i) {
            const auto& a = d.attributes[i];
            if (a.bufferIndex != slot) continue;
            size_t compSize = 0;
            switch (a.format) {
              case VertexAttributeFormat::Float1: compSize = 4; break;
              case VertexAttributeFormat::Float2: compSize = 8; break;
              case VertexAttributeFormat::Float3: compSize = 12; break;
              case VertexAttributeFormat::Float4: compSize = 16; break;
              case VertexAttributeFormat::Byte1: compSize = 1; break;
              case VertexAttributeFormat::Byte2: compSize = 2; break;
              case VertexAttributeFormat::Byte4: compSize = 4; break;
              case VertexAttributeFormat::UByte4Norm: compSize = 4; break;
              default: compSize = 0; break;
            }
            maxEnd = std::max(maxEnd, a.offset + compSize);
          }
          // Fallback to slot0 stride if present
          if (maxEnd == 0 && d.numInputBindings > 0) {
            return static_cast<uint32_t>(d.inputBindings[0].stride);
          }
          return static_cast<uint32_t>(maxEnd);
        };
        for (size_t s = 0; s <= maxSlot && s < IGL_BUFFER_BINDINGS_MAX; ++s) {
          if (vertexStrides_[s] == 0) {
            vertexStrides_[s] = computeStrideForSlot(s);
          }
        }
        if (vertexStride_ == 0) {
          vertexStride_ = vertexStrides_[0];
        }
      }
    }
  }
}

std::shared_ptr<IRenderPipelineReflection> RenderPipelineState::renderPipelineReflection() {
  if (reflection_) {
    return reflection_;
  }

  struct ReflectionImpl final : public IRenderPipelineReflection {
    std::vector<BufferArgDesc> ubs;
    std::vector<SamplerArgDesc> samplers;
    std::vector<TextureArgDesc> textures;
    const std::vector<BufferArgDesc>& allUniformBuffers() const override { return ubs; }
    const std::vector<SamplerArgDesc>& allSamplers() const override { return samplers; }
    const std::vector<TextureArgDesc>& allTextures() const override { return textures; }
  };

  auto out = std::make_shared<ReflectionImpl>();

  auto mapUniformType = [](const D3D12_SHADER_TYPE_DESC& td) -> UniformType {
    if ((td.Class == D3D_SVC_MATRIX_ROWS || td.Class == D3D_SVC_MATRIX_COLUMNS) && td.Rows == 4 && td.Columns == 4) {
      return UniformType::Mat4x4;
    }
    if (td.Type == D3D_SVT_FLOAT) {
      if (td.Class == D3D_SVC_SCALAR) return UniformType::Float;
      if (td.Class == D3D_SVC_VECTOR) {
        switch (td.Columns) {
          case 2: return UniformType::Float2;
          case 3: return UniformType::Float3;
          case 4: return UniformType::Float4;
          default: return UniformType::Invalid;
        }
      }
    }
    return UniformType::Invalid;
  };

  auto reflectShader = [&](const std::shared_ptr<IShaderModule>& mod, ShaderStage stage) {
    if (!mod) return;
    auto* d3dMod = dynamic_cast<const igl::d3d12::ShaderModule*>(mod.get());
    if (!d3dMod) return;
    const auto& bc = d3dMod->getBytecode();
    if (bc.empty()) return;
    Microsoft::WRL::ComPtr<ID3D12ShaderReflection> refl;
    if (FAILED(D3DReflect(bc.data(), bc.size(), IID_PPV_ARGS(refl.GetAddressOf())))) return;
    D3D12_SHADER_DESC sd{};
    if (FAILED(refl->GetDesc(&sd))) return;

    // Constant buffers
    for (UINT i = 0; i < sd.ConstantBuffers; ++i) {
      auto* cb = refl->GetConstantBufferByIndex(i);
      D3D12_SHADER_BUFFER_DESC cbd{}; if (FAILED(cb->GetDesc(&cbd))) continue;
      int bufferIndex = -1;
      for (UINT r = 0; r < sd.BoundResources; ++r) {
        D3D12_SHADER_INPUT_BIND_DESC bind{};
        if (SUCCEEDED(refl->GetResourceBindingDesc(r, &bind))) {
          if (bind.Type == D3D_SIT_CBUFFER && std::string(bind.Name) == std::string(cbd.Name)) {
            bufferIndex = static_cast<int>(bind.BindPoint);
            break;
          }
        }
      }
      BufferArgDesc ub;
      ub.name = igl::genNameHandle(cbd.Name ? cbd.Name : "");
      ub.bufferAlignment = 256;
      ub.bufferDataSize = cbd.Size;
      ub.bufferIndex = bufferIndex;
      ub.shaderStage = stage;
      ub.isUniformBlock = true;
      for (UINT v = 0; v < cbd.Variables; ++v) {
        auto* var = cb->GetVariableByIndex(v);
        D3D12_SHADER_VARIABLE_DESC vd{}; if (FAILED(var->GetDesc(&vd))) continue;
        auto* t = var->GetType(); if (!t) continue;
        D3D12_SHADER_TYPE_DESC td{}; if (FAILED(t->GetDesc(&td))) continue;
        BufferArgDesc::BufferMemberDesc m;
        m.name = igl::genNameHandle(vd.Name ? vd.Name : "");
        m.type = mapUniformType(td);
        m.offset = vd.StartOffset;
        m.arrayLength = td.Elements ? td.Elements : 1;
        ub.members.push_back(std::move(m));
      }
      out->ubs.push_back(std::move(ub));
    }

    // Textures and samplers
    for (UINT r = 0; r < sd.BoundResources; ++r) {
      D3D12_SHADER_INPUT_BIND_DESC bind{};
      if (FAILED(refl->GetResourceBindingDesc(r, &bind))) continue;
      if (bind.Type == D3D_SIT_TEXTURE) {
        TextureArgDesc t; t.name = bind.Name ? bind.Name : ""; t.type = TextureType::TwoD; t.textureIndex = bind.BindPoint; t.shaderStage = stage; out->textures.push_back(std::move(t));
      } else if (bind.Type == D3D_SIT_SAMPLER) {
        SamplerArgDesc s; s.name = bind.Name ? bind.Name : ""; s.samplerIndex = bind.BindPoint; s.shaderStage = stage; out->samplers.push_back(std::move(s));
      }
    }
  };

  if (auto stages = getRenderPipelineDesc().shaderStages) {
    reflectShader(stages->getVertexModule(), ShaderStage::Vertex);
    reflectShader(stages->getFragmentModule(), ShaderStage::Fragment);
  }

  reflection_ = out;
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
