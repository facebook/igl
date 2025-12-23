/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <vector>
#include <igl/ComputePipelineState.h>
#include <igl/RenderPipelineState.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class D3D12Context;

/**
 * @brief Fluent builder for D3D12 graphics pipeline state objects
 *
 * Encapsulates the complex setup of D3D12_GRAPHICS_PIPELINE_STATE_DESC
 * and provides a clean, chainable API similar to VulkanPipelineBuilder.
 *
 * Usage:
 *   D3D12GraphicsPipelineBuilder builder;
 *   builder.shaderBytecode(vsBytecode, psBytecode)
 *          .vertexInputLayout(inputElements)
 *          .blendState(blendDesc)
 *          .rasterizerState(rasterizerDesc)
 *          .depthStencilState(depthStencilDesc)
 *          .renderTargetFormats(rtvFormats)
 *          .sampleCount(sampleCount)
 *          .primitiveTopology(topology);
 *   auto result = builder.build(device, rootSignature, outPipelineState);
 */
class D3D12GraphicsPipelineBuilder final {
 public:
  D3D12GraphicsPipelineBuilder();
  ~D3D12GraphicsPipelineBuilder() = default;

  // Shader configuration
  D3D12GraphicsPipelineBuilder& vertexShader(const std::vector<uint8_t>& bytecode);
  D3D12GraphicsPipelineBuilder& pixelShader(const std::vector<uint8_t>& bytecode);
  D3D12GraphicsPipelineBuilder& shaderBytecode(const std::vector<uint8_t>& vs,
                                               const std::vector<uint8_t>& ps);

  // Vertex input layout
  D3D12GraphicsPipelineBuilder& vertexInputLayout(
      const std::vector<D3D12_INPUT_ELEMENT_DESC>& elements);

  // Blend state
  D3D12GraphicsPipelineBuilder& blendState(const D3D12_BLEND_DESC& desc);
  D3D12GraphicsPipelineBuilder& blendStateForAttachment(
      UINT attachmentIndex,
      const RenderPipelineDesc::TargetDesc::ColorAttachment& attachment);

  // Rasterizer state
  D3D12GraphicsPipelineBuilder& rasterizerState(const D3D12_RASTERIZER_DESC& desc);
  D3D12GraphicsPipelineBuilder& cullMode(CullMode mode);
  D3D12GraphicsPipelineBuilder& frontFaceWinding(WindingMode mode);
  D3D12GraphicsPipelineBuilder& polygonFillMode(PolygonFillMode mode);

  // Depth-stencil state
  D3D12GraphicsPipelineBuilder& depthStencilState(const D3D12_DEPTH_STENCIL_DESC& desc);
  D3D12GraphicsPipelineBuilder& depthTestEnabled(bool enabled);
  D3D12GraphicsPipelineBuilder& depthWriteEnabled(bool enabled);
  D3D12GraphicsPipelineBuilder& depthCompareFunc(D3D12_COMPARISON_FUNC func);

  // Render target configuration
  D3D12GraphicsPipelineBuilder& renderTargetFormat(UINT index, DXGI_FORMAT format);
  D3D12GraphicsPipelineBuilder& renderTargetFormats(const std::vector<DXGI_FORMAT>& formats);
  D3D12GraphicsPipelineBuilder& depthStencilFormat(DXGI_FORMAT format);
  D3D12GraphicsPipelineBuilder& numRenderTargets(UINT count);

  // Sample configuration
  D3D12GraphicsPipelineBuilder& sampleCount(UINT count);
  D3D12GraphicsPipelineBuilder& sampleMask(UINT mask);

  // Primitive topology
  D3D12GraphicsPipelineBuilder& primitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE type);

  // Stream output (optional)
  D3D12GraphicsPipelineBuilder& streamOutput(const D3D12_STREAM_OUTPUT_DESC& desc);

  // Build the pipeline state object
  [[nodiscard]] Result build(ID3D12Device* device,
                             ID3D12RootSignature* rootSignature,
                             ID3D12PipelineState** outPipelineState,
                             const char* debugName = nullptr);

  // Get the current PSO desc (for inspection/debugging)
  [[nodiscard]] const D3D12_GRAPHICS_PIPELINE_STATE_DESC& getDesc() const {
    return psoDesc_;
  }

 private:
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc_;
  std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements_;
  std::vector<uint8_t> vsBytecode_;
  std::vector<uint8_t> psBytecode_;
};

/**
 * @brief Fluent builder for D3D12 compute pipeline state objects
 *
 * Simplified builder for compute shaders.
 *
 * Usage:
 *   D3D12ComputePipelineBuilder builder;
 *   builder.shaderBytecode(csBytecode);
 *   auto result = builder.build(device, rootSignature, outPipelineState);
 */
class D3D12ComputePipelineBuilder final {
 public:
  D3D12ComputePipelineBuilder();
  ~D3D12ComputePipelineBuilder() = default;

  // Shader configuration
  D3D12ComputePipelineBuilder& shaderBytecode(const std::vector<uint8_t>& bytecode);

  // Build the pipeline state object
  [[nodiscard]] Result build(ID3D12Device* device,
                             ID3D12RootSignature* rootSignature,
                             ID3D12PipelineState** outPipelineState,
                             const char* debugName = nullptr);

  // Get the current PSO desc (for inspection/debugging)
  [[nodiscard]] const D3D12_COMPUTE_PIPELINE_STATE_DESC& getDesc() const {
    return psoDesc_;
  }

 private:
  D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc_;
  std::vector<uint8_t> csBytecode_;
};

/**
 * @brief Builder for D3D12 root signatures
 *
 * Encapsulates root signature creation with support for:
 * - Root constants (push constants)
 * - Root descriptors (CBVs)
 * - Descriptor tables (CBV/SRV/UAV/Sampler)
 * - Automatic cost calculation and validation
 *
 * Usage:
 *   D3D12RootSignatureBuilder builder;
 *   builder.addRootConstants(shaderRegister, num32BitValues)
 *          .addRootCBV(shaderRegister)
 *          .addDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, count, baseRegister);
 *   auto result = builder.build(device, context, outRootSignature);
 */
class D3D12RootSignatureBuilder final {
 public:
  D3D12RootSignatureBuilder();
  ~D3D12RootSignatureBuilder() = default;

  // Root constants (inline 32-bit values)
  D3D12RootSignatureBuilder& addRootConstants(UINT shaderRegister,
                                              UINT num32BitValues,
                                              UINT registerSpace = 0);

  // Root descriptors (CBV/SRV/UAV accessed directly via GPU virtual address)
  D3D12RootSignatureBuilder& addRootCBV(UINT shaderRegister, UINT registerSpace = 0);
  D3D12RootSignatureBuilder& addRootSRV(UINT shaderRegister, UINT registerSpace = 0);
  D3D12RootSignatureBuilder& addRootUAV(UINT shaderRegister, UINT registerSpace = 0);

  // Descriptor tables
  D3D12RootSignatureBuilder& addDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
                                                UINT numDescriptors,
                                                UINT baseShaderRegister,
                                                UINT registerSpace = 0);

  // Flags
  D3D12RootSignatureBuilder& flags(D3D12_ROOT_SIGNATURE_FLAGS flags);

  // Build the root signature
  // Note: context parameter is reserved for future tier-based validation.
  // Currently, callers should use getMaxDescriptorCount() when configuring
  // descriptor tables to ensure hardware compatibility.
  [[nodiscard]] Result build(ID3D12Device* device,
                             const D3D12Context* context,
                             ID3D12RootSignature** outRootSignature);

  // Query limits from device - use this when calling addDescriptorTable()
  // to ensure descriptor counts are within hardware tier limits
  static UINT getMaxDescriptorCount(const D3D12Context* context,
                                    D3D12_DESCRIPTOR_RANGE_TYPE rangeType);

  // Calculate root signature size in DWORDs (must be <= 64)
  [[nodiscard]] uint32_t getDwordSize() const;

 private:
  struct DescriptorRange {
    D3D12_DESCRIPTOR_RANGE range;
  };

  struct RootParameter {
    D3D12_ROOT_PARAMETER param;
    std::vector<DescriptorRange> ranges; // For descriptor tables
  };

  std::vector<RootParameter> rootParameters_;
  D3D12_ROOT_SIGNATURE_FLAGS flags_ = D3D12_ROOT_SIGNATURE_FLAG_NONE;
};

} // namespace igl::d3d12
