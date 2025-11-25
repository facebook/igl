/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/RenderPipelineState.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/Common.h>
#include <igl/RenderPipelineReflection.h>
#include <igl/NameHandle.h>
#include <igl/d3d12/VertexInputState.h>
#include <igl/d3d12/ShaderModule.h>
#include <igl/d3d12/D3D12ReflectionUtils.h>
#include <d3dcompiler.h>

namespace igl::d3d12 {

RenderPipelineState::RenderPipelineState(const RenderPipelineDesc& desc,
                                         igl::d3d12::ComPtr<ID3D12PipelineState> pipelineState,
                                         igl::d3d12::ComPtr<ID3D12RootSignature> rootSignature)
    : IRenderPipelineState(desc),
      pipelineState_(std::move(pipelineState)),
      rootSignature_(std::move(rootSignature)) {
  // Set D3D12 object names for PIX debugging
  const std::string& debugName = desc.debugName.toString();
  if (pipelineState_.Get() && !debugName.empty()) {
    std::wstring wideName(debugName.begin(), debugName.end());
    pipelineState_->SetName((L"PSO_" + wideName).c_str());
    IGL_D3D12_LOG_VERBOSE("RenderPipelineState: Set PIX debug name 'PSO_%s'\n", debugName.c_str());
  }
  if (rootSignature_.Get() && !debugName.empty()) {
    std::wstring wideName(debugName.begin(), debugName.end());
    rootSignature_->SetName((L"RootSig_" + wideName).c_str());
    IGL_D3D12_LOG_VERBOSE("RenderPipelineState: Set PIX root signature name 'RootSig_%s'\n", debugName.c_str());
  }

  // Extract shader reflection info for future dynamic binding support
  if (desc.shaderStages) {
    auto* vertexModule = static_cast<const ShaderModule*>(desc.shaderStages->getVertexModule().get());
    auto* fragmentModule = static_cast<const ShaderModule*>(desc.shaderStages->getFragmentModule().get());

    // Prefer vertex shader for push constants if both define them
    if (vertexModule) {
      const auto& vsReflection = vertexModule->getReflectionInfo();
      if (vsReflection.hasPushConstants) {
        shaderReflection_.hasPushConstants = true;
        shaderReflection_.pushConstantSlot = vsReflection.pushConstantSlot;
        shaderReflection_.pushConstantSize = vsReflection.pushConstantSize;
        IGL_D3D12_LOG_VERBOSE("RenderPipelineState: VS push constants at b%u (%u DWORDs)\n",
                     vsReflection.pushConstantSlot, vsReflection.pushConstantSize);
      }
    }

    // Use fragment shader push constants if vertex shader doesn't have them
    if (!shaderReflection_.hasPushConstants && fragmentModule) {
      const auto& psReflection = fragmentModule->getReflectionInfo();
      if (psReflection.hasPushConstants) {
        shaderReflection_.hasPushConstants = true;
        shaderReflection_.pushConstantSlot = psReflection.pushConstantSlot;
        shaderReflection_.pushConstantSize = psReflection.pushConstantSize;
        IGL_D3D12_LOG_VERBOSE("RenderPipelineState: PS push constants at b%u (%u DWORDs)\n",
                     psReflection.pushConstantSlot, psReflection.pushConstantSize);
      }
    }
  }

  // Convert IGL primitive topology to D3D12 primitive topology
  switch (desc.topology) {
    case PrimitiveType::Point:
      primitiveTopology_ = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
      IGL_D3D12_LOG_VERBOSE("RenderPipelineState: Set topology to POINTLIST\n");
      break;
    case PrimitiveType::Line:
      primitiveTopology_ = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
      IGL_D3D12_LOG_VERBOSE("RenderPipelineState: Set topology to LINELIST\n");
      break;
    case PrimitiveType::LineStrip:
      primitiveTopology_ = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
      IGL_D3D12_LOG_VERBOSE("RenderPipelineState: Set topology to LINESTRIP\n");
      break;
    case PrimitiveType::Triangle:
      primitiveTopology_ = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
      IGL_D3D12_LOG_VERBOSE("RenderPipelineState: Set topology to TRIANGLELIST\n");
      break;
    case PrimitiveType::TriangleStrip:
      primitiveTopology_ = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
      IGL_D3D12_LOG_VERBOSE("RenderPipelineState: Set topology to TRIANGLESTRIP\n");
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

  auto reflectShader = [&](const std::shared_ptr<IShaderModule>& mod, ShaderStage stage) {
    if (!mod) return;
    auto* d3dMod = dynamic_cast<const igl::d3d12::ShaderModule*>(mod.get());
    if (!d3dMod) return;
    const auto& bc = d3dMod->getBytecode();
    if (bc.empty()) return;
    igl::d3d12::ComPtr<ID3D12ShaderReflection> refl;
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
        m.type = ReflectionUtils::mapUniformType(td);
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

ID3D12PipelineState* RenderPipelineState::getPipelineState(
    const D3D12RenderPipelineDynamicState& dynamicState,
    Device& device) const {
  // Fast path: Check if dynamic state matches base PSO
  // This happens when pipeline was created with same formats as framebuffer
  const auto& desc = getRenderPipelineDesc();
  bool matchesBasePSO = true;

  // Check render target formats
  const UINT numRTs = static_cast<UINT>(
      std::min<size_t>(desc.targetDesc.colorAttachments.size(),
                      D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT));
  for (UINT i = 0; i < numRTs; ++i) {
    if (dynamicState.rtvFormats[i] !=
        textureFormatToDXGIFormat(desc.targetDesc.colorAttachments[i].textureFormat)) {
      matchesBasePSO = false;
      break;
    }
  }

  // Check depth-stencil format
  if (matchesBasePSO) {
    const DXGI_FORMAT baseDSVFormat =
        (desc.targetDesc.depthAttachmentFormat != TextureFormat::Invalid)
            ? textureFormatToDXGIFormat(desc.targetDesc.depthAttachmentFormat)
            : DXGI_FORMAT_UNKNOWN;
    if (dynamicState.dsvFormat != baseDSVFormat) {
      matchesBasePSO = false;
    }
  }

  // Return base PSO if formats match
  if (matchesBasePSO) {
    return pipelineState_.Get();
  }

  // Check variant cache
  auto it = psoVariants_.find(dynamicState);
  if (it != psoVariants_.end()) {
    return it->second.Get();
  }

  // Create PSO variant with substituted formats (Vulkan-style on-demand creation)
  IGL_LOG_INFO("Creating PSO variant: RTV[0]=%d (base) -> %d (framebuffer)\n",
                textureFormatToDXGIFormat(desc.targetDesc.colorAttachments[0].textureFormat),
                dynamicState.rtvFormats[0]);

  // Following Vulkan's approach: create modified RenderPipelineDesc with substituted formats
  // Create a modified descriptor with framebuffer formats substituted
  RenderPipelineDesc variantDesc = desc;  // Copy all state

  // Substitute RT formats from actual framebuffer
  for (UINT i = 0; i < numRTs; ++i) {
    if (dynamicState.rtvFormats[i] != DXGI_FORMAT_UNKNOWN) {
      // Convert DXGI format back to IGL TextureFormat
      variantDesc.targetDesc.colorAttachments[i].textureFormat =
          dxgiFormatToTextureFormat(dynamicState.rtvFormats[i]);
      IGL_LOG_INFO("  RTV[%u]: substituted format %d\n", i, dynamicState.rtvFormats[i]);
    }
  }

  // Substitute DSV format if present
  if (dynamicState.dsvFormat != DXGI_FORMAT_UNKNOWN) {
    variantDesc.targetDesc.depthAttachmentFormat =
        dxgiFormatToTextureFormat(dynamicState.dsvFormat);
  }

  // Call Device::createPipelineStateVariant() to create PSO with modified formats
  Result variantResult;
  auto variantPSO = device.createPipelineStateVariant(
      variantDesc, rootSignature_.Get(), &variantResult);

  if (!variantPSO.Get()) {
    IGL_LOG_ERROR("PSO variant creation failed: %s\n", variantResult.message.c_str());
    IGL_LOG_ERROR("Falling back to base PSO (this will cause D3D12 validation errors!)\n");
    return pipelineState_.Get();  // Fallback to base PSO
  }

  // Cache the variant for future use
  psoVariants_[dynamicState] = variantPSO;
  IGL_LOG_INFO("PSO variant created and cached successfully: PSO=%p\n", variantPSO.Get());

  return variantPSO.Get();
}

} // namespace igl::d3d12
