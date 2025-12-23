/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/D3D12PipelineBuilder.h>

#include <igl/d3d12/D3D12Context.h>

namespace igl::d3d12 {

namespace {

// Helper function to calculate root signature size in DWORDs
uint32_t getRootSignatureDwordSize(const D3D12_ROOT_SIGNATURE_DESC& desc) {
  uint32_t totalSize = 0;

  for (uint32_t i = 0; i < desc.NumParameters; ++i) {
    const auto& param = desc.pParameters[i];

    switch (param.ParameterType) {
    case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
      totalSize += param.Constants.Num32BitValues;
      break;
    case D3D12_ROOT_PARAMETER_TYPE_CBV:
    case D3D12_ROOT_PARAMETER_TYPE_SRV:
    case D3D12_ROOT_PARAMETER_TYPE_UAV:
      totalSize += 2; // Root descriptors cost 2 DWORDs
      break;
    case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
      totalSize += 1; // Descriptor tables cost 1 DWORD
      break;
    }
  }

  return totalSize;
}

// Helper to convert IGL blend factor to D3D12
D3D12_BLEND toD3D12Blend(BlendFactor f) {
  switch (f) {
  case BlendFactor::Zero:
    return D3D12_BLEND_ZERO;
  case BlendFactor::One:
    return D3D12_BLEND_ONE;
  case BlendFactor::SrcColor:
    return D3D12_BLEND_SRC_COLOR;
  case BlendFactor::OneMinusSrcColor:
    return D3D12_BLEND_INV_SRC_COLOR;
  case BlendFactor::SrcAlpha:
    return D3D12_BLEND_SRC_ALPHA;
  case BlendFactor::OneMinusSrcAlpha:
    return D3D12_BLEND_INV_SRC_ALPHA;
  case BlendFactor::DstColor:
    return D3D12_BLEND_DEST_COLOR;
  case BlendFactor::OneMinusDstColor:
    return D3D12_BLEND_INV_DEST_COLOR;
  case BlendFactor::DstAlpha:
    return D3D12_BLEND_DEST_ALPHA;
  case BlendFactor::OneMinusDstAlpha:
    return D3D12_BLEND_INV_DEST_ALPHA;
  case BlendFactor::SrcAlphaSaturated:
    return D3D12_BLEND_SRC_ALPHA_SAT;
  case BlendFactor::BlendColor:
    return D3D12_BLEND_BLEND_FACTOR;
  case BlendFactor::OneMinusBlendColor:
    return D3D12_BLEND_INV_BLEND_FACTOR;
  case BlendFactor::BlendAlpha:
    return D3D12_BLEND_BLEND_FACTOR;
  case BlendFactor::OneMinusBlendAlpha:
    return D3D12_BLEND_INV_BLEND_FACTOR;
  case BlendFactor::Src1Color:
    return D3D12_BLEND_SRC1_COLOR;
  case BlendFactor::OneMinusSrc1Color:
    return D3D12_BLEND_INV_SRC1_COLOR;
  case BlendFactor::Src1Alpha:
    return D3D12_BLEND_SRC1_ALPHA;
  case BlendFactor::OneMinusSrc1Alpha:
    return D3D12_BLEND_INV_SRC1_ALPHA;
  default:
    return D3D12_BLEND_ONE;
  }
}

// Helper to convert IGL blend operation to D3D12
D3D12_BLEND_OP toD3D12BlendOp(BlendOp op) {
  switch (op) {
  case BlendOp::Add:
    return D3D12_BLEND_OP_ADD;
  case BlendOp::Subtract:
    return D3D12_BLEND_OP_SUBTRACT;
  case BlendOp::ReverseSubtract:
    return D3D12_BLEND_OP_REV_SUBTRACT;
  case BlendOp::Min:
    return D3D12_BLEND_OP_MIN;
  case BlendOp::Max:
    return D3D12_BLEND_OP_MAX;
  default:
    return D3D12_BLEND_OP_ADD;
  }
}

} // anonymous namespace

//=============================================================================
// D3D12GraphicsPipelineBuilder
//=============================================================================

D3D12GraphicsPipelineBuilder::D3D12GraphicsPipelineBuilder() {
  // Zero-initialize the descriptor
  psoDesc_ = {};

  // Set sensible defaults for rasterizer state
  psoDesc_.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  psoDesc_.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
  psoDesc_.RasterizerState.FrontCounterClockwise = FALSE;
  psoDesc_.RasterizerState.DepthBias = 0;
  psoDesc_.RasterizerState.DepthBiasClamp = 0.0f;
  psoDesc_.RasterizerState.SlopeScaledDepthBias = 0.0f;
  psoDesc_.RasterizerState.DepthClipEnable = TRUE;
  psoDesc_.RasterizerState.MultisampleEnable = FALSE;
  psoDesc_.RasterizerState.AntialiasedLineEnable = FALSE;
  psoDesc_.RasterizerState.ForcedSampleCount = 0;
  psoDesc_.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

  // Set sensible defaults for blend state
  psoDesc_.BlendState.AlphaToCoverageEnable = FALSE;
  psoDesc_.BlendState.IndependentBlendEnable = FALSE;
  for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
    psoDesc_.BlendState.RenderTarget[i].BlendEnable = FALSE;
    psoDesc_.BlendState.RenderTarget[i].LogicOpEnable = FALSE;
    psoDesc_.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
    psoDesc_.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
    psoDesc_.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc_.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc_.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
    psoDesc_.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc_.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
    psoDesc_.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  }

  // Set sensible defaults for depth-stencil state
  psoDesc_.DepthStencilState.DepthEnable = FALSE;
  psoDesc_.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
  psoDesc_.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  psoDesc_.DepthStencilState.StencilEnable = FALSE;
  psoDesc_.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
  psoDesc_.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
  psoDesc_.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
  psoDesc_.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
  psoDesc_.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
  psoDesc_.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  psoDesc_.DepthStencilState.BackFace = psoDesc_.DepthStencilState.FrontFace;

  // Defaults for other fields
  psoDesc_.SampleMask = UINT_MAX;
  psoDesc_.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc_.NumRenderTargets = 1;
  psoDesc_.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  psoDesc_.DSVFormat = DXGI_FORMAT_UNKNOWN;
  psoDesc_.SampleDesc.Count = 1;
  psoDesc_.SampleDesc.Quality = 0;
  psoDesc_.NodeMask = 0;
  psoDesc_.CachedPSO.pCachedBlob = nullptr;
  psoDesc_.CachedPSO.CachedBlobSizeInBytes = 0;
  psoDesc_.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::vertexShader(
    const std::vector<uint8_t>& bytecode) {
  vsBytecode_ = bytecode;
  psoDesc_.VS = {vsBytecode_.data(), vsBytecode_.size()};
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::pixelShader(
    const std::vector<uint8_t>& bytecode) {
  psBytecode_ = bytecode;
  psoDesc_.PS = {psBytecode_.data(), psBytecode_.size()};
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::shaderBytecode(
    const std::vector<uint8_t>& vs,
    const std::vector<uint8_t>& ps) {
  return vertexShader(vs).pixelShader(ps);
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::vertexInputLayout(
    const std::vector<D3D12_INPUT_ELEMENT_DESC>& elements) {
  inputElements_ = elements;
  psoDesc_.InputLayout = {inputElements_.data(), static_cast<UINT>(inputElements_.size())};
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::blendState(
    const D3D12_BLEND_DESC& desc) {
  psoDesc_.BlendState = desc;
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::blendStateForAttachment(
    UINT attachmentIndex,
    const RenderPipelineDesc::TargetDesc::ColorAttachment& attachment) {
  if (attachmentIndex >= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT) {
    return *this;
  }

  // Enable independent blending when configuring attachments beyond RT0
  if (attachmentIndex > 0) {
    psoDesc_.BlendState.IndependentBlendEnable = TRUE;
  }

  auto& rt = psoDesc_.BlendState.RenderTarget[attachmentIndex];
  rt.BlendEnable = attachment.blendEnabled ? TRUE : FALSE;
  rt.SrcBlend = toD3D12Blend(attachment.srcRGBBlendFactor);
  rt.DestBlend = toD3D12Blend(attachment.dstRGBBlendFactor);
  rt.BlendOp = toD3D12BlendOp(attachment.rgbBlendOp);
  rt.SrcBlendAlpha = toD3D12Blend(attachment.srcAlphaBlendFactor);
  rt.DestBlendAlpha = toD3D12Blend(attachment.dstAlphaBlendFactor);
  rt.BlendOpAlpha = toD3D12BlendOp(attachment.alphaBlendOp);

  // Convert IGL color write mask to D3D12
  UINT8 writeMask = 0;
  if (attachment.colorWriteMask & igl::kColorWriteBitsRed) {
    writeMask |= D3D12_COLOR_WRITE_ENABLE_RED;
  }
  if (attachment.colorWriteMask & igl::kColorWriteBitsGreen) {
    writeMask |= D3D12_COLOR_WRITE_ENABLE_GREEN;
  }
  if (attachment.colorWriteMask & igl::kColorWriteBitsBlue) {
    writeMask |= D3D12_COLOR_WRITE_ENABLE_BLUE;
  }
  if (attachment.colorWriteMask & igl::kColorWriteBitsAlpha) {
    writeMask |= D3D12_COLOR_WRITE_ENABLE_ALPHA;
  }
  rt.RenderTargetWriteMask = writeMask;

  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::rasterizerState(
    const D3D12_RASTERIZER_DESC& desc) {
  psoDesc_.RasterizerState = desc;
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::cullMode(CullMode mode) {
  switch (mode) {
  case CullMode::Back:
    psoDesc_.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    break;
  case CullMode::Front:
    psoDesc_.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
    break;
  case CullMode::Disabled:
  default:
    psoDesc_.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    break;
  }
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::frontFaceWinding(WindingMode mode) {
  psoDesc_.RasterizerState.FrontCounterClockwise = (mode == WindingMode::CounterClockwise) ? TRUE
                                                                                           : FALSE;
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::polygonFillMode(PolygonFillMode mode) {
  psoDesc_.RasterizerState.FillMode = (mode == PolygonFillMode::Line) ? D3D12_FILL_MODE_WIREFRAME
                                                                      : D3D12_FILL_MODE_SOLID;
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::depthStencilState(
    const D3D12_DEPTH_STENCIL_DESC& desc) {
  psoDesc_.DepthStencilState = desc;
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::depthTestEnabled(bool enabled) {
  psoDesc_.DepthStencilState.DepthEnable = enabled ? TRUE : FALSE;
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::depthWriteEnabled(bool enabled) {
  psoDesc_.DepthStencilState.DepthWriteMask = enabled ? D3D12_DEPTH_WRITE_MASK_ALL
                                                      : D3D12_DEPTH_WRITE_MASK_ZERO;
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::depthCompareFunc(
    D3D12_COMPARISON_FUNC func) {
  psoDesc_.DepthStencilState.DepthFunc = func;
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::renderTargetFormat(UINT index,
                                                                               DXGI_FORMAT format) {
  if (index < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT) {
    psoDesc_.RTVFormats[index] = format;
    // Auto-update NumRenderTargets to include this slot
    if (index + 1 > psoDesc_.NumRenderTargets) {
      psoDesc_.NumRenderTargets = index + 1;
      // Enable independent blending when using multiple render targets
      psoDesc_.BlendState.IndependentBlendEnable = (psoDesc_.NumRenderTargets > 1) ? TRUE : FALSE;
    }
  }
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::renderTargetFormats(
    const std::vector<DXGI_FORMAT>& formats) {
  const UINT count =
      static_cast<UINT>(std::min<size_t>(formats.size(), D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT));
  psoDesc_.NumRenderTargets = count;
  // Enable independent blending when using multiple render targets
  psoDesc_.BlendState.IndependentBlendEnable = (count > 1) ? TRUE : FALSE;
  for (UINT i = 0; i < count; ++i) {
    psoDesc_.RTVFormats[i] = formats[i];
  }
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::depthStencilFormat(DXGI_FORMAT format) {
  psoDesc_.DSVFormat = format;
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::numRenderTargets(UINT count) {
  const UINT clamped = std::min(count, static_cast<UINT>(D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT));
  psoDesc_.NumRenderTargets = clamped;
  // Enable independent blending when using multiple render targets
  psoDesc_.BlendState.IndependentBlendEnable = (clamped > 1) ? TRUE : FALSE;
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::sampleCount(UINT count) {
  psoDesc_.SampleDesc.Count = count;
  psoDesc_.RasterizerState.MultisampleEnable = (count > 1) ? TRUE : FALSE;
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::sampleMask(UINT mask) {
  psoDesc_.SampleMask = mask;
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::primitiveTopologyType(
    D3D12_PRIMITIVE_TOPOLOGY_TYPE type) {
  psoDesc_.PrimitiveTopologyType = type;
  return *this;
}

D3D12GraphicsPipelineBuilder& D3D12GraphicsPipelineBuilder::streamOutput(
    const D3D12_STREAM_OUTPUT_DESC& desc) {
  psoDesc_.StreamOutput = desc;
  return *this;
}

Result D3D12GraphicsPipelineBuilder::build(ID3D12Device* device,
                                           ID3D12RootSignature* rootSignature,
                                           ID3D12PipelineState** outPipelineState,
                                           const char* debugName) {
  if (!device) {
    return Result(Result::Code::ArgumentNull, "Device is null");
  }
  if (!rootSignature) {
    return Result(Result::Code::ArgumentNull, "Root signature is null");
  }
  if (!outPipelineState) {
    return Result(Result::Code::ArgumentNull, "Output pipeline state is null");
  }

  // Initialize output to null for safety
  *outPipelineState = nullptr;

  // Validate shader bytecode
  if (psoDesc_.VS.BytecodeLength == 0) {
    return Result(Result::Code::ArgumentInvalid, "Vertex shader bytecode is required");
  }
  if (psoDesc_.PS.BytecodeLength == 0) {
    return Result(Result::Code::ArgumentInvalid, "Pixel shader bytecode is required");
  }

  // Set root signature
  psoDesc_.pRootSignature = rootSignature;

  // Create pipeline state
  igl::d3d12::ComPtr<ID3D12PipelineState> pipelineState;
  HRESULT hr =
      device->CreateGraphicsPipelineState(&psoDesc_, IID_PPV_ARGS(pipelineState.GetAddressOf()));
  if (FAILED(hr)) {
    char errorMsg[512];
    snprintf(errorMsg,
             sizeof(errorMsg),
             "Failed to create graphics pipeline state. HRESULT: 0x%08X",
             static_cast<unsigned int>(hr));
    return Result(Result::Code::RuntimeError, errorMsg);
  }

  // Set debug name if provided
  if (debugName && debugName[0] != '\0') {
    std::wstring wideName(debugName, debugName + strlen(debugName));
    pipelineState->SetName(wideName.c_str());
  }

  *outPipelineState = pipelineState.Get();
  pipelineState->AddRef(); // Transfer ownership
  return Result();
}

//=============================================================================
// D3D12ComputePipelineBuilder
//=============================================================================

D3D12ComputePipelineBuilder::D3D12ComputePipelineBuilder() {
  // Zero-initialize the descriptor
  psoDesc_ = {};
  psoDesc_.NodeMask = 0;
  psoDesc_.CachedPSO.pCachedBlob = nullptr;
  psoDesc_.CachedPSO.CachedBlobSizeInBytes = 0;
  psoDesc_.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
}

D3D12ComputePipelineBuilder& D3D12ComputePipelineBuilder::shaderBytecode(
    const std::vector<uint8_t>& bytecode) {
  csBytecode_ = bytecode;
  psoDesc_.CS.pShaderBytecode = csBytecode_.data();
  psoDesc_.CS.BytecodeLength = csBytecode_.size();
  return *this;
}

Result D3D12ComputePipelineBuilder::build(ID3D12Device* device,
                                          ID3D12RootSignature* rootSignature,
                                          ID3D12PipelineState** outPipelineState,
                                          const char* debugName) {
  if (!device) {
    return Result(Result::Code::ArgumentNull, "Device is null");
  }
  if (!rootSignature) {
    return Result(Result::Code::ArgumentNull, "Root signature is null");
  }
  if (!outPipelineState) {
    return Result(Result::Code::ArgumentNull, "Output pipeline state is null");
  }

  // Initialize output to null for safety
  *outPipelineState = nullptr;

  // Validate shader bytecode
  if (psoDesc_.CS.BytecodeLength == 0) {
    return Result(Result::Code::ArgumentInvalid, "Compute shader bytecode is required");
  }

  // Set root signature
  psoDesc_.pRootSignature = rootSignature;

  // Create pipeline state
  igl::d3d12::ComPtr<ID3D12PipelineState> pipelineState;
  HRESULT hr =
      device->CreateComputePipelineState(&psoDesc_, IID_PPV_ARGS(pipelineState.GetAddressOf()));
  if (FAILED(hr)) {
    char errorMsg[512];
    snprintf(errorMsg,
             sizeof(errorMsg),
             "Failed to create compute pipeline state. HRESULT: 0x%08X",
             static_cast<unsigned int>(hr));
    return Result(Result::Code::RuntimeError, errorMsg);
  }

  // Set debug name if provided
  if (debugName && debugName[0] != '\0') {
    std::wstring wideName(debugName, debugName + strlen(debugName));
    pipelineState->SetName(wideName.c_str());
  }

  *outPipelineState = pipelineState.Get();
  pipelineState->AddRef(); // Transfer ownership
  return Result();
}

//=============================================================================
// D3D12RootSignatureBuilder
//=============================================================================

D3D12RootSignatureBuilder::D3D12RootSignatureBuilder() {
  flags_ = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
}

D3D12RootSignatureBuilder& D3D12RootSignatureBuilder::addRootConstants(UINT shaderRegister,
                                                                       UINT num32BitValues,
                                                                       UINT registerSpace) {
  RootParameter param{}; // Zero-initialize
  param.param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  param.param.Constants.ShaderRegister = shaderRegister;
  param.param.Constants.RegisterSpace = registerSpace;
  param.param.Constants.Num32BitValues = num32BitValues;
  param.param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  rootParameters_.push_back(param);
  return *this;
}

D3D12RootSignatureBuilder& D3D12RootSignatureBuilder::addRootCBV(UINT shaderRegister,
                                                                 UINT registerSpace) {
  RootParameter param{}; // Zero-initialize
  param.param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  param.param.Descriptor.ShaderRegister = shaderRegister;
  param.param.Descriptor.RegisterSpace = registerSpace;
  param.param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  rootParameters_.push_back(param);
  return *this;
}

D3D12RootSignatureBuilder& D3D12RootSignatureBuilder::addRootSRV(UINT shaderRegister,
                                                                 UINT registerSpace) {
  RootParameter param{}; // Zero-initialize
  param.param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
  param.param.Descriptor.ShaderRegister = shaderRegister;
  param.param.Descriptor.RegisterSpace = registerSpace;
  param.param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  rootParameters_.push_back(param);
  return *this;
}

D3D12RootSignatureBuilder& D3D12RootSignatureBuilder::addRootUAV(UINT shaderRegister,
                                                                 UINT registerSpace) {
  RootParameter param{}; // Zero-initialize
  param.param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
  param.param.Descriptor.ShaderRegister = shaderRegister;
  param.param.Descriptor.RegisterSpace = registerSpace;
  param.param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  rootParameters_.push_back(param);
  return *this;
}

D3D12RootSignatureBuilder& D3D12RootSignatureBuilder::addDescriptorTable(
    D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
    UINT numDescriptors,
    UINT baseShaderRegister,
    UINT registerSpace) {
  RootParameter param{}; // Zero-initialize
  param.param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Create descriptor range
  DescriptorRange range;
  range.range.RangeType = rangeType;
  range.range.NumDescriptors = numDescriptors;
  range.range.BaseShaderRegister = baseShaderRegister;
  range.range.RegisterSpace = registerSpace;
  range.range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  param.ranges.push_back(range);
  rootParameters_.push_back(param);
  return *this;
}

D3D12RootSignatureBuilder& D3D12RootSignatureBuilder::flags(D3D12_ROOT_SIGNATURE_FLAGS flags) {
  flags_ = flags;
  return *this;
}

Result D3D12RootSignatureBuilder::build(ID3D12Device* device,
                                        const D3D12Context* context,
                                        ID3D12RootSignature** outRootSignature) {
  if (!device) {
    return Result(Result::Code::ArgumentNull, "Device is null");
  }
  if (!outRootSignature) {
    return Result(Result::Code::ArgumentNull, "Output root signature is null");
  }

  // Initialize output to null for safety
  *outRootSignature = nullptr;

  // Build arrays of D3D12_ROOT_PARAMETER and descriptor ranges
  std::vector<D3D12_ROOT_PARAMETER> d3d12Params;
  std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> allRanges;

  d3d12Params.reserve(rootParameters_.size());
  allRanges.reserve(rootParameters_.size());

  for (auto& param : rootParameters_) {
    if (param.param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
      // Store ranges for this table
      std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
      ranges.reserve(param.ranges.size());
      for (auto& r : param.ranges) {
        D3D12_DESCRIPTOR_RANGE range = r.range;
        if (context) {
          const UINT maxCount = getMaxDescriptorCount(context, range.RangeType);
          if (range.NumDescriptors == UINT_MAX || range.NumDescriptors > maxCount) {
            range.NumDescriptors = maxCount;
          }
        }
        ranges.push_back(range);
      }
      allRanges.push_back(std::move(ranges));

      // Update descriptor table to point to the ranges
      D3D12_ROOT_PARAMETER d3d12Param = param.param;
      d3d12Param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(allRanges.back().size());
      d3d12Param.DescriptorTable.pDescriptorRanges = allRanges.back().data();
      d3d12Params.push_back(d3d12Param);
    } else {
      // Not a descriptor table, just copy
      d3d12Params.push_back(param.param);
    }
  }

  // Build root signature descriptor
  D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
  rootSigDesc.NumParameters = static_cast<UINT>(d3d12Params.size());
  rootSigDesc.pParameters = d3d12Params.data();
  rootSigDesc.NumStaticSamplers = 0;
  rootSigDesc.pStaticSamplers = nullptr;
  rootSigDesc.Flags = flags_;

  // Validate size (64 DWORD limit)
  const uint32_t size = getRootSignatureDwordSize(rootSigDesc);
  if (size > 64) {
    char errorMsg[256];
    snprintf(
        errorMsg, sizeof(errorMsg), "Root signature size exceeds 64 DWORD limit: %u DWORDs", size);
    return Result(Result::Code::ArgumentOutOfRange, errorMsg);
  }

  // Serialize root signature
  igl::d3d12::ComPtr<ID3DBlob> signature;
  igl::d3d12::ComPtr<ID3DBlob> error;
  HRESULT hr = D3D12SerializeRootSignature(
      &rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, signature.GetAddressOf(), error.GetAddressOf());
  if (FAILED(hr)) {
    const char* errorStr = error.Get() ? static_cast<const char*>(error->GetBufferPointer())
                                       : "Unknown error";
    char errorMsg[512];
    snprintf(errorMsg,
             sizeof(errorMsg),
             "Failed to serialize root signature. HRESULT: 0x%08X, Error: %s",
             static_cast<unsigned int>(hr),
             errorStr);
    return Result(Result::Code::RuntimeError, errorMsg);
  }

  // Create root signature
  igl::d3d12::ComPtr<ID3D12RootSignature> rootSignature;
  hr = device->CreateRootSignature(0,
                                   signature->GetBufferPointer(),
                                   signature->GetBufferSize(),
                                   IID_PPV_ARGS(rootSignature.GetAddressOf()));
  if (FAILED(hr)) {
    char errorMsg[256];
    snprintf(errorMsg,
             sizeof(errorMsg),
             "Failed to create root signature. HRESULT: 0x%08X",
             static_cast<unsigned int>(hr));
    return Result(Result::Code::RuntimeError, errorMsg);
  }

  *outRootSignature = rootSignature.Get();
  rootSignature->AddRef(); // Transfer ownership
  return Result();
}

UINT D3D12RootSignatureBuilder::getMaxDescriptorCount(const D3D12Context* context,
                                                      D3D12_DESCRIPTOR_RANGE_TYPE rangeType) {
  if (!context) {
    return 128; // Conservative default
  }

  const D3D12_RESOURCE_BINDING_TIER bindingTier = context->getResourceBindingTier();
  const bool needsBoundedRanges = (bindingTier == D3D12_RESOURCE_BINDING_TIER_1);

  if (!needsBoundedRanges) {
    return UINT_MAX; // Unbounded
  }

  // Conservative bounds for Tier 1 devices
  switch (rangeType) {
  case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
    return 128;
  case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
    return 64;
  case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
    return 64;
  case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
    return 32;
  default:
    return 128;
  }
}

uint32_t D3D12RootSignatureBuilder::getDwordSize() const {
  // Build temporary descriptor for cost calculation
  std::vector<D3D12_ROOT_PARAMETER> d3d12Params;
  std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> allRanges;

  d3d12Params.reserve(rootParameters_.size());
  allRanges.reserve(rootParameters_.size());

  for (const auto& param : rootParameters_) {
    if (param.param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
      std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
      ranges.reserve(param.ranges.size());
      for (const auto& r : param.ranges) {
        ranges.push_back(r.range);
      }
      allRanges.push_back(std::move(ranges));

      D3D12_ROOT_PARAMETER d3d12Param = param.param;
      d3d12Param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(allRanges.back().size());
      d3d12Param.DescriptorTable.pDescriptorRanges = allRanges.back().data();
      d3d12Params.push_back(d3d12Param);
    } else {
      d3d12Params.push_back(param.param);
    }
  }

  D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
  rootSigDesc.NumParameters = static_cast<UINT>(d3d12Params.size());
  rootSigDesc.pParameters = d3d12Params.data();
  rootSigDesc.NumStaticSamplers = 0;
  rootSigDesc.pStaticSamplers = nullptr;
  rootSigDesc.Flags = flags_;

  return getRootSignatureDwordSize(rootSigDesc);
}

} // namespace igl::d3d12
