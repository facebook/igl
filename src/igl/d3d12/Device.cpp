/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/Device.h>
#include <igl/d3d12/CommandQueue.h>
#include <igl/d3d12/Buffer.h>
#include <igl/d3d12/RenderPipelineState.h>
#include <igl/d3d12/ShaderModule.h>
#include <igl/d3d12/Framebuffer.h>
#include <igl/VertexInputState.h>
#include <cstring>
#include <vector>

namespace igl::d3d12 {

Device::Device(std::unique_ptr<D3D12Context> ctx) : ctx_(std::move(ctx)) {}

// BindGroups
Holder<BindGroupTextureHandle> Device::createBindGroup(
    const BindGroupTextureDesc& /*desc*/,
    const IRenderPipelineState* IGL_NULLABLE /*compatiblePipeline*/,
    Result* IGL_NULLABLE outResult) {
  Result::setResult(outResult, Result::Code::Unimplemented, "D3D12 Device not yet implemented");
  return {};
}

Holder<BindGroupBufferHandle> Device::createBindGroup(const BindGroupBufferDesc& /*desc*/,
                                                       Result* IGL_NULLABLE outResult) {
  Result::setResult(outResult, Result::Code::Unimplemented, "D3D12 Device not yet implemented");
  return {};
}

void Device::destroy(BindGroupTextureHandle /*handle*/) {
  // Stub: Not yet implemented
}

void Device::destroy(BindGroupBufferHandle /*handle*/) {
  // Stub: Not yet implemented
}

void Device::destroy(SamplerHandle /*handle*/) {
  // Stub: Not yet implemented
}

// Command Queue
std::shared_ptr<ICommandQueue> Device::createCommandQueue(const CommandQueueDesc& /*desc*/,
                                                           Result* IGL_NULLABLE
                                                               outResult) noexcept {
  Result::setOk(outResult);
  return std::make_shared<CommandQueue>(*this);
}

// Resources
std::unique_ptr<IBuffer> Device::createBuffer(const BufferDesc& desc,
                                              Result* IGL_NULLABLE outResult) const noexcept {
  auto* device = ctx_->getDevice();
  if (!device) {
    Result::setResult(outResult, Result::Code::RuntimeError, "D3D12 device is null");
    return nullptr;
  }

  // Determine heap type and initial state based on storage
  D3D12_HEAP_TYPE heapType;
  D3D12_RESOURCE_STATES initialState;

  if (desc.storage == ResourceStorage::Shared || desc.storage == ResourceStorage::Managed) {
    // CPU-writable upload heap
    heapType = D3D12_HEAP_TYPE_UPLOAD;
    initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
  } else {
    // GPU-only default heap
    heapType = D3D12_HEAP_TYPE_DEFAULT;
    initialState = D3D12_RESOURCE_STATE_COMMON;
  }

  // Create heap properties
  D3D12_HEAP_PROPERTIES heapProps = {};
  heapProps.Type = heapType;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

  // Create buffer description
  D3D12_RESOURCE_DESC bufferDesc = {};
  bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufferDesc.Alignment = 0;
  bufferDesc.Width = desc.length;
  bufferDesc.Height = 1;
  bufferDesc.DepthOrArraySize = 1;
  bufferDesc.MipLevels = 1;
  bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
  bufferDesc.SampleDesc.Count = 1;
  bufferDesc.SampleDesc.Quality = 0;
  bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  // Create the buffer resource
  Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
  HRESULT hr = device->CreateCommittedResource(
      &heapProps,
      D3D12_HEAP_FLAG_NONE,
      &bufferDesc,
      initialState,
      nullptr,
      IID_PPV_ARGS(buffer.GetAddressOf())
  );

  if (FAILED(hr)) {
    char errorMsg[256];
    snprintf(errorMsg, sizeof(errorMsg), "Failed to create buffer: HRESULT = 0x%08X", static_cast<unsigned>(hr));
    Result::setResult(outResult, Result::Code::RuntimeError, errorMsg);
    return nullptr;
  }

  // Upload initial data if provided
  if (desc.data && heapType == D3D12_HEAP_TYPE_UPLOAD) {
    void* mappedData = nullptr;
    D3D12_RANGE readRange = {0, 0};
    hr = buffer->Map(0, &readRange, &mappedData);

    if (SUCCEEDED(hr)) {
      std::memcpy(mappedData, desc.data, desc.length);
      buffer->Unmap(0, nullptr);
    }
  }

  Result::setOk(outResult);
  return std::make_unique<Buffer>(std::move(buffer), desc);
}

std::shared_ptr<IDepthStencilState> Device::createDepthStencilState(
    const DepthStencilStateDesc& /*desc*/,
    Result* IGL_NULLABLE outResult) const {
  Result::setResult(outResult, Result::Code::Unimplemented, "D3D12 DepthStencilState not yet implemented");
  return nullptr;
}

std::unique_ptr<IShaderStages> Device::createShaderStages(const ShaderStagesDesc& desc,
                                                          Result* IGL_NULLABLE
                                                              outResult) const {
  Result::setOk(outResult);
  return std::make_unique<ShaderStages>(desc);
}

std::shared_ptr<ISamplerState> Device::createSamplerState(const SamplerStateDesc& /*desc*/,
                                                          Result* IGL_NULLABLE outResult) const {
  Result::setResult(outResult, Result::Code::Unimplemented, "D3D12 SamplerState not yet implemented");
  return nullptr;
}

std::shared_ptr<ITexture> Device::createTexture(const TextureDesc& /*desc*/,
                                                Result* IGL_NULLABLE outResult) const noexcept {
  Result::setResult(outResult, Result::Code::Unimplemented, "D3D12 Texture not yet implemented");
  return nullptr;
}

std::shared_ptr<ITexture> Device::createTextureView(std::shared_ptr<ITexture> /*texture*/,
                                                    const TextureViewDesc& /*desc*/,
                                                    Result* IGL_NULLABLE
                                                        outResult) const noexcept {
  Result::setResult(outResult, Result::Code::Unimplemented, "D3D12 TextureView not yet implemented");
  return nullptr;
}

std::shared_ptr<ITimer> Device::createTimer(Result* IGL_NULLABLE outResult) const noexcept {
  Result::setResult(outResult, Result::Code::Unimplemented, "D3D12 Timer not yet implemented");
  return nullptr;
}

std::shared_ptr<IVertexInputState> Device::createVertexInputState(
    const VertexInputStateDesc& /*desc*/,
    Result* IGL_NULLABLE outResult) const {
  Result::setResult(outResult, Result::Code::Unimplemented, "D3D12 VertexInputState not yet implemented");
  return nullptr;
}

// Pipelines
std::shared_ptr<IComputePipelineState> Device::createComputePipeline(
    const ComputePipelineDesc& /*desc*/,
    Result* IGL_NULLABLE outResult) const {
  Result::setResult(outResult, Result::Code::Unimplemented, "D3D12 ComputePipeline not yet implemented");
  return nullptr;
}

std::shared_ptr<IRenderPipelineState> Device::createRenderPipeline(
    const RenderPipelineDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  auto* device = ctx_->getDevice();
  if (!device) {
    Result::setResult(outResult, Result::Code::InvalidOperation, "D3D12 device is null");
    return nullptr;
  }

  if (!desc.shaderStages) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Shader stages are required");
    return nullptr;
  }

  // Get shader modules
  auto* vertexModule = static_cast<const ShaderModule*>(desc.shaderStages->getVertexModule().get());
  auto* fragmentModule = static_cast<const ShaderModule*>(desc.shaderStages->getFragmentModule().get());

  if (!vertexModule || !fragmentModule) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Vertex and fragment shaders required");
    return nullptr;
  }

  // Create root signature (empty for now - Phase 3 Step 3.3)
  D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
  rootSigDesc.NumParameters = 0;
  rootSigDesc.pParameters = nullptr;
  rootSigDesc.NumStaticSamplers = 0;
  rootSigDesc.pStaticSamplers = nullptr;
  rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  Microsoft::WRL::ComPtr<ID3DBlob> signature;
  Microsoft::WRL::ComPtr<ID3DBlob> error;
  HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                            signature.GetAddressOf(), error.GetAddressOf());
  if (FAILED(hr)) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Failed to serialize root signature");
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
  hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                    IID_PPV_ARGS(rootSignature.GetAddressOf()));
  if (FAILED(hr)) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Failed to create root signature");
    return nullptr;
  }

  // Create PSO
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.pRootSignature = rootSignature.Get();

  // Shader bytecode
  const auto& vsBytecode = vertexModule->getBytecode();
  const auto& psBytecode = fragmentModule->getBytecode();
  psoDesc.VS = {vsBytecode.data(), vsBytecode.size()};
  psoDesc.PS = {psBytecode.data(), psBytecode.size()};

  // Blend state
  psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
  psoDesc.BlendState.IndependentBlendEnable = FALSE;
  psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
  psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  // Rasterizer state
  psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
  psoDesc.RasterizerState.DepthBias = 0;
  psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
  psoDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
  psoDesc.RasterizerState.DepthClipEnable = TRUE;
  psoDesc.RasterizerState.MultisampleEnable = FALSE;
  psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
  psoDesc.RasterizerState.ForcedSampleCount = 0;
  psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

  // Depth stencil state
  psoDesc.DepthStencilState.DepthEnable = FALSE;
  psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
  psoDesc.DepthStencilState.StencilEnable = FALSE;
  psoDesc.DepthStencilState.StencilReadMask = 0xFF;
  psoDesc.DepthStencilState.StencilWriteMask = 0xFF;

  // Render target format
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

  // Sample settings
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.SampleDesc.Count = 1;
  psoDesc.SampleDesc.Quality = 0;

  // Primitive topology
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

  // Input layout (Phase 3 Step 3.4)
  std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
  if (desc.vertexInputState) {
    // TODO: Convert IGL vertex input state to D3D12 input layout
    // For now, use hardcoded layout for simple triangle
  } else {
    // Default simple triangle layout: position (float3) + color (float4)
    inputElements.resize(2);
    inputElements[0] = {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
    inputElements[1] = {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
                        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
  }
  psoDesc.InputLayout = {inputElements.data(), static_cast<UINT>(inputElements.size())};

  Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
  hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf()));
  if (FAILED(hr)) {
    char errorMsg[256];
    snprintf(errorMsg, sizeof(errorMsg), "Failed to create pipeline state. HRESULT: 0x%08X", static_cast<unsigned>(hr));
    IGL_LOG_ERROR(errorMsg);
    IGL_LOG_ERROR("\n");
    Result::setResult(outResult, Result::Code::RuntimeError, errorMsg);
    return nullptr;
  }

  Result::setOk(outResult);
  return std::make_shared<RenderPipelineState>(desc, std::move(pipelineState), std::move(rootSignature));
}

// Shader library and modules
std::unique_ptr<IShaderLibrary> Device::createShaderLibrary(const ShaderLibraryDesc& /*desc*/,
                                                            Result* IGL_NULLABLE
                                                                outResult) const {
  Result::setResult(outResult, Result::Code::Unimplemented, "D3D12 ShaderLibrary not yet implemented");
  return nullptr;
}

std::shared_ptr<IShaderModule> Device::createShaderModule(const ShaderModuleDesc& desc,
                                                          Result* IGL_NULLABLE outResult) const {
  if (!desc.input.isValid()) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Invalid shader input");
    return nullptr;
  }

  // D3D12 requires binary shader input (DXIL bytecode)
  if (desc.input.type != ShaderInputType::Binary) {
    Result::setResult(outResult, Result::Code::Unsupported,
                      "D3D12 requires binary shader input (DXIL). Use ShaderInputType::Binary");
    return nullptr;
  }

  // Copy bytecode
  std::vector<uint8_t> bytecode(desc.input.length);
  std::memcpy(bytecode.data(), desc.input.data, desc.input.length);

  Result::setOk(outResult);
  return std::make_shared<ShaderModule>(desc.info, std::move(bytecode));
}

// Framebuffer
std::shared_ptr<IFramebuffer> Device::createFramebuffer(const FramebufferDesc& desc,
                                                        Result* IGL_NULLABLE outResult) {
  Result::setOk(outResult);
  return std::make_shared<Framebuffer>(desc);
}

// Capabilities
const IPlatformDevice& Device::getPlatformDevice() const noexcept {
  // TODO: Implement proper D3D12 platform device
  class D3D12PlatformDevice : public IPlatformDevice {
  public:
    bool isType(PlatformDeviceType /*t*/) const noexcept override { return false; }
  };
  static D3D12PlatformDevice platformDevice;
  return platformDevice;
}

bool Device::hasFeature(DeviceFeatures /*feature*/) const {
  return false;
}

bool Device::hasRequirement(DeviceRequirement /*requirement*/) const {
  return false;
}

bool Device::getFeatureLimits(DeviceFeatureLimits /*featureLimits*/, size_t& result) const {
  result = 0;
  return false;
}

ICapabilities::TextureFormatCapabilities Device::getTextureFormatCapabilities(TextureFormat /*format*/) const {
  return ICapabilities::TextureFormatCapabilities{};
}

ShaderVersion Device::getShaderVersion() const {
  // HLSL Shader Model 6.0
  return ShaderVersion{ShaderFamily::Hlsl, 6, 0, 0};
}

BackendVersion Device::getBackendVersion() const {
  return BackendVersion{BackendFlavor::D3D12, 12, 0};
}

BackendType Device::getBackendType() const {
  return BackendType::D3D12;
}

size_t Device::getCurrentDrawCount() const {
  return drawCount_;
}

size_t Device::getShaderCompilationCount() const {
  return shaderCompilationCount_;
}

} // namespace igl::d3d12
