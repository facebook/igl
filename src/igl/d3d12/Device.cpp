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
#include <igl/d3d12/ComputePipelineState.h>
#include <igl/d3d12/ShaderModule.h>
#include <igl/d3d12/Framebuffer.h>
#include <igl/d3d12/VertexInputState.h>
#include <igl/d3d12/DepthStencilState.h>
#include <igl/d3d12/SamplerState.h>
#include <igl/d3d12/Texture.h>
#include <igl/d3d12/PlatformDevice.h>
#include <igl/d3d12/DXCCompiler.h>
#include <igl/d3d12/Timer.h>
#include <igl/d3d12/UploadRingBuffer.h>
#include <igl/d3d12/D3D12ImmediateCommands.h>
#include <igl/d3d12/D3D12StagingDevice.h>
#include <igl/d3d12/D3D12FenceWaiter.h>
#include <igl/VertexInputState.h>
#include <igl/Texture.h>
#include <igl/Assert.h>  // For IGL_DEBUG_ASSERT in waitForUploadFence.
#include <d3dcompiler.h>
#include <cstring>
#include <vector>
#include <mutex>   // For std::call_once.

#pragma comment(lib, "d3dcompiler.lib")

namespace igl::d3d12 {

namespace {
// Import ComPtr for readability
template<typename T>
using ComPtr = igl::d3d12::ComPtr<T>;

// Use std::hash<SamplerStateDesc> for deduplication (implemented in igl/SamplerState.cpp).
} // namespace

// Helper: Calculate root signature cost in DWORDs
// Root signature limit: 64 DWORDs
// Cost formula (per Microsoft documentation):
//   - Root constants: 1 DWORD per 32-bit value
//   - Root descriptors (CBV/SRV/UAV): 2 DWORDs each
//   - Descriptor tables: 1 DWORD each (regardless of table size)
//   - Static samplers: 0 DWORDs (free)
// Reference: https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signature-limits
static uint32_t calculateRootSignatureCost(const D3D12_ROOT_SIGNATURE_DESC& desc) {
  uint32_t totalCost = 0;

  for (uint32_t i = 0; i < desc.NumParameters; ++i) {
    const auto& param = desc.pParameters[i];

    switch (param.ParameterType) {
      case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
        totalCost += param.Constants.Num32BitValues;
        IGL_D3D12_LOG_VERBOSE("    [%u] Root constants (b%u): %u DWORDs\n",
                     i, param.Constants.ShaderRegister, param.Constants.Num32BitValues);
        break;

      case D3D12_ROOT_PARAMETER_TYPE_CBV:
        totalCost += 2;
        IGL_D3D12_LOG_VERBOSE("    [%u] Root CBV (b%u): 2 DWORDs\n",
                     i, param.Descriptor.ShaderRegister);
        break;

      case D3D12_ROOT_PARAMETER_TYPE_SRV:
        totalCost += 2;
        IGL_D3D12_LOG_VERBOSE("    [%u] Root SRV (t%u): 2 DWORDs\n",
                     i, param.Descriptor.ShaderRegister);
        break;

      case D3D12_ROOT_PARAMETER_TYPE_UAV:
        totalCost += 2;
        IGL_D3D12_LOG_VERBOSE("    [%u] Root UAV (u%u): 2 DWORDs\n",
                     i, param.Descriptor.ShaderRegister);
        break;

      case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
        totalCost += 1;
        const char* tableType = "Unknown";
        if (param.DescriptorTable.NumDescriptorRanges > 0) {
          switch (param.DescriptorTable.pDescriptorRanges[0].RangeType) {
            case D3D12_DESCRIPTOR_RANGE_TYPE_CBV: tableType = "CBV"; break;
            case D3D12_DESCRIPTOR_RANGE_TYPE_SRV: tableType = "SRV"; break;
            case D3D12_DESCRIPTOR_RANGE_TYPE_UAV: tableType = "UAV"; break;
            case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER: tableType = "Sampler"; break;
          }
        }
        IGL_D3D12_LOG_VERBOSE("    [%u] Descriptor table (%s): 1 DWORD\n", i, tableType);
        break;
    }
  }

  if (desc.NumStaticSamplers > 0) {
    IGL_D3D12_LOG_VERBOSE("    Static samplers: 0 DWORDs (free, count=%u)\n", desc.NumStaticSamplers);
  }

  return totalCost;
}

Device::Device(std::unique_ptr<D3D12Context> ctx) : ctx_(std::move(ctx)) {
  platformDevice_ = std::make_unique<PlatformDevice>(*this);

  // Validate device limits against actual device capabilities.
  capabilities_.initialize(*ctx_);

  // Initialize upload infrastructure (allocator pool and upload helpers).
  allocatorPool_.initialize(*ctx_, this);

  auto* device = ctx_->getDevice();
  if (device) {
    // Pre-compile mipmap generation shaders at device initialization.
    // This avoids runtime compilation overhead in Texture::generateMipmap().
    {
      // HLSL shader sources (identical to those in Texture.cpp)
      static const char* kVS = R"(
struct VSOut { float4 pos: SV_POSITION; float2 uv: TEXCOORD0; };
VSOut main(uint id: SV_VertexID) {
  float2 p = float2((id << 1) & 2, id & 2);
  VSOut o; o.pos = float4(p*float2(2,-2)+float2(-1,1), 0, 1); o.uv = p; return o;
}
)";
      static const char* kPS = R"(
Texture2D tex0 : register(t0);
SamplerState smp : register(s0);
float4 main(float4 pos:SV_POSITION, float2 uv:TEXCOORD0) : SV_TARGET { return tex0.SampleLevel(smp, uv, 0); }
)";

      // Initialize DXC compiler
      DXCCompiler dxcCompiler;
      Result initResult = dxcCompiler.initialize();
      if (!initResult.isOk()) {
        IGL_LOG_ERROR("Device::Device: Failed to initialize DXC for mipmap shader compilation: %s\n",
                      initResult.message.c_str());
        IGL_LOG_ERROR("  Mipmap generation will be unavailable\n");
        return;  // Early exit - don't attempt compilation without DXC
      }

      // Get shader model from context (minimum SM 6.0 for DXC)
      D3D_SHADER_MODEL shaderModel = ctx_->getMaxShaderModel();
      std::string vsTarget = getShaderTarget(shaderModel, ShaderStage::Vertex);
      std::string psTarget = getShaderTarget(shaderModel, ShaderStage::Fragment);

      // Compile vertex shader
      std::string vsErrors;
      Result vsResult = dxcCompiler.compile(kVS,
                                            strlen(kVS),
                                            "main",
                                            vsTarget.c_str(),
                                            "MipmapGenerationVS",
                                            0,
                                            pipelineCache_.mipmapVSBytecode_,
                                            vsErrors);
      if (!vsResult.isOk()) {
        IGL_LOG_ERROR("Device::Device: Failed to pre-compile mipmap VS: %s\n%s\n",
                      vsResult.message.c_str(), vsErrors.c_str());
        pipelineCache_.mipmapVSBytecode_.clear();
        return;  // Early exit - can't proceed without VS
      }

      // Compile pixel shader
      std::string psErrors;
      Result psResult = dxcCompiler.compile(kPS,
                                            strlen(kPS),
                                            "main",
                                            psTarget.c_str(),
                                            "MipmapGenerationPS",
                                            0,
                                            pipelineCache_.mipmapPSBytecode_,
                                            psErrors);
      if (!psResult.isOk()) {
        IGL_LOG_ERROR("Device::Device: Failed to pre-compile mipmap PS: %s\n%s\n",
                      psResult.message.c_str(), psErrors.c_str());
        pipelineCache_.mipmapPSBytecode_.clear();
        pipelineCache_.mipmapVSBytecode_.clear();  // Clear VS too for consistency
        return;  // Early exit - can't proceed without PS
      }

      // Create root signature for mipmap generation
      D3D12_DESCRIPTOR_RANGE ranges[2] = {};
      ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
      ranges[0].NumDescriptors = 1;
      ranges[0].BaseShaderRegister = 0;
      ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
      ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
      ranges[1].NumDescriptors = 1;
      ranges[1].BaseShaderRegister = 0;
      ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

      D3D12_ROOT_PARAMETER params[2] = {};
      params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      params[0].DescriptorTable.NumDescriptorRanges = 1;
      params[0].DescriptorTable.pDescriptorRanges = &ranges[0];
      params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
      params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
      params[1].DescriptorTable.NumDescriptorRanges = 1;
      params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
      params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

      D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
      rsDesc.NumParameters = 2;
      rsDesc.pParameters = params;
      rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

      igl::d3d12::ComPtr<ID3DBlob> sig, err;
      if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                             sig.GetAddressOf(), err.GetAddressOf()))) {
        IGL_LOG_ERROR("Device::Device: Failed to serialize mipmap root signature\n");
        if (err && err->GetBufferPointer()) {
          IGL_LOG_ERROR("  D3D12 error: %s\n", static_cast<const char*>(err->GetBufferPointer()));
        }
        pipelineCache_.mipmapVSBytecode_.clear();
        pipelineCache_.mipmapPSBytecode_.clear();
        return;
      }

      if (FAILED(device->CreateRootSignature(0,
                                             sig->GetBufferPointer(),
                                             sig->GetBufferSize(),
                                             IID_PPV_ARGS(
                                                 pipelineCache_.mipmapRootSignature_.GetAddressOf())))) {
        IGL_LOG_ERROR("Device::Device: Failed to create mipmap root signature\n");
        pipelineCache_.mipmapVSBytecode_.clear();
        pipelineCache_.mipmapPSBytecode_.clear();
        return;
      }

      // Success! Mark mipmap shaders as available
      pipelineCache_.mipmapShadersAvailable_ = true;
      IGL_D3D12_LOG_VERBOSE("Device::Device: Mipmap shaders pre-compiled successfully (%zu bytes VS, %zu bytes PS)\n",
                   pipelineCache_.mipmapVSBytecode_.size(),
                   pipelineCache_.mipmapPSBytecode_.size());
    }
  }
}

Device::~Device() {
  // No shared event to clean up; events are per-call in waitForUploadFence.

  // Ensure upload-related resources are released before destroying the device.
  // D3D12Context destructor handles main queue fence waits via waitForGPU().
  pipelineCache_.clear();
  samplerCache_.clear();
  allocatorPool_.clearOnDeviceDestruction();

  // Clear bind group pools to release texture and buffer shared_ptrs that keep resources alive.
  bindGroupTexturesPool_.clear();
  bindGroupBuffersPool_.clear();
}

// Check for device removal and report detailed error.
Result Device::checkDeviceRemoval() const {
  auto* device = ctx_->getDevice();
  if (!device) {
    // Device not initialized is an invalid operation, not success.
    IGL_DEBUG_ASSERT(false, "Device::checkDeviceRemoval() called before device initialization");
    return Result(Result::Code::InvalidOperation, "Device not initialized");
  }

  // Early return if device already marked as lost (return cached reason for diagnostics)
  if (deviceLost_) {
    return Result(Result::Code::RuntimeError,
                  std::string("Device previously lost: ") + deviceLostReason_);
  }

  HRESULT hr = device->GetDeviceRemovedReason();
  if (FAILED(hr)) {
    const char* reason = "Unknown";
    switch (hr) {
      case DXGI_ERROR_DEVICE_HUNG:
        reason = "DEVICE_HUNG (GPU not responding)";
        break;
      case DXGI_ERROR_DEVICE_REMOVED:
        reason = "DEVICE_REMOVED (Driver crash or hardware failure)";
        break;
      case DXGI_ERROR_DEVICE_RESET:
        reason = "DEVICE_RESET (Driver update or TDR)";
        break;
      case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
        reason = "DRIVER_INTERNAL_ERROR (Driver bug)";
        break;
      case DXGI_ERROR_INVALID_CALL:
        reason = "INVALID_CALL (API misuse detected)";
        break;
      default:
        break;
    }

    // Cache the reason and mark device as lost for diagnostics.
    deviceLostReason_ = reason;
    deviceLost_ = true;

    IGL_LOG_ERROR("D3D12 Device Removal Detected: %s (HRESULT=0x%08X)\n", reason, hr);
    IGL_DEBUG_ASSERT(false);
    return Result(Result::Code::RuntimeError, std::string("D3D12 device removed: ") + reason);
  }

  // On success (S_OK), device is healthy
  return Result();
}

// Alignment validation methods.

bool Device::validateMSAAAlignment(const TextureDesc& desc, Result* IGL_NULLABLE outResult) const {
  if (desc.numSamples <= 1) {
    return true;  // Not MSAA, no special alignment requirements
  }

  // MSAA resources require 64KB alignment in D3D12
  // D3D12 CreateCommittedResource automatically handles this, but we validate dimensions
  // to ensure resource won't exceed device limits
  IGL_D3D12_LOG_VERBOSE("Device::validateMSAAAlignment: Validating MSAA texture (samples=%u, %ux%u)\n",
               desc.numSamples, desc.width, desc.height);

  // Check if texture dimensions are reasonable for MSAA
  // Large MSAA textures may fail due to memory constraints
  const size_t pixelCount = static_cast<size_t>(desc.width) * desc.height;
  const size_t bytesPerPixel = 4;  // Conservative estimate (RGBA8)
  const size_t estimatedSize = pixelCount * bytesPerPixel * desc.numSamples;

  // Warn if MSAA texture is very large (> 256MB)
  if (estimatedSize > 256 * 1024 * 1024) {
    IGL_D3D12_LOG_VERBOSE("Device::validateMSAAAlignment: WARNING - Large MSAA texture detected (%zu MB). "
                 "May cause memory pressure.\n", estimatedSize / (1024 * 1024));
  }

  return true;
}

bool Device::validateTextureAlignment(const D3D12_RESOURCE_DESC& resourceDesc,
                                       uint32_t sampleCount,
                                       Result* IGL_NULLABLE outResult) const {
  // D3D12 texture alignment requirements:
  // - MSAA textures (SampleDesc.Count > 1): 64KB alignment (automatic via CreateCommittedResource)
  // - Regular textures: 64KB alignment (automatic via CreateCommittedResource)
  // - Small textures (<= 64KB): May use 4KB alignment

  // This validation is informational - D3D12 handles alignment automatically
  // We just verify parameters are within expected ranges

  if (sampleCount > 1) {
    // MSAA texture - will use 64KB alignment
    IGL_D3D12_LOG_VERBOSE("Device::validateTextureAlignment: MSAA texture will use 64KB alignment (samples=%u)\n",
                 sampleCount);
  }

  // Validate resource dimensions don't exceed D3D12 limits
  constexpr UINT64 kMaxTextureDimension2D = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;  // 16384

  if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
    if (resourceDesc.Width > kMaxTextureDimension2D || resourceDesc.Height > kMaxTextureDimension2D) {
      IGL_LOG_ERROR("Device::validateTextureAlignment: Texture dimensions (%llux%u) exceed D3D12 limit (%llu)\n",
                    resourceDesc.Width, resourceDesc.Height, kMaxTextureDimension2D);
      Result::setResult(outResult, Result::Code::ArgumentInvalid,
                        "Texture dimensions exceed D3D12 maximum (16384x16384)");
      return false;
    }
  }

  return true;
}

bool Device::validateBufferAlignment(size_t bufferSize, bool isUniform) const {
  // D3D12 buffer alignment requirements:
  // - Constant buffers: 256 bytes (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
  // - Other buffers: No strict alignment requirement

  if (isUniform) {
    // Uniform buffers must be 256-byte aligned
    // This is already handled in createBuffer() by rounding up the size
    if (bufferSize % BUFFER_ALIGNMENT != 0) {
      IGL_D3D12_LOG_VERBOSE("Device::validateBufferAlignment: Uniform buffer size %zu will be rounded up to %zu\n",
                   bufferSize, (bufferSize + BUFFER_ALIGNMENT - 1) & ~(BUFFER_ALIGNMENT - 1));
    }
  }

  return true;
}

// BindGroups
Holder<BindGroupTextureHandle> Device::createBindGroup(
    const BindGroupTextureDesc& desc,
    const IRenderPipelineState* IGL_NULLABLE /*compatiblePipeline*/,
    Result* IGL_NULLABLE outResult) {
  // Store bind group descriptor in pool for later use by encoder
  BindGroupTextureDesc description(desc);
  const auto handle = bindGroupTexturesPool_.create(std::move(description));
  Result::setResult(outResult,
                    handle.empty() ? Result(Result::Code::RuntimeError, "Cannot create bind group")
                                   : Result());
  return {this, handle};
}

Holder<BindGroupBufferHandle> Device::createBindGroup(const BindGroupBufferDesc& desc,
                                                       Result* IGL_NULLABLE outResult) {
  // Store bind group descriptor in pool for later use by encoder
  BindGroupBufferDesc description(desc);
  const auto handle = bindGroupBuffersPool_.create(std::move(description));
  Result::setResult(outResult,
                    handle.empty() ? Result(Result::Code::RuntimeError, "Cannot create bind group")
                                   : Result());
  return {this, handle};
}

void Device::destroy(BindGroupTextureHandle handle) {
  if (handle.empty()) {
    return;
  }
  bindGroupTexturesPool_.destroy(handle);
}

void Device::destroy(BindGroupBufferHandle handle) {
  if (handle.empty()) {
    return;
  }
  bindGroupBuffersPool_.destroy(handle);
}

void Device::destroy(SamplerHandle /*handle*/) {
  // No-op: D3D12 backend doesn't use the SamplerHandle system.
  // Samplers are created as shared_ptr<ISamplerState> and managed via ref-counting.
  // Sampler descriptors are allocated transiently per command encoder at bind time,
  // not persistently at sampler creation time, so there's nothing to deallocate here.
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
  // Single const_cast at the API boundary; all mutation happens in the non-const helper.
  auto& self = const_cast<Device&>(*this);
  return self.createBufferImpl(desc, outResult);
}

std::unique_ptr<IBuffer> Device::createBufferImpl(const BufferDesc& desc,
                                                   Result* IGL_NULLABLE outResult) noexcept {
  auto* device = ctx_->getDevice();
  if (!device) {
    Result::setResult(outResult, Result::Code::RuntimeError, "D3D12 device is null");
    return nullptr;
  }

  // Determine heap type and initial state based on storage
  D3D12_HEAP_TYPE heapType;
  D3D12_RESOURCE_STATES initialState;

  // CRITICAL: Storage buffers with UAV flags MUST use DEFAULT heap
  // D3D12 does not allow UAV resources on UPLOAD heaps
  const bool isStorageBuffer = (desc.type & BufferDesc::BufferTypeBits::Storage) != 0;
  const bool forceDefaultHeap = isStorageBuffer;  // Storage buffers need UAV, which requires DEFAULT heap

  if ((desc.storage == ResourceStorage::Shared || desc.storage == ResourceStorage::Managed) && !forceDefaultHeap) {
    // CPU-writable upload heap (for non-storage buffers only)
    heapType = D3D12_HEAP_TYPE_UPLOAD;
    initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
  } else {
    // GPU-only default heap (required for storage buffers with UAV)
    heapType = D3D12_HEAP_TYPE_DEFAULT;
    initialState = D3D12_RESOURCE_STATE_COMMON;
  }

  // Create heap properties
  D3D12_HEAP_PROPERTIES heapProps = {};
  heapProps.Type = heapType;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

  // For uniform buffers, size must be aligned to 256 bytes (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
  const bool isUniformBuffer = (desc.type & BufferDesc::BufferTypeBits::Uniform) != 0;

  // Validate buffer alignment requirements.
  validateBufferAlignment(desc.length, isUniformBuffer);

  const UINT64 alignedSize = isUniformBuffer
      ? AlignUp<UINT64>(desc.length, 256)  // D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
      : desc.length;

  IGL_D3D12_LOG_VERBOSE("Device::createBuffer: type=%d, requested_size=%zu, aligned_size=%llu, isUniform=%d\n",
               desc.type, desc.length, alignedSize, isUniformBuffer);

  // Create buffer description
  D3D12_RESOURCE_DESC bufferDesc = {};
  bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufferDesc.Alignment = 0;
  bufferDesc.Width = alignedSize;
  bufferDesc.Height = 1;
  bufferDesc.DepthOrArraySize = 1;
  bufferDesc.MipLevels = 1;
  bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
  bufferDesc.SampleDesc.Count = 1;
  bufferDesc.SampleDesc.Quality = 0;
  bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  // Add UAV flag for storage buffers (used by compute shaders)
  // isStorageBuffer already defined above for heap type determination
  if (isStorageBuffer) {
    bufferDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    IGL_D3D12_LOG_VERBOSE("Device::createBuffer: Storage buffer - adding UAV flag\n");
  }

  // Create the buffer resource
  igl::d3d12::ComPtr<ID3D12Resource> buffer;
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

  // Debug: Log GPU address for uniform buffers
  if (isUniformBuffer) {
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddr = buffer->GetGPUVirtualAddress();
    IGL_D3D12_LOG_VERBOSE("Device::createBuffer: Uniform buffer created, GPU address=0x%llx\n", gpuAddr);
  }

  // Upload initial data if provided
  D3D12_RESOURCE_STATES finalState = initialState;

  if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
    finalState = D3D12_RESOURCE_STATE_GENERIC_READ;
  }

  if (desc.data) {
    if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
      void* mappedData = nullptr;
      D3D12_RANGE readRange = {0, 0};
      hr = buffer->Map(0, &readRange, &mappedData);

      if (SUCCEEDED(hr)) {
        std::memcpy(mappedData, desc.data, desc.length);
        buffer->Unmap(0, nullptr);
      }
    } else if (heapType == D3D12_HEAP_TYPE_DEFAULT) {
      // DEFAULT heap: stage through an UPLOAD buffer and copy
      IGL_D3D12_LOG_VERBOSE("Device::createBuffer: Staging initial data via UPLOAD heap for DEFAULT buffer\n");

      // Create upload buffer
      D3D12_HEAP_PROPERTIES uploadHeapProps = {};
      uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
      uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

      // Create upload buffer description WITHOUT UAV flag (UPLOAD heaps can't have UAV)
      D3D12_RESOURCE_DESC uploadBufferDesc = bufferDesc;
      uploadBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;  // Remove UAV flag for upload buffer

      igl::d3d12::ComPtr<ID3D12Resource> uploadBuffer;
      HRESULT upHr = device->CreateCommittedResource(&uploadHeapProps,
                                                     D3D12_HEAP_FLAG_NONE,
                                                     &uploadBufferDesc,
                                                     D3D12_RESOURCE_STATE_GENERIC_READ,
                                                     nullptr,
                                                     IID_PPV_ARGS(uploadBuffer.GetAddressOf()));
      if (FAILED(upHr)) {
        IGL_LOG_ERROR("Device::createBuffer: Failed to create upload buffer: 0x%08X\n", static_cast<unsigned>(upHr));
      } else {
        // Map and copy data
        void* mapped = nullptr;
        D3D12_RANGE rr = {0, 0};
        if (SUCCEEDED(uploadBuffer->Map(0, &rr, &mapped)) && mapped) {
          std::memcpy(mapped, desc.data, desc.length);
          uploadBuffer->Unmap(0, nullptr);

          igl::d3d12::ComPtr<ID3D12CommandAllocator> allocator = getUploadCommandAllocator();
          if (!allocator.Get()) {
            IGL_LOG_ERROR("Device::createBuffer: Failed to get command allocator from pool\n");
          } else {
            igl::d3d12::ComPtr<ID3D12GraphicsCommandList> cmdList;
            if (SUCCEEDED(device->CreateCommandList(0,
                                                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                    allocator.Get(),
                                                    nullptr,
                                                    IID_PPV_ARGS(cmdList.GetAddressOf())))) {
              // Transition default buffer to COPY_DEST
              D3D12_RESOURCE_BARRIER toCopyDest = {};
              toCopyDest.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
              toCopyDest.Transition.pResource = buffer.Get();
              toCopyDest.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
              toCopyDest.Transition.StateBefore = initialState; // COMMON
              toCopyDest.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
              cmdList->ResourceBarrier(1, &toCopyDest);

              // Copy upload -> default
              cmdList->CopyBufferRegion(buffer.Get(), 0, uploadBuffer.Get(), 0, alignedSize);

              // Transition to a likely-read state based on buffer type
              D3D12_RESOURCE_STATES targetState = D3D12_RESOURCE_STATE_GENERIC_READ;
              if (desc.type & BufferDesc::BufferTypeBits::Vertex) {
                targetState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
              } else if (desc.type & BufferDesc::BufferTypeBits::Uniform) {
                targetState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
              } else if (desc.type & BufferDesc::BufferTypeBits::Index) {
                targetState = D3D12_RESOURCE_STATE_INDEX_BUFFER;
              }
              D3D12_RESOURCE_BARRIER toTarget = {};
              toTarget.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
              toTarget.Transition.pResource = buffer.Get();
              toTarget.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
              toTarget.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
              toTarget.Transition.StateAfter = targetState;
              cmdList->ResourceBarrier(1, &toTarget);

              cmdList->Close();
              ID3D12CommandList* lists[] = {cmdList.Get()};
              ctx_->getCommandQueue()->ExecuteCommandLists(1, lists);

              // Use async fence signaling instead of synchronous waitForGPU().
              // Get fence value that will signal when this upload completes.
              UINT64 uploadFenceValue = getNextUploadFenceValue();

              // Signal upload fence after copy completes
              HRESULT hrSignal =
                  ctx_->getCommandQueue()->Signal(allocatorPool_.getUploadFence(), uploadFenceValue);
              if (FAILED(hrSignal)) {
                IGL_LOG_ERROR("Device::createBuffer: Failed to signal upload fence: 0x%08X\n", hrSignal);
                // Return allocator with 0 to avoid blocking the pool
                returnUploadCommandAllocator(allocator, 0);
              } else {
                // Return allocator to pool with fence value (will be reused after the fence is signaled).
                returnUploadCommandAllocator(allocator, uploadFenceValue);

                // Track staging buffer for async cleanup with the associated fence value.
                trackUploadBuffer(std::move(uploadBuffer), uploadFenceValue);
              }

              finalState = targetState;
            } else {
              IGL_LOG_ERROR("Device::createBuffer: Failed to create command list\n");
              // Return allocator with 0 to avoid blocking the pool
              returnUploadCommandAllocator(allocator, 0);
            }
          }
        }
      }
    }
  }

  Result::setOk(outResult);
  return std::make_unique<Buffer>(const_cast<Device&>(*this), std::move(buffer), desc, finalState);
}

std::shared_ptr<IDepthStencilState> Device::createDepthStencilState(
    const DepthStencilStateDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  Result::setOk(outResult);
  return std::make_shared<DepthStencilState>(desc);
}

std::unique_ptr<IShaderStages> Device::createShaderStages(const ShaderStagesDesc& desc,
                                                          Result* IGL_NULLABLE
                                                              outResult) const {
  Result::setOk(outResult);
  return std::make_unique<ShaderStages>(desc);
}

std::shared_ptr<ISamplerState> Device::createSamplerState(const SamplerStateDesc& desc,
                                                          Result* IGL_NULLABLE outResult) const {
  return samplerCache_.createSamplerState(desc, outResult);
}

std::shared_ptr<ITexture> Device::createTexture(const TextureDesc& desc,
                                                Result* IGL_NULLABLE outResult) const noexcept {
  auto* device = ctx_->getDevice();

  // Check for exportability - D3D12 doesn't support exportable textures
  if (desc.exportability == TextureDesc::TextureExportability::Exportable) {
    Result::setResult(outResult, Result::Code::Unimplemented,
                      "D3D12 does not support exportable textures");
    return nullptr;
  }

  // Convert IGL texture format to DXGI format
  DXGI_FORMAT dxgiFormat = textureFormatToDXGIFormat(desc.format);
  IGL_D3D12_LOG_VERBOSE("Device::createTexture: IGL format=%d -> DXGI format=%d\n", (int)desc.format, (int)dxgiFormat);
  if (dxgiFormat == DXGI_FORMAT_UNKNOWN) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Unsupported texture format");
    return nullptr;
  }

  // Create texture resource description
  D3D12_RESOURCE_DESC resourceDesc = {};

  // Set dimension based on texture type
  if (desc.type == TextureType::ThreeD) {
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    resourceDesc.DepthOrArraySize = static_cast<UINT16>(desc.depth);
  } else if (desc.type == TextureType::Cube) {
    // Cube textures are 2D textures with 6 array slices per layer (one per face).
    // For cube arrays: numLayers * 6 faces.
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.DepthOrArraySize = static_cast<UINT16>(desc.numLayers * 6);
    IGL_D3D12_LOG_VERBOSE("Device::createTexture: Cube texture with %u layers -> %u array slices\n",
                 desc.numLayers, resourceDesc.DepthOrArraySize);
  } else {
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.DepthOrArraySize = static_cast<UINT16>(desc.numLayers);
  }

  const bool sampledUsage =
      (desc.usage & TextureDesc::TextureUsageBits::Sampled) != 0;
  const DXGI_FORMAT resourceFormat =
      textureFormatToDXGIResourceFormat(desc.format, sampledUsage);
  if (resourceFormat == DXGI_FORMAT_UNKNOWN) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Unsupported resource format");
    return nullptr;
  }
  resourceDesc.Alignment = 0;
  resourceDesc.Width = desc.width;
  resourceDesc.Height = desc.height;
  resourceDesc.MipLevels = static_cast<UINT16>(desc.numMipLevels);
  resourceDesc.Format = resourceFormat;

  // MSAA configuration
  // D3D12 MSAA requirements:
  // - Sample count must be 1, 2, 4, 8, or 16 (power of 2)
  // - Quality level 0 is standard MSAA (higher quality levels are vendor-specific)
  // - MSAA textures cannot have mipmaps (numMipLevels must be 1)
  // - Not all formats support all sample counts - validation required
  const uint32_t sampleCount = std::max(1u, desc.numSamples);

  // Validate MSAA alignment requirements before creating the resource.
  if (sampleCount > 1) {
    if (!validateMSAAAlignment(desc, outResult)) {
      // Error already set by validation function
      return nullptr;
    }
  }

  // Validate MSAA constraints.
  if (sampleCount > 1) {
    // MSAA textures cannot have mipmaps
    if (desc.numMipLevels > 1) {
      IGL_LOG_ERROR("Device::createTexture: MSAA textures cannot have mipmaps (numMipLevels=%u, numSamples=%u)\n",
                    desc.numMipLevels, sampleCount);
      Result::setResult(outResult, Result::Code::ArgumentInvalid,
                        "MSAA textures cannot have mipmaps (numMipLevels must be 1)");
      return nullptr;
    }

    // Validate that the requested MSAA sample count is supported for this format.
    // NOTE: Applications should query DeviceFeatureLimits::MaxMultisampleCount proactively
    //       to avoid runtime errors. Use getMaxMSAASamplesForFormat() for format-specific queries.
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msqLevels = {};
    msqLevels.Format = dxgiFormat;
    msqLevels.SampleCount = sampleCount;
    msqLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msqLevels, sizeof(msqLevels))) ||
        msqLevels.NumQualityLevels == 0) {
      // Query maximum supported samples for better error messages.
      const uint32_t maxSamples = getMaxMSAASamplesForFormat(desc.format);

      char errorMsg[512];
      snprintf(errorMsg, sizeof(errorMsg),
               "Device::createTexture: Format %d does not support %u samples (max supported: %u). "
               "Query DeviceFeatureLimits::MaxMultisampleCount before texture creation.",
               static_cast<int>(dxgiFormat), sampleCount, maxSamples);
      IGL_LOG_ERROR("%s\n", errorMsg);
      Result::setResult(outResult, Result::Code::Unsupported, errorMsg);
      return nullptr;
    }

    IGL_D3D12_LOG_VERBOSE("Device::createTexture: MSAA enabled - format=%d, samples=%u, quality levels=%u\n",
                 static_cast<int>(dxgiFormat), sampleCount, msqLevels.NumQualityLevels);
  }

  resourceDesc.SampleDesc.Count = sampleCount;
  resourceDesc.SampleDesc.Quality = 0;  // Standard MSAA quality (0 = default/standard)
  resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  // Set resource flags based on usage
  if (desc.usage & TextureDesc::TextureUsageBits::Sampled) {
    // Shader resource - no special flags needed
  }
  if (desc.usage & TextureDesc::TextureUsageBits::Storage) {
    resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  }
  if (desc.usage & TextureDesc::TextureUsageBits::Attachment) {
    if (desc.format >= TextureFormat::Z_UNorm16 && desc.format <= TextureFormat::S_UInt8) {
      resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    } else {
      resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
  }

  // Create heap properties
  D3D12_HEAP_PROPERTIES heapProps = {};
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  // Determine initial state
  D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

  // Prepare optimized clear value for render targets and depth/stencil
  D3D12_CLEAR_VALUE clearValue = {};
  D3D12_CLEAR_VALUE* pClearValue = nullptr;

  if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
    clearValue.Format = dxgiFormat;
    clearValue.Color[0] = 0.0f;  // Default clear color: black
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 1.0f;  // Alpha = 1
    pClearValue = &clearValue;
  } else if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
    clearValue.Format = dxgiFormat;
    clearValue.DepthStencil.Depth = 1.0f;     // Default far plane
    clearValue.DepthStencil.Stencil = 0;
    pClearValue = &clearValue;
  }

  // Validate texture alignment before creating the resource.
  if (!validateTextureAlignment(resourceDesc, sampleCount, outResult)) {
    // Error already set by validation function
    return nullptr;
  }

  // Create the texture resource
  igl::d3d12::ComPtr<ID3D12Resource> resource;
  HRESULT hr = device->CreateCommittedResource(
      &heapProps,
      D3D12_HEAP_FLAG_NONE,
      &resourceDesc,
      initialState,
      pClearValue,  // Optimized clear value for render targets/depth-stencil
      IID_PPV_ARGS(resource.GetAddressOf()));

  if (FAILED(hr)) {
    char errorMsg[512];
    if (hr == DXGI_ERROR_DEVICE_REMOVED) {
      HRESULT removedReason = device->GetDeviceRemovedReason();
      snprintf(errorMsg, sizeof(errorMsg),
               "Failed to create texture resource. Device removed! HRESULT: 0x%08X, Removed reason: 0x%08X",
               static_cast<unsigned>(hr), static_cast<unsigned>(removedReason));
    } else {
      snprintf(errorMsg, sizeof(errorMsg), "Failed to create texture resource. HRESULT: 0x%08X", static_cast<unsigned>(hr));
    }
    Result::setResult(outResult, Result::Code::RuntimeError, errorMsg);
    return nullptr;
  }

  // Create IGL texture from D3D12 resource.
  auto texture = Texture::createFromResource(
      resource.Get(), desc.format, desc, device, ctx_->getCommandQueue(), initialState,
      const_cast<Device*>(this));
  Result::setOk(outResult);
  return texture;
}

std::shared_ptr<ITexture> Device::createTextureView(std::shared_ptr<ITexture> texture,
                                                    const TextureViewDesc& desc,
                                                    Result* IGL_NULLABLE
                                                        outResult) const noexcept {
  if (!texture) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Parent texture is null");
    return nullptr;
  }

  // Cast to D3D12 texture
  auto d3d12Texture = std::static_pointer_cast<Texture>(texture);
  if (!d3d12Texture) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Texture is not a D3D12 texture");
    return nullptr;
  }

  // Create the texture view
  auto view = Texture::createTextureView(d3d12Texture, desc);
  if (!view) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Failed to create texture view");
    return nullptr;
  }

  Result::setOk(outResult);
  return view;
}

std::shared_ptr<ITimer> Device::createTimer(Result* IGL_NULLABLE outResult) const noexcept {
  auto timer = std::make_shared<Timer>(*this);
  Result::setOk(outResult);
  return timer;
}

std::shared_ptr<IVertexInputState> Device::createVertexInputState(
    const VertexInputStateDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  Result::setOk(outResult);
  return std::make_shared<VertexInputState>(desc);
}

std::shared_ptr<IComputePipelineState> Device::createComputePipeline(
    const ComputePipelineDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  IGL_D3D12_LOG_VERBOSE("Device::createComputePipeline() START - debugName='%s'\n", desc.debugName.c_str());

  auto* device = ctx_->getDevice();
  if (!device) {
    IGL_LOG_ERROR("  D3D12 device is null!\n");
    Result::setResult(outResult, Result::Code::InvalidOperation, "D3D12 device is null");
    return nullptr;
  }

  if (!desc.shaderStages) {
    IGL_LOG_ERROR("  Shader stages are required!\n");
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Shader stages are required");
    return nullptr;
  }

  if (desc.shaderStages->getType() != ShaderStagesType::Compute) {
    IGL_LOG_ERROR("  Shader stages must be compute type!\n");
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Shader stages must be compute type");
    return nullptr;
  }

  // Get compute shader module
  auto* computeModule = static_cast<const ShaderModule*>(desc.shaderStages->getComputeModule().get());
  if (!computeModule) {
    IGL_LOG_ERROR("  Compute module is null!\n");
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Compute shader required");
    return nullptr;
  }

  IGL_D3D12_LOG_VERBOSE("  Getting compute shader bytecode...\n");
  const auto& csBytecode = computeModule->getBytecode();
  IGL_D3D12_LOG_VERBOSE("  CS bytecode: %zu bytes\n", csBytecode.size());

  // Create root signature for compute
  // Root signature layout for compute:
  // - Root parameter 0: Root Constants for b0 (Push Constants)
  // - Root parameter 1: Descriptor table with unbounded UAVs (u0-uN)
  // - Root parameter 2: Descriptor table with unbounded SRVs (t0-tN)
  // - Root parameter 3: Descriptor table with unbounded CBVs (b1-bN)
  // - Root parameter 4: Descriptor table with unbounded Samplers (s0-sN)

  // Query root signature capabilities to determine descriptor range bounds.
  // Tier 1 devices require bounded descriptor ranges.
  const D3D12_RESOURCE_BINDING_TIER bindingTier = ctx_->getResourceBindingTier();
  const bool needsBoundedRanges = (bindingTier == D3D12_RESOURCE_BINDING_TIER_1);

  // Conservative bounds for Tier 1 devices (based on actual usage in render sessions)
  // These limits are sufficient for all current IGL usage patterns
  const UINT uavBound = needsBoundedRanges ? 64 : UINT_MAX;
  const UINT srvBound = needsBoundedRanges ? 128 : UINT_MAX;
  const UINT cbvBound = needsBoundedRanges ? 64 : UINT_MAX;
  const UINT samplerBound = needsBoundedRanges ? 32 : UINT_MAX;  // Samplers always bounded on Tier 1/2

  if (needsBoundedRanges) {
    IGL_D3D12_LOG_VERBOSE("  Using bounded descriptor ranges (Tier 1): UAV=%u, SRV=%u, CBV=%u, Sampler=%u\n",
                 uavBound, srvBound, cbvBound, samplerBound);
  } else {
    IGL_D3D12_LOG_VERBOSE("  Using unbounded descriptor ranges (Tier %u)\n",
                 bindingTier == D3D12_RESOURCE_BINDING_TIER_3 ? 3 : 2);
  }

  // Descriptor range for UAVs (unordered access views - read/write buffers and textures).
  D3D12_DESCRIPTOR_RANGE uavRange = {};
  uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
  uavRange.NumDescriptors = uavBound;
  uavRange.BaseShaderRegister = 0;  // Starting at u0
  uavRange.RegisterSpace = 0;
  uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // Descriptor range for SRVs (shader resource views - read-only textures and buffers)
  D3D12_DESCRIPTOR_RANGE srvRange = {};
  srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  srvRange.NumDescriptors = srvBound;
  srvRange.BaseShaderRegister = 0;  // Starting at t0
  srvRange.RegisterSpace = 0;
  srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // Descriptor range for CBVs (constant buffer views).
  // Note: b0 will be used for root constants (push constants), so the CBV table starts at b1.
  D3D12_DESCRIPTOR_RANGE cbvRange = {};
  cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
  cbvRange.NumDescriptors = cbvBound;
  cbvRange.BaseShaderRegister = 1;  // Starting at b1 (b0 is root constants)
  cbvRange.RegisterSpace = 0;
  cbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // Descriptor range for Samplers.
  D3D12_DESCRIPTOR_RANGE samplerRange = {};
  samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
  samplerRange.NumDescriptors = samplerBound;
  samplerRange.BaseShaderRegister = 0;  // Starting at s0
  samplerRange.RegisterSpace = 0;
  samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // Root parameters
  D3D12_ROOT_PARAMETER rootParams[5] = {};

  // Parameter 0: Root Constants for b0 (Push Constants)
  //  Increased from 16 to 32 DWORDs (64→128 bytes) to match Vulkan
  // Using 32-bit constants for push constants in compute shaders
  rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  rootParams[0].Constants.ShaderRegister = 0;  // b0
  rootParams[0].Constants.RegisterSpace = 0;
  rootParams[0].Constants.Num32BitValues = 32;  // 32 DWORDs = 128 bytes (matches Vulkan)
  rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 1: Descriptor table for UAVs
  rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
  rootParams[1].DescriptorTable.pDescriptorRanges = &uavRange;
  rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 2: Descriptor table for SRVs
  rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
  rootParams[2].DescriptorTable.pDescriptorRanges = &srvRange;
  rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 3: Descriptor table for CBVs (b1+)
  // Note: b0 is now root constants, this table starts at b1
  rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
  rootParams[3].DescriptorTable.pDescriptorRanges = &cbvRange;
  rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 4: Descriptor table for Samplers
  rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParams[4].DescriptorTable.NumDescriptorRanges = 1;
  rootParams[4].DescriptorTable.pDescriptorRanges = &samplerRange;
  rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
  rootSigDesc.NumParameters = 5;
  rootSigDesc.pParameters = rootParams;
  rootSigDesc.NumStaticSamplers = 0;
  rootSigDesc.pStaticSamplers = nullptr;
  rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

  // CRITICAL: Validate root signature cost (64 DWORD hardware limit).
  IGL_D3D12_LOG_VERBOSE("  Validating compute root signature cost:\n");
  const uint32_t cost = calculateRootSignatureCost(rootSigDesc);
  IGL_D3D12_LOG_VERBOSE("  Total cost: %u / 64 DWORDs (%.1f%%)\n", cost, 100.0f * cost / 64.0f);

  // Warning threshold at 50% (32 DWORDs)
  if (cost > 32) {
    IGL_D3D12_LOG_VERBOSE("  WARNING: Root signature cost exceeds 50%% of limit: %u / 64 DWORDs\n", cost);
  }

  // Hard limit enforcement
  IGL_DEBUG_ASSERT(cost <= 64, "Root signature exceeds 64 DWORD limit!");
  if (cost > 64) {
    IGL_LOG_ERROR("  ROOT SIGNATURE COST OVERFLOW: %u DWORDs (limit: 64)\n", cost);
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange,
                      "Root signature cost exceeds 64 DWORD hardware limit");
    return nullptr;
  }

  IGL_D3D12_LOG_VERBOSE("  Creating compute root signature with Root Constants (b0)/UAVs/SRVs/CBVs/Samplers\n");

  // Get or create cached root signature.
  igl::d3d12::ComPtr<ID3D12RootSignature> rootSignature =
      pipelineCache_.getOrCreateRootSignature(ctx_->getDevice(), rootSigDesc, outResult);
  if (!rootSignature.Get()) {
    return nullptr;
  }

  // Create compute pipeline state
  D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.pRootSignature = rootSignature.Get();
  psoDesc.CS.pShaderBytecode = csBytecode.data();
  psoDesc.CS.BytecodeLength = csBytecode.size();
  psoDesc.NodeMask = 0;
  psoDesc.CachedPSO.pCachedBlob = nullptr;
  psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
  psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

  // PSO cache lookup (thread-safe with double-checked locking).
  const size_t psoHash = pipelineCache_.hashComputePipelineDesc(desc);
  igl::d3d12::ComPtr<ID3D12PipelineState> pipelineState;

  // First check: Lock for cache lookup
  {
    std::lock_guard<std::mutex> lock(pipelineCache_.psoCacheMutex_);
    auto psoIt = pipelineCache_.computePSOCache_.find(psoHash);
    if (psoIt != pipelineCache_.computePSOCache_.end()) {
      // Cache hit - reuse existing PSO
      pipelineCache_.computePSOCacheHits_++;
      pipelineState = psoIt->second;  // Assignment creates a ref-counted copy
      IGL_D3D12_LOG_VERBOSE("  [PSO CACHE HIT] Hash=0x%zx, hits=%zu, misses=%zu, hit rate=%.1f%%\n",
                   psoHash,
                   pipelineCache_.computePSOCacheHits_,
                   pipelineCache_.computePSOCacheMisses_,
                   100.0 * pipelineCache_.computePSOCacheHits_ /
                       (pipelineCache_.computePSOCacheHits_ +
                        pipelineCache_.computePSOCacheMisses_));
      IGL_D3D12_LOG_VERBOSE("Device::createComputePipeline() SUCCESS (CACHED) - PSO=%p, RootSig=%p\n",
                   pipelineState.Get(), rootSignature.Get());
      Result::setOk(outResult);
      // Create a copy of the root signature for the returned object
      igl::d3d12::ComPtr<ID3D12RootSignature> rootSigCopy = rootSignature;
      return std::make_shared<ComputePipelineState>(desc, std::move(pipelineState), std::move(rootSigCopy));
    }
  }

  // Cache miss - create new PSO outside lock (expensive operation)
  IGL_D3D12_LOG_VERBOSE("  [PSO CACHE MISS] Hash=0x%zx\n", psoHash);

  IGL_D3D12_LOG_VERBOSE("  Creating compute pipeline state...\n");
  HRESULT hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("  CreateComputePipelineState FAILED: 0x%08X\n", static_cast<unsigned>(hr));

    // Print debug layer messages if available
    igl::d3d12::ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf())))) {
      UINT64 numMessages = infoQueue->GetNumStoredMessages();
      IGL_LOG_ERROR("  D3D12 Info Queue has %llu messages:\n", numMessages);
      for (UINT64 i = 0; i < numMessages; ++i) {
        SIZE_T messageLength = 0;
        infoQueue->GetMessage(i, nullptr, &messageLength);
        if (messageLength > 0) {
          auto message = (D3D12_MESSAGE*)malloc(messageLength);
          if (message && SUCCEEDED(infoQueue->GetMessage(i, message, &messageLength))) {
            const char* severityStr = "UNKNOWN";
            switch (message->Severity) {
              case D3D12_MESSAGE_SEVERITY_CORRUPTION: severityStr = "CORRUPTION"; break;
              case D3D12_MESSAGE_SEVERITY_ERROR: severityStr = "ERROR"; break;
              case D3D12_MESSAGE_SEVERITY_WARNING: severityStr = "WARNING"; break;
              case D3D12_MESSAGE_SEVERITY_INFO: severityStr = "INFO"; break;
              case D3D12_MESSAGE_SEVERITY_MESSAGE: severityStr = "MESSAGE"; break;
            }
            IGL_LOG_ERROR("    [%s] %s\n", severityStr, message->pDescription);
          }
          free(message);
        }
      }
      infoQueue->ClearStoredMessages();
    }

    Result::setResult(outResult, Result::Code::RuntimeError, "Failed to create compute pipeline state");
    return nullptr;
  }

  // E-011: Set debug name on compute PSO for better debugging in PIX/RenderDoc
  if (desc.shaderStages && desc.shaderStages->getComputeModule()) {
    const std::string& psoName = desc.shaderStages->getComputeModule()->info().debugName;
    if (!psoName.empty()) {
      // Convert to wide string for D3D12 SetName API
      std::wstring wideName(psoName.begin(), psoName.end());
      pipelineState->SetName(wideName.c_str());
      IGL_D3D12_LOG_VERBOSE("  Set compute PSO debug name: %s\n", psoName.c_str());
    }
  }

  // Second check: Lock for cache insertion with double-check.
  // Another thread may have created the PSO while we were creating ours
  {
    std::lock_guard<std::mutex> lock(pipelineCache_.psoCacheMutex_);
    auto psoIt = pipelineCache_.computePSOCache_.find(psoHash);
    if (psoIt != pipelineCache_.computePSOCache_.end()) {
      // Another thread beat us to it - use their PSO
      pipelineCache_.computePSOCacheHits_++;
      pipelineState = psoIt->second;
      IGL_D3D12_LOG_VERBOSE("  [PSO DOUBLE-CHECK HIT] Another thread created PSO, using theirs. Hash=0x%zx\n", psoHash);
    } else {
      // We're the first to complete - cache our PSO
      pipelineCache_.computePSOCacheMisses_++;
      pipelineCache_.computePSOCache_[psoHash] = pipelineState;
      IGL_D3D12_LOG_VERBOSE("  [PSO CACHED] Hash=0x%zx, hits=%zu, misses=%zu\n",
                   psoHash,
                   pipelineCache_.computePSOCacheHits_,
                   pipelineCache_.computePSOCacheMisses_);
    }
  }

  IGL_D3D12_LOG_VERBOSE("Device::createComputePipeline() SUCCESS - PSO=%p, RootSig=%p (hash=0x%zx)\n",
               pipelineState.Get(), rootSignature.Get(), psoHash);
  Result::setOk(outResult);
  return std::make_shared<ComputePipelineState>(desc, std::move(pipelineState), std::move(rootSignature));
}

std::shared_ptr<IRenderPipelineState> Device::createRenderPipeline(
    const RenderPipelineDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  IGL_D3D12_LOG_VERBOSE("Device::createRenderPipeline() START - debugName='%s'\n", desc.debugName.c_str());

  auto* device = ctx_->getDevice();
  if (!device) {
    IGL_LOG_ERROR("  D3D12 device is null!\n");
    Result::setResult(outResult, Result::Code::InvalidOperation, "D3D12 device is null");
    return nullptr;
  }

  if (!desc.shaderStages) {
    IGL_LOG_ERROR("  Shader stages are required!\n");
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Shader stages are required");
    return nullptr;
  }

  // Get shader modules
  auto* vertexModule = static_cast<const ShaderModule*>(desc.shaderStages->getVertexModule().get());
  auto* fragmentModule = static_cast<const ShaderModule*>(desc.shaderStages->getFragmentModule().get());

  if (!vertexModule || !fragmentModule) {
    IGL_LOG_ERROR("  Vertex or fragment module is null!\n");
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Vertex and fragment shaders required");
    return nullptr;
  }

  IGL_D3D12_LOG_VERBOSE("  Getting shader bytecode...\n");
  // Get shader bytecode first
  const auto& vsBytecode = vertexModule->getBytecode();
  const auto& psBytecode = fragmentModule->getBytecode();
  IGL_D3D12_LOG_VERBOSE("  VS bytecode: %zu bytes, PS bytecode: %zu bytes\n", vsBytecode.size(), psBytecode.size());

  // Create root signature with descriptor tables for textures and constant buffers
  // Root signature layout:
  // - Root parameter 0: Root Constants for b2 (Push Constants) - 16 DWORDs = 64 bytes max
  // - Root parameter 1: Root CBV for b0 (legacy bindBuffer support)
  // - Root parameter 2: Root CBV for b1 (legacy bindBuffer support)
  // - Root parameter 3: Descriptor table for CBVs b3-b15 (bindBindGroup support)
  // - Root parameter 4: Descriptor table with SRVs for textures t0-tN (unbounded)
  // - Root parameter 5: Descriptor table with Samplers for s0-sN (unbounded)
  //
  // This hybrid approach supports:
  //   - Legacy sessions using bindBuffer(0/1) -> root CBVs at b0/b1
  //   - New buffer bind groups using bindBindGroup() -> descriptor table at b3-b15
  //   - Push constants at b2 (inline root constants, cannot overlap with CBV table)

  // Query root signature capabilities to determine descriptor range bounds.
  // Tier 1 devices (integrated GPUs, WARP) require bounded descriptor ranges.
  const D3D12_RESOURCE_BINDING_TIER bindingTier = ctx_->getResourceBindingTier();
  const bool needsBoundedRanges = (bindingTier == D3D12_RESOURCE_BINDING_TIER_1);

  // Conservative bounds for Tier 1 devices (based on actual usage in render sessions)
  // These limits are sufficient for all current IGL usage patterns
  const UINT srvBound = needsBoundedRanges ? 128 : UINT_MAX;
  const UINT samplerBound = needsBoundedRanges ? 32 : UINT_MAX;

  if (needsBoundedRanges) {
    IGL_D3D12_LOG_VERBOSE("  Using bounded descriptor ranges (Tier 1): SRV=%u, Sampler=%u\n",
                 srvBound, samplerBound);
  } else {
    IGL_D3D12_LOG_VERBOSE("  Using unbounded descriptor ranges (Tier %u)\n",
                 bindingTier == D3D12_RESOURCE_BINDING_TIER_3 ? 3 : 2);
  }

  // Descriptor range for CBVs b3-b15 (buffer bind groups)
  D3D12_DESCRIPTOR_RANGE cbvRange = {};
  cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
  cbvRange.NumDescriptors = 13;  // b3 through b15
  cbvRange.BaseShaderRegister = 3;  // Starting at b3 (b2 reserved for push constants)
  cbvRange.RegisterSpace = 0;
  cbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // Descriptor range for SRVs (textures)
  // Use bounded ranges on Tier 1 hardware (integrated GPUs, WARP), unbounded on Tier 2+
  D3D12_DESCRIPTOR_RANGE srvRange = {};
  srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  srvRange.NumDescriptors = srvBound;
  srvRange.BaseShaderRegister = 0;  // Starting at t0
  srvRange.RegisterSpace = 0;
  srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // Descriptor range for Samplers
  // Use bounded ranges on Tier 1 hardware (integrated GPUs, WARP), unbounded on Tier 2+
  D3D12_DESCRIPTOR_RANGE samplerRange = {};
  samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
  samplerRange.NumDescriptors = samplerBound;
  samplerRange.BaseShaderRegister = 0;  // Starting at s0
  samplerRange.RegisterSpace = 0;
  samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // Descriptor range for UAVs (read-write storage buffers).
  // Use bounded range for Tier 1, unbounded for Tier 2+.
  const UINT uavBound = needsBoundedRanges ? 8 : UINT_MAX;  // Conservative: 8 UAVs for storage buffers
  D3D12_DESCRIPTOR_RANGE uavRange = {};
  uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
  uavRange.NumDescriptors = uavBound;
  uavRange.BaseShaderRegister = 0;  // Starting at u0
  uavRange.RegisterSpace = 0;
  uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // Root parameters (increased to 7 to add UAV support).
  D3D12_ROOT_PARAMETER rootParams[7] = {};

  // Parameter 0: Root Constants for b2 (Push Constants)
  //  Increased from 16 to 32 DWORDs (64→128 bytes) to match Vulkan
  rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  rootParams[0].Constants.ShaderRegister = 2;  // b2
  rootParams[0].Constants.RegisterSpace = 0;
  rootParams[0].Constants.Num32BitValues = 32;  // 32 DWORDs = 128 bytes (matches Vulkan)
  rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 1: Root CBV for b0 (legacy bindBuffer support)
  rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rootParams[1].Descriptor.ShaderRegister = 0;  // b0
  rootParams[1].Descriptor.RegisterSpace = 0;
  rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 2: Root CBV for b1 (legacy bindBuffer support)
  rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rootParams[2].Descriptor.ShaderRegister = 1;  // b1
  rootParams[2].Descriptor.RegisterSpace = 0;
  rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 3: Descriptor table for CBVs b2-b15 (buffer bind groups)
  rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
  rootParams[3].DescriptorTable.pDescriptorRanges = &cbvRange;
  rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 4: Descriptor table for SRVs (textures)
  rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParams[4].DescriptorTable.NumDescriptorRanges = 1;
  rootParams[4].DescriptorTable.pDescriptorRanges = &srvRange;
  // Enable texture access in all shader stages (vertex, pixel, etc.).
  rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 5: Descriptor table for Samplers.
  rootParams[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParams[5].DescriptorTable.NumDescriptorRanges = 1;
  rootParams[5].DescriptorTable.pDescriptorRanges = &samplerRange;
  // Enable sampler access in all shader stages (vertex, pixel, etc.).
  rootParams[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 6: Descriptor table for UAVs (storage buffers).
  rootParams[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParams[6].DescriptorTable.NumDescriptorRanges = 1;
  rootParams[6].DescriptorTable.pDescriptorRanges = &uavRange;
  rootParams[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
  // Updated to 7 parameters (added UAV support).
  rootSigDesc.NumParameters = 7;
  rootSigDesc.pParameters = rootParams;
  rootSigDesc.NumStaticSamplers = 0;
  rootSigDesc.pStaticSamplers = nullptr;
  rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  // CRITICAL: Validate root signature cost (64 DWORD hardware limit).
  IGL_D3D12_LOG_VERBOSE("  Validating render root signature cost:\n");
  const uint32_t cost = calculateRootSignatureCost(rootSigDesc);
  IGL_D3D12_LOG_VERBOSE("  Total cost: %u / 64 DWORDs (%.1f%%)\n", cost, 100.0f * cost / 64.0f);

  // Warning threshold at 50% (32 DWORDs)
  if (cost > 32) {
    IGL_D3D12_LOG_VERBOSE("  WARNING: Root signature cost exceeds 50%% of limit: %u / 64 DWORDs\n", cost);
  }

  // Hard limit enforcement
  IGL_DEBUG_ASSERT(cost <= 64, "Root signature exceeds 64 DWORD limit!");
  if (cost > 64) {
    IGL_LOG_ERROR("  ROOT SIGNATURE COST OVERFLOW: %u DWORDs (limit: 64)\n", cost);
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange,
                      "Root signature cost exceeds 64 DWORD hardware limit");
    return nullptr;
  }

  IGL_D3D12_LOG_VERBOSE("  Creating root signature with Push Constants (b2)/Root CBVs (b0,b1)/CBV Table (b3-b15)/SRVs/Samplers/UAVs\n");

  // Get or create cached root signature.
  igl::d3d12::ComPtr<ID3D12RootSignature> rootSignature =
      pipelineCache_.getOrCreateRootSignature(ctx_->getDevice(), rootSigDesc, outResult);
  if (!rootSignature.Get()) {
    return nullptr;
  }

  // Create PSO - zero-initialize all fields
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.pRootSignature = rootSignature.Get();

  // Shader bytecode
  psoDesc.VS = {vsBytecode.data(), vsBytecode.size()};
  psoDesc.PS = {psBytecode.data(), psBytecode.size()};
  // Explicitly zero unused shader stages
  psoDesc.DS = {nullptr, 0};
  psoDesc.HS = {nullptr, 0};
  psoDesc.GS = {nullptr, 0};

  // Rasterizer state - configure based on pipeline descriptor
  // Fill mode (solid vs wireframe)
  psoDesc.RasterizerState.FillMode = (desc.polygonFillMode == PolygonFillMode::Line)
      ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;

  // Cull mode configuration
  switch (desc.cullMode) {
    case CullMode::Back:
      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
      break;
    case CullMode::Front:
      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
      break;
    case CullMode::Disabled:
    default:
      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      break;
  }

  // Front face winding order
  psoDesc.RasterizerState.FrontCounterClockwise =
      (desc.frontFaceWinding == WindingMode::CounterClockwise) ? TRUE : FALSE;

  // Depth bias (polygon offset) - baseline values set in PSO
  // Note: IGL doesn't currently expose depth bias in RenderPipelineDesc
  // Applications can dynamically adjust depth bias via RenderCommandEncoder::setDepthBias()
  // These PSO values serve as the baseline which can be dynamically overridden
  psoDesc.RasterizerState.DepthBias = 0;  // Integer depth bias (default: no bias)
  psoDesc.RasterizerState.DepthBiasClamp = 0.0f;  // Max depth bias value (default: no clamp)
  psoDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;  // Slope-scaled bias for angled surfaces

  psoDesc.RasterizerState.DepthClipEnable = TRUE;  // Enable depth clipping
  psoDesc.RasterizerState.MultisampleEnable = (desc.sampleCount > 1) ? TRUE : FALSE;
  psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
  psoDesc.RasterizerState.ForcedSampleCount = 0;
  psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

  // Blend state - configure per render target based on pipeline descriptor
  psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
  const size_t numColorAttachments = desc.targetDesc.colorAttachments.size();
  psoDesc.BlendState.IndependentBlendEnable = numColorAttachments > 1 ? TRUE : FALSE;

  // Helper to convert IGL blend factor to D3D12
  auto toD3D12Blend = [](BlendFactor f) {
    switch (f) {
      case BlendFactor::Zero: return D3D12_BLEND_ZERO;
      case BlendFactor::One: return D3D12_BLEND_ONE;
      case BlendFactor::SrcColor: return D3D12_BLEND_SRC_COLOR;
      case BlendFactor::OneMinusSrcColor: return D3D12_BLEND_INV_SRC_COLOR;
      case BlendFactor::SrcAlpha: return D3D12_BLEND_SRC_ALPHA;
      case BlendFactor::OneMinusSrcAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
      case BlendFactor::DstColor: return D3D12_BLEND_DEST_COLOR;
      case BlendFactor::OneMinusDstColor: return D3D12_BLEND_INV_DEST_COLOR;
      case BlendFactor::DstAlpha: return D3D12_BLEND_DEST_ALPHA;
      case BlendFactor::OneMinusDstAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
      case BlendFactor::SrcAlphaSaturated: return D3D12_BLEND_SRC_ALPHA_SAT;
      case BlendFactor::BlendColor: return D3D12_BLEND_BLEND_FACTOR;
      case BlendFactor::OneMinusBlendColor: return D3D12_BLEND_INV_BLEND_FACTOR;
      case BlendFactor::BlendAlpha: return D3D12_BLEND_BLEND_FACTOR; // D3D12 uses same constant for RGB and Alpha
      case BlendFactor::OneMinusBlendAlpha: return D3D12_BLEND_INV_BLEND_FACTOR; // D3D12 uses same constant for RGB and Alpha
      case BlendFactor::Src1Color: return D3D12_BLEND_SRC1_COLOR; // Dual-source blending
      case BlendFactor::OneMinusSrc1Color: return D3D12_BLEND_INV_SRC1_COLOR; // Dual-source blending
      case BlendFactor::Src1Alpha: return D3D12_BLEND_SRC1_ALPHA; // Dual-source blending
      case BlendFactor::OneMinusSrc1Alpha: return D3D12_BLEND_INV_SRC1_ALPHA; // Dual-source blending
      default: return D3D12_BLEND_ONE;
    }
  };

  auto toD3D12BlendOp = [](BlendOp op) {
    switch (op) {
      case BlendOp::Add: return D3D12_BLEND_OP_ADD;
      case BlendOp::Subtract: return D3D12_BLEND_OP_SUBTRACT;
      case BlendOp::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
      case BlendOp::Min: return D3D12_BLEND_OP_MIN;
      case BlendOp::Max: return D3D12_BLEND_OP_MAX;
      default: return D3D12_BLEND_OP_ADD;
    }
  };

  for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
    if (i < desc.targetDesc.colorAttachments.size()) {
      const auto& att = desc.targetDesc.colorAttachments[i];
      psoDesc.BlendState.RenderTarget[i].BlendEnable = att.blendEnabled ? TRUE : FALSE;
      psoDesc.BlendState.RenderTarget[i].SrcBlend = toD3D12Blend(att.srcRGBBlendFactor);
      psoDesc.BlendState.RenderTarget[i].DestBlend = toD3D12Blend(att.dstRGBBlendFactor);
      psoDesc.BlendState.RenderTarget[i].BlendOp = toD3D12BlendOp(att.rgbBlendOp);
      psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = toD3D12Blend(att.srcAlphaBlendFactor);
      psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = toD3D12Blend(att.dstAlphaBlendFactor);
      psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = toD3D12BlendOp(att.alphaBlendOp);

      // Convert IGL color write mask to D3D12
      UINT8 writeMask = 0;
      if (att.colorWriteMask & ColorWriteBitsRed)   writeMask |= D3D12_COLOR_WRITE_ENABLE_RED;
      if (att.colorWriteMask & ColorWriteBitsGreen) writeMask |= D3D12_COLOR_WRITE_ENABLE_GREEN;
      if (att.colorWriteMask & ColorWriteBitsBlue)  writeMask |= D3D12_COLOR_WRITE_ENABLE_BLUE;
      if (att.colorWriteMask & ColorWriteBitsAlpha) writeMask |= D3D12_COLOR_WRITE_ENABLE_ALPHA;
      psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = writeMask;

      IGL_D3D12_LOG_VERBOSE("  PSO RenderTarget[%u]: BlendEnable=%d, SrcBlend=%d, DstBlend=%d, WriteMask=0x%02X\n",
                   i, att.blendEnabled, psoDesc.BlendState.RenderTarget[i].SrcBlend,
                   psoDesc.BlendState.RenderTarget[i].DestBlend, writeMask);
    } else {
      // Default blend state for unused render targets
      psoDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
      psoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
      psoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
      psoDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
      psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
      psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
      psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
      psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    // Logic operations support (bitwise blend operations)
    // Query hardware support for logic operations
    // Note: LogicOp is currently disabled as IGL doesn't expose logic operation settings in RenderPipelineDesc
    // To enable in the future:
    // 1. Add LogicOp enum and logicOpEnabled/logicOp fields to RenderPipelineDesc::ColorAttachment
    // 2. Query D3D12_FEATURE_D3D12_OPTIONS.OutputMergerLogicOp at device initialization
    // 3. Set LogicOpEnable = TRUE and LogicOp = convertLogicOp(att.logicOp) when enabled
    psoDesc.BlendState.RenderTarget[i].LogicOpEnable = FALSE;
    psoDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
  }

  // Helper to convert IGL stencil operation to D3D12
  auto toD3D12StencilOp = [](StencilOperation op) {
    switch (op) {
      case StencilOperation::Keep: return D3D12_STENCIL_OP_KEEP;
      case StencilOperation::Zero: return D3D12_STENCIL_OP_ZERO;
      case StencilOperation::Replace: return D3D12_STENCIL_OP_REPLACE;
      case StencilOperation::IncrementClamp: return D3D12_STENCIL_OP_INCR_SAT;
      case StencilOperation::DecrementClamp: return D3D12_STENCIL_OP_DECR_SAT;
      case StencilOperation::Invert: return D3D12_STENCIL_OP_INVERT;
      case StencilOperation::IncrementWrap: return D3D12_STENCIL_OP_INCR;
      case StencilOperation::DecrementWrap: return D3D12_STENCIL_OP_DECR;
      default: return D3D12_STENCIL_OP_KEEP;
    }
  };

  // Helper to convert IGL compare function to D3D12
  auto toD3D12CompareFunc = [](CompareFunction func) {
    switch (func) {
      case CompareFunction::Never: return D3D12_COMPARISON_FUNC_NEVER;
      case CompareFunction::Less: return D3D12_COMPARISON_FUNC_LESS;
      case CompareFunction::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
      case CompareFunction::LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
      case CompareFunction::Greater: return D3D12_COMPARISON_FUNC_GREATER;
      case CompareFunction::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
      case CompareFunction::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
      case CompareFunction::AlwaysPass: return D3D12_COMPARISON_FUNC_ALWAYS;
      default: return D3D12_COMPARISON_FUNC_LESS;
    }
  };

  // Depth stencil state - check if we have a depth or stencil attachment
  const bool hasDepth = (desc.targetDesc.depthAttachmentFormat != TextureFormat::Invalid);
  const bool hasStencil = (desc.targetDesc.stencilAttachmentFormat != TextureFormat::Invalid);

  if (hasDepth) {
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    // Use LESS_EQUAL to allow Z=0 to pass when depth buffer is cleared to 0
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  } else {
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  }

  // Configure stencil state (can be used with or without depth)
  if (hasStencil) {
    // Note: In D3D12/IGL, stencil state is configured via DepthStencilState binding
    // For now, we set up basic stencil configuration in the PSO
    // Default: stencil disabled unless explicitly configured by DepthStencilState
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    psoDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

    // Front face stencil operations (defaults)
    psoDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    psoDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    psoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    // Back face stencil operations (defaults, same as front)
    psoDesc.DepthStencilState.BackFace = psoDesc.DepthStencilState.FrontFace;

    IGL_D3D12_LOG_VERBOSE("  PSO Stencil configured: format=%d\n", (int)desc.targetDesc.stencilAttachmentFormat);
  } else {
    psoDesc.DepthStencilState.StencilEnable = FALSE;
  }

  // Render target formats: support multiple render targets (MRT)
  if (!desc.targetDesc.colorAttachments.empty()) {
    const UINT n = static_cast<UINT>(std::min<size_t>(desc.targetDesc.colorAttachments.size(), D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT));
    psoDesc.NumRenderTargets = n;
    IGL_D3D12_LOG_VERBOSE("  PSO NumRenderTargets = %u (color attachments = %zu)\n", n, desc.targetDesc.colorAttachments.size());
    for (UINT i = 0; i < n; ++i) {
      // CRITICAL: Extract value to avoid MSVC debug iterator bounds check in function call
      const auto textureFormat = desc.targetDesc.colorAttachments[i].textureFormat;
      psoDesc.RTVFormats[i] = textureFormatToDXGIFormat(textureFormat);
      IGL_D3D12_LOG_VERBOSE("  PSO RTVFormats[%u] = %d (IGL format %d)\n", i, psoDesc.RTVFormats[i], textureFormat);
    }
  } else {
    psoDesc.NumRenderTargets = 0;
    IGL_D3D12_LOG_VERBOSE("  PSO NumRenderTargets = 0 (no color attachments)\n");
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
      psoDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
    }
  }
  if (desc.targetDesc.depthAttachmentFormat != TextureFormat::Invalid) {
    psoDesc.DSVFormat = textureFormatToDXGIFormat(desc.targetDesc.depthAttachmentFormat);
  } else {
    psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
  }

  // Sample settings
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.SampleDesc.Count = 1;
  psoDesc.SampleDesc.Quality = 0;  // Must be 0 for Count=1

  // Primitive topology - convert from IGL topology enum
  if (desc.topology == igl::PrimitiveType::Point) {
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    IGL_D3D12_LOG_VERBOSE("  Setting PSO topology type to POINT\n");
  } else if (desc.topology == igl::PrimitiveType::Line ||
             desc.topology == igl::PrimitiveType::LineStrip) {
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    IGL_D3D12_LOG_VERBOSE("  Setting PSO topology type to LINE\n");
  } else {
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    IGL_D3D12_LOG_VERBOSE("  Setting PSO topology type to TRIANGLE\n");
  }
  psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

  // Additional required fields
  psoDesc.NodeMask = 0;  // Single GPU operation
  psoDesc.CachedPSO.pCachedBlob = nullptr;
  psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
  psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

  // Input layout.
  std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
  std::vector<std::string> semanticNames; // Keep semantic name strings alive

  if (desc.vertexInputState) {
    // Convert IGL vertex input state to D3D12 input layout
    auto* d3d12VertexInput = static_cast<const VertexInputState*>(desc.vertexInputState.get());
    const auto& vertexDesc = d3d12VertexInput->getDesc();

    // Pre-reserve space to prevent reallocation (which would invalidate c_str() pointers)
    semanticNames.reserve(vertexDesc.numAttributes);

    IGL_D3D12_LOG_VERBOSE("  Processing vertex input state: %zu attributes\n", vertexDesc.numAttributes);
    for (size_t i = 0; i < vertexDesc.numAttributes; ++i) {
      const auto& attr = vertexDesc.attributes[i];
      IGL_D3D12_LOG_VERBOSE("    Attribute %zu: name='%s', format=%d, offset=%zu, bufferIndex=%u\n",
                   i, attr.name.c_str(), static_cast<int>(attr.format), attr.offset, attr.bufferIndex);

      // Map IGL attribute names to D3D12 HLSL semantic names
      // IMPORTANT: Semantic names must NOT end with numbers - use SemanticIndex field instead
      std::string semanticName;
      // Case-insensitive helpers
      auto toLower = [](std::string s){ for (auto& c : s) c = static_cast<char>(tolower(c)); return s; };
      const std::string nlow = toLower(attr.name);
      auto startsWith = [&](const char* p){ return nlow.rfind(p, 0) == 0; };
      auto contains = [&](const char* p){ return nlow.find(p) != std::string::npos; };

      if (startsWith("pos") || startsWith("position") || contains("position")) {
        semanticName = "POSITION";
      } else if (startsWith("col") || startsWith("color")) {
        semanticName = "COLOR";
      } else if (startsWith("st") || startsWith("uv") || startsWith("tex") || contains("texcoord") || startsWith("offset")) {
        semanticName = "TEXCOORD";
      } else if (startsWith("norm") || startsWith("normal")) {
        semanticName = "NORMAL";
      } else if (startsWith("tangent")) {
        semanticName = "TANGENT";
      } else {
        // Fallback: POSITION for first attribute, TEXCOORD for second, COLOR otherwise
        if (i == 0) semanticName = "POSITION";
        else if (i == 1) semanticName = "TEXCOORD";
        else semanticName = "COLOR";
      }
      semanticNames.push_back(semanticName);
      IGL_D3D12_LOG_VERBOSE("      Mapped '%s' -> '%s'\n", attr.name.c_str(), semanticName.c_str());

      D3D12_INPUT_ELEMENT_DESC element = {};
      element.SemanticName = semanticNames.back().c_str();
      element.SemanticIndex = 0;
      element.AlignedByteOffset = static_cast<UINT>(attr.offset);
      element.InputSlot = attr.bufferIndex;
      // Check if this buffer binding uses per-instance data
      // Note: inputBindings array may be sparse (bufferIndex >= numInputBindings), so check bounds with MAX
      const bool isInstanceData = (attr.bufferIndex < IGL_BUFFER_BINDINGS_MAX &&
                                    vertexDesc.inputBindings[attr.bufferIndex].sampleFunction ==
                                        VertexSampleFunction::Instance);
      element.InputSlotClass = isInstanceData ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                                               : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
      element.InstanceDataStepRate = isInstanceData ? 1 : 0;
      IGL_D3D12_LOG_VERBOSE("      bufferIndex=%u, isInstance=%d, sampleFunc=%d, InputSlotClass=%d, StepRate=%u\n",
                   attr.bufferIndex, isInstanceData,
                   (int)vertexDesc.inputBindings[attr.bufferIndex].sampleFunction,
                   (int)element.InputSlotClass, element.InstanceDataStepRate);

      // Convert IGL vertex format to DXGI format
      switch (attr.format) {
        case VertexAttributeFormat::Float1:
          element.Format = DXGI_FORMAT_R32_FLOAT;
          break;
        case VertexAttributeFormat::Float2:
          element.Format = DXGI_FORMAT_R32G32_FLOAT;
          break;
        case VertexAttributeFormat::Float3:
          element.Format = DXGI_FORMAT_R32G32B32_FLOAT;
          break;
        case VertexAttributeFormat::Float4:
          element.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
          break;
        case VertexAttributeFormat::Byte1:
          element.Format = DXGI_FORMAT_R8_UINT;
          break;
        case VertexAttributeFormat::Byte2:
          element.Format = DXGI_FORMAT_R8G8_UINT;
          break;
        case VertexAttributeFormat::Byte4:
          element.Format = DXGI_FORMAT_R8G8B8A8_UINT;
          break;
        case VertexAttributeFormat::UByte4Norm:
          element.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
          break;
        default:
          element.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // fallback
          break;
      }

      inputElements.push_back(element);
    }
  } else {
    // Default simple triangle layout: position (float3) + color (float4)
    inputElements.resize(2);
    inputElements[0] = {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
    inputElements[1] = {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
                        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
  }
  psoDesc.InputLayout = {inputElements.data(), static_cast<UINT>(inputElements.size())};

  IGL_D3D12_LOG_VERBOSE("  Final input layout: %u elements\n", static_cast<unsigned>(inputElements.size()));
  for (size_t i = 0; i < inputElements.size(); ++i) {
    IGL_D3D12_LOG_VERBOSE("    [%zu]: %s (index %u), format %d, slot %u, offset %u\n",
                 i, inputElements[i].SemanticName, inputElements[i].SemanticIndex,
                 static_cast<int>(inputElements[i].Format),
                 inputElements[i].InputSlot, inputElements[i].AlignedByteOffset);
  }

  // Use shader reflection to verify input signature matches input layout
  IGL_D3D12_LOG_VERBOSE("  Reflecting vertex shader to verify input signature...\n");
  igl::d3d12::ComPtr<ID3D12ShaderReflection> vsReflection;
  HRESULT hr = D3DReflect(vsBytecode.data(), vsBytecode.size(), IID_PPV_ARGS(vsReflection.GetAddressOf()));
  if (SUCCEEDED(hr)) {
    D3D12_SHADER_DESC shaderDesc = {};
    vsReflection->GetDesc(&shaderDesc);
    IGL_D3D12_LOG_VERBOSE("    Shader expects %u input parameters:\n", shaderDesc.InputParameters);
    for (UINT i = 0; i < shaderDesc.InputParameters; ++i) {
      D3D12_SIGNATURE_PARAMETER_DESC paramDesc = {};
      vsReflection->GetInputParameterDesc(i, &paramDesc);
      IGL_D3D12_LOG_VERBOSE("      [%u]: %s%u (semantic index %u), mask 0x%02X\n",
                   i, paramDesc.SemanticName, paramDesc.SemanticIndex,
                   paramDesc.SemanticIndex, paramDesc.Mask);
    }
  } else {
    IGL_D3D12_LOG_VERBOSE("    Shader reflection unavailable: 0x%08X (non-critical - pipeline will still be created)\n", static_cast<unsigned>(hr));
  }

  // PSO cache lookup (thread-safe with double-checked locking).
  const size_t psoHash = pipelineCache_.hashRenderPipelineDesc(desc);
  igl::d3d12::ComPtr<ID3D12PipelineState> pipelineState;

  // First check: Lock for cache lookup
  {
    std::lock_guard<std::mutex> lock(pipelineCache_.psoCacheMutex_);
    auto psoIt = pipelineCache_.graphicsPSOCache_.find(psoHash);
    if (psoIt != pipelineCache_.graphicsPSOCache_.end()) {
      // Cache hit - reuse existing PSO
      pipelineCache_.graphicsPSOCacheHits_++;
      pipelineState = psoIt->second;  // Assignment creates a ref-counted copy
      IGL_D3D12_LOG_VERBOSE("  [PSO CACHE HIT] Hash=0x%zx, hits=%zu, misses=%zu, hit rate=%.1f%%\n",
                   psoHash,
                   pipelineCache_.graphicsPSOCacheHits_,
                   pipelineCache_.graphicsPSOCacheMisses_,
                   100.0 * pipelineCache_.graphicsPSOCacheHits_ /
                       (pipelineCache_.graphicsPSOCacheHits_ +
                        pipelineCache_.graphicsPSOCacheMisses_));
      IGL_D3D12_LOG_VERBOSE("Device::createRenderPipeline() SUCCESS (CACHED) - PSO=%p, RootSig=%p\n",
                   pipelineState.Get(), rootSignature.Get());
      Result::setOk(outResult);
      // Create a copy of the root signature for the returned object
      igl::d3d12::ComPtr<ID3D12RootSignature> rootSigCopy = rootSignature;
      return std::make_shared<RenderPipelineState>(desc, std::move(pipelineState), std::move(rootSigCopy));
    }
  }

  // Cache miss - create new PSO outside lock (expensive operation)
  IGL_D3D12_LOG_VERBOSE("  [PSO CACHE MISS] Hash=0x%zx\n", psoHash);

  IGL_D3D12_LOG_VERBOSE("  Creating pipeline state (this may take a moment)...\n");
  hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf()));
  if (FAILED(hr)) {
    // Print debug layer messages if available
    igl::d3d12::ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf())))) {
      UINT64 numMessages = infoQueue->GetNumStoredMessages();
      IGL_D3D12_LOG_VERBOSE("  D3D12 Info Queue has %llu messages:\n", numMessages);
      for (UINT64 i = 0; i < numMessages; ++i) {
        SIZE_T messageLength = 0;
        infoQueue->GetMessage(i, nullptr, &messageLength);
        if (messageLength > 0) {
          auto message = (D3D12_MESSAGE*)malloc(messageLength);
          if (message && SUCCEEDED(infoQueue->GetMessage(i, message, &messageLength))) {
            const char* severityStr = "UNKNOWN";
            switch (message->Severity) {
              case D3D12_MESSAGE_SEVERITY_CORRUPTION: severityStr = "CORRUPTION"; break;
              case D3D12_MESSAGE_SEVERITY_ERROR: severityStr = "ERROR"; break;
              case D3D12_MESSAGE_SEVERITY_WARNING: severityStr = "WARNING"; break;
              case D3D12_MESSAGE_SEVERITY_INFO: severityStr = "INFO"; break;
              case D3D12_MESSAGE_SEVERITY_MESSAGE: severityStr = "MESSAGE"; break;
            }
            IGL_D3D12_LOG_VERBOSE("    [%s] %s\n", severityStr, message->pDescription);
          }
          free(message);
        }
      }
      infoQueue->ClearStoredMessages();
    }

    char errorMsg[512];
    snprintf(errorMsg, sizeof(errorMsg),
             "Failed to create pipeline state. HRESULT: 0x%08X\n"
             "  VS size: %zu, PS size: %zu\n"
             "  Input elements: %u\n"
             "  RT format: %d\n",
             static_cast<unsigned>(hr),
             psoDesc.VS.BytecodeLength,
             psoDesc.PS.BytecodeLength,
             psoDesc.InputLayout.NumElements,
             static_cast<int>(psoDesc.RTVFormats[0]));
    IGL_LOG_ERROR(errorMsg);
    Result::setResult(outResult, Result::Code::RuntimeError, errorMsg);
    return nullptr;
  }

  // E-011: Set debug name on PSO for better debugging in PIX/RenderDoc
  std::string psoName;
  if (desc.shaderStages->getVertexModule()) {
    psoName += desc.shaderStages->getVertexModule()->info().debugName;
  }
  if (desc.shaderStages->getFragmentModule()) {
    if (!psoName.empty()) {
      psoName += " + ";
    }
    psoName += desc.shaderStages->getFragmentModule()->info().debugName;
  }
  if (!psoName.empty()) {
    // Convert to wide string for D3D12 SetName API
    std::wstring wideName(psoName.begin(), psoName.end());
    pipelineState->SetName(wideName.c_str());
    IGL_D3D12_LOG_VERBOSE("  Set PSO debug name: %s\n", psoName.c_str());
  }

  // Second check: Lock for cache insertion with double-check.
  // Another thread may have created the PSO while we were creating ours
  {
    std::lock_guard<std::mutex> lock(pipelineCache_.psoCacheMutex_);
    auto psoIt = pipelineCache_.graphicsPSOCache_.find(psoHash);
    if (psoIt != pipelineCache_.graphicsPSOCache_.end()) {
      // Another thread beat us to it - use their PSO
      pipelineCache_.graphicsPSOCacheHits_++;
      pipelineState = psoIt->second;
      IGL_D3D12_LOG_VERBOSE("  [PSO DOUBLE-CHECK HIT] Another thread created PSO, using theirs. Hash=0x%zx\n", psoHash);
    } else {
      // We're the first to complete - cache our PSO
      pipelineCache_.graphicsPSOCacheMisses_++;
      pipelineCache_.graphicsPSOCache_[psoHash] = pipelineState;
      IGL_D3D12_LOG_VERBOSE("  [PSO CACHED] Hash=0x%zx, hits=%zu, misses=%zu\n",
                   psoHash,
                   pipelineCache_.graphicsPSOCacheHits_,
                   pipelineCache_.graphicsPSOCacheMisses_);
    }
  }

  IGL_D3D12_LOG_VERBOSE("Device::createRenderPipeline() SUCCESS - PSO=%p, RootSig=%p (hash=0x%zx)\n",
               pipelineState.Get(), rootSignature.Get(), psoHash);
  Result::setOk(outResult);
  return std::make_shared<RenderPipelineState>(desc, std::move(pipelineState), std::move(rootSignature));
}

  // Shader library and modules.
std::unique_ptr<IShaderLibrary> Device::createShaderLibrary(const ShaderLibraryDesc& desc,
                                                            Result* IGL_NULLABLE
                                                                outResult) const {
  IGL_D3D12_LOG_VERBOSE("Device::createShaderLibrary() - moduleInfo count=%zu, debugName='%s'\n",
               desc.moduleInfo.size(), desc.debugName.c_str());

  if (desc.moduleInfo.empty()) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "ShaderLibrary requires at least one module");
    return nullptr;
  }

  if (!desc.input.isValid()) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Invalid shader library input");
    return nullptr;
  }

  std::vector<std::shared_ptr<IShaderModule>> modules;
  modules.reserve(desc.moduleInfo.size());

  if (desc.input.type == ShaderInputType::Binary) {
    // Binary input: share the same bytecode across all modules (Metal-style)
    IGL_D3D12_LOG_VERBOSE("  Using binary input (%zu bytes) for all modules\n", desc.input.length);
    std::vector<uint8_t> bytecode(desc.input.length);
    std::memcpy(bytecode.data(), desc.input.data, desc.input.length);

    for (const auto& info : desc.moduleInfo) {
      // Create a copy of the bytecode for each module
      std::vector<uint8_t> moduleBytecode = bytecode;
      modules.push_back(std::make_shared<ShaderModule>(info, std::move(moduleBytecode)));
    }
  } else if (desc.input.type == ShaderInputType::String) {
    // String input: compile each module separately with its own entry point
    if (!desc.input.source || !*desc.input.source) {
      Result::setResult(outResult, Result::Code::ArgumentInvalid, "Shader library source is empty");
      return nullptr;
    }

    IGL_D3D12_LOG_VERBOSE("  Compiling %zu modules from string input\n", desc.moduleInfo.size());

    for (const auto& info : desc.moduleInfo) {
      // Create a ShaderModuleDesc for this specific module
      ShaderModuleDesc moduleDesc;
      moduleDesc.info = info;
      moduleDesc.input.type = ShaderInputType::String;
      moduleDesc.input.source = desc.input.source;
      moduleDesc.input.options = desc.input.options;
      moduleDesc.debugName = desc.debugName + "_" + info.entryPoint;

      Result moduleResult;
      auto module = createShaderModule(moduleDesc, &moduleResult);
      if (!moduleResult.isOk()) {
        IGL_LOG_ERROR("  Failed to compile module '%s': %s\n",
                      info.entryPoint.c_str(), moduleResult.message.c_str());
        Result::setResult(outResult, std::move(moduleResult));
        return nullptr;
      }
      modules.push_back(std::move(module));
    }
  } else {
    Result::setResult(outResult, Result::Code::Unsupported, "Unsupported shader library input type");
    return nullptr;
  }

  IGL_D3D12_LOG_VERBOSE("Device::createShaderLibrary() SUCCESS - created %zu modules\n", modules.size());
  Result::setOk(outResult);
  return std::make_unique<ShaderLibrary>(std::move(modules));
}

// Helper function: Compile HLSL shader using legacy FXC compiler (Shader Model 5.1)
// This is a fallback when DXC is unavailable or fails
namespace {
Result compileShaderFXC(
    const char* source,
    size_t sourceLength,
    const char* entryPoint,
    const char* target,
    const char* debugName,
    UINT compileFlags,
    std::vector<uint8_t>& outBytecode,
    std::string& outErrors) {

  IGL_D3D12_LOG_VERBOSE("FXC: Compiling shader '%s' with target '%s' (%zu bytes source)\n",
               debugName ? debugName : "unnamed",
               target,
               sourceLength);

  igl::d3d12::ComPtr<ID3DBlob> bytecode;
  igl::d3d12::ComPtr<ID3DBlob> errors;

  // D3DCompile is the legacy FXC compiler API
  // It's always available on Windows 10+ (via d3dcompiler_47.dll)
  HRESULT hr = D3DCompile(
      source,
      sourceLength,
      debugName,  // Source name (for error messages)
      nullptr,    // Defines
      D3D_COMPILE_STANDARD_FILE_INCLUDE,
      entryPoint,
      target,
      compileFlags,
      0,          // Effect flags (not used for shaders)
      bytecode.GetAddressOf(),
      errors.GetAddressOf()
  );

  if (FAILED(hr)) {
    std::string errorMsg = "FXC compilation failed";
    if (errors.Get() && errors->GetBufferSize() > 0) {
      outErrors = std::string(
          static_cast<const char*>(errors->GetBufferPointer()),
          errors->GetBufferSize()
      );
      errorMsg += ": " + outErrors;
      IGL_LOG_ERROR("FXC: %s\n", outErrors.c_str());
    }
    return Result(Result::Code::RuntimeError, errorMsg);
  }

  // Log warnings if any
  if (errors.Get() && errors->GetBufferSize() > 0) {
    outErrors = std::string(
        static_cast<const char*>(errors->GetBufferPointer()),
        errors->GetBufferSize()
    );
    IGL_D3D12_LOG_VERBOSE("FXC: Compilation warnings:\n%s\n", outErrors.c_str());
  }

  // Copy bytecode to output
  const uint8_t* data = static_cast<const uint8_t*>(bytecode->GetBufferPointer());
  size_t size = bytecode->GetBufferSize();
  outBytecode.assign(data, data + size);

  IGL_D3D12_LOG_VERBOSE("FXC: Compilation successful (%zu bytes bytecode)\n", size);

  return Result();
}
} // anonymous namespace

// Note: getShaderTarget() helper moved to Common.h for shared use.

std::shared_ptr<IShaderModule> Device::createShaderModule(const ShaderModuleDesc& desc,
                                                          Result* IGL_NULLABLE outResult) const {
  IGL_D3D12_LOG_VERBOSE("Device::createShaderModule() - stage=%d, entryPoint='%s', debugName='%s'\n",
               static_cast<int>(desc.info.stage), desc.info.entryPoint.c_str(), desc.debugName.c_str());

  if (!desc.input.isValid()) {
    IGL_LOG_ERROR("  Invalid shader input!\n");
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Invalid shader input");
    return nullptr;
  }

  std::vector<uint8_t> bytecode;

  if (desc.input.type == ShaderInputType::Binary) {
    // Binary input - copy bytecode directly
    IGL_D3D12_LOG_VERBOSE("  Using binary input (%zu bytes)\n", desc.input.length);
    bytecode.resize(desc.input.length);
    std::memcpy(bytecode.data(), desc.input.data, desc.input.length);
  } else if (desc.input.type == ShaderInputType::String) {
    // String input - compile HLSL at runtime using DXC (DirectX Shader Compiler)
    // For string input, use desc.input.source (not data) and calculate length
    if (!desc.input.source) {
      IGL_LOG_ERROR("  Shader source is null!\n");
      Result::setResult(outResult, Result::Code::ArgumentInvalid, "Shader source is null");
      return nullptr;
    }

    const size_t sourceLength = strlen(desc.input.source);
    IGL_D3D12_LOG_VERBOSE("  Compiling HLSL from string (%zu bytes) using DXC...\n", sourceLength);

    // Initialize DXC compiler thread-safely using std::call_once.
    static DXCCompiler dxcCompiler;
    static std::once_flag dxcInitFlag;
    static bool dxcAvailable = false;

    std::call_once(dxcInitFlag, []() {
      Result initResult = dxcCompiler.initialize();
      dxcAvailable = initResult.isOk();

      if (dxcAvailable) {
        IGL_D3D12_LOG_VERBOSE("  DXC compiler initialized successfully (Shader Model 6.0+ support)\n");
      } else {
        IGL_D3D12_LOG_VERBOSE("  DXC compiler initialization failed: %s\n", initResult.message.c_str());
        IGL_D3D12_LOG_VERBOSE("  Falling back to FXC (Shader Model 5.1)\n");
      }
    });

    // Determine shader target based on stage
    // Use SM 6.0 for DXC, SM 5.1 for FXC fallback
    const char* targetDXC = nullptr;
    const char* targetFXC = nullptr;
    switch (desc.info.stage) {
    case ShaderStage::Vertex:
      targetDXC = "vs_6_0";
      targetFXC = "vs_5_1";
      break;
    case ShaderStage::Fragment:
      targetDXC = "ps_6_0";
      targetFXC = "ps_5_1";
      break;
    case ShaderStage::Compute:
      targetDXC = "cs_6_0";
      targetFXC = "cs_5_1";
      break;
    default:
      IGL_LOG_ERROR("  Unsupported shader stage!\n");
      Result::setResult(outResult, Result::Code::ArgumentInvalid, "Unsupported shader stage");
      return nullptr;
    }

    // Compile flags (DXC uses D3DCOMPILE_* flags)
    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;

    // Enable shader debugging features
    #ifdef _DEBUG
      compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
      IGL_D3D12_LOG_VERBOSE("  DEBUG BUILD: Enabling shader debug info and disabling optimizations\n");
    #else
      // In release builds, still enable debug info for PIX captures unless explicitly disabled
      const char* disableDebugInfo = std::getenv("IGL_D3D12_DISABLE_SHADER_DEBUG");
      if (!disableDebugInfo || std::string(disableDebugInfo) != "1") {
        compileFlags |= D3DCOMPILE_DEBUG;
        IGL_D3D12_LOG_VERBOSE("  RELEASE BUILD: Enabling shader debug info (disable with IGL_D3D12_DISABLE_SHADER_DEBUG=1)\n");
      }
    #endif

    // Optional: Enable warnings as errors for stricter validation
    const char* warningsAsErrors = std::getenv("IGL_D3D12_SHADER_WARNINGS_AS_ERRORS");
    if (warningsAsErrors && std::string(warningsAsErrors) == "1") {
      compileFlags |= D3DCOMPILE_WARNINGS_ARE_ERRORS;
      IGL_D3D12_LOG_VERBOSE("  Treating shader warnings as errors\n");
    }

    // Try DXC first if available, fallback to FXC if DXC fails or unavailable
    std::string errors;
    Result compileResult;
    bool compiledWithDXC = false;

    if (dxcAvailable) {
      // Try DXC compilation (Shader Model 6.0)
      IGL_D3D12_LOG_VERBOSE("  Attempting DXC compilation (Shader Model 6.0)...\n");
      compileResult = dxcCompiler.compile(
          desc.input.source,
          sourceLength,
          desc.info.entryPoint.c_str(),
          targetDXC,
          desc.debugName.c_str(),
          compileFlags,
          bytecode,
          errors
      );

      if (compileResult.isOk()) {
        IGL_D3D12_LOG_VERBOSE("  DXC shader compiled successfully (%zu bytes DXIL bytecode)\n", bytecode.size());
        compiledWithDXC = true;
      } else {
        IGL_D3D12_LOG_VERBOSE("  DXC compilation failed: %s\n", compileResult.message.c_str());
        if (!errors.empty()) {
          IGL_D3D12_LOG_VERBOSE("  DXC errors: %s\n", errors.c_str());
        }
        IGL_D3D12_LOG_VERBOSE("  Falling back to FXC (Shader Model 5.1)...\n");
      }
    }

    // Use FXC if DXC is unavailable or failed
    if (!compiledWithDXC) {
      errors.clear();
      compileResult = compileShaderFXC(
          desc.input.source,
          sourceLength,
          desc.info.entryPoint.c_str(),
          targetFXC,
          desc.debugName.c_str(),
          compileFlags,
          bytecode,
          errors
      );

      if (!compileResult.isOk()) {
        // Both DXC and FXC failed - report error
        std::string errorMsg;
        const char* stageStr = "";
        switch (desc.info.stage) {
          case ShaderStage::Vertex: stageStr = "VERTEX"; break;
          case ShaderStage::Fragment: stageStr = "FRAGMENT/PIXEL"; break;
          case ShaderStage::Compute: stageStr = "COMPUTE"; break;
          default: stageStr = "UNKNOWN"; break;
        }

        errorMsg = "Shader compilation FAILED (both DXC and FXC)\n";
        errorMsg += "  Stage: " + std::string(stageStr) + "\n";
        errorMsg += "  Entry Point: " + desc.info.entryPoint + "\n";
        errorMsg += "  Target (FXC): " + std::string(targetFXC) + "\n";
        errorMsg += "  Debug Name: " + desc.debugName + "\n";

        if (!errors.empty()) {
          errorMsg += "\n=== FXC COMPILER ERRORS ===\n";
          errorMsg += errors;
          errorMsg += "\n===========================\n";
        } else {
          errorMsg += "  Error: " + compileResult.message + "\n";
        }

        IGL_LOG_ERROR("%s", errorMsg.c_str());
        Result::setResult(outResult, Result::Code::RuntimeError, errorMsg.c_str());
        return nullptr;
      }

      IGL_D3D12_LOG_VERBOSE("  FXC shader compiled successfully (%zu bytes bytecode)\n", bytecode.size());
    }
  } else {
    Result::setResult(outResult, Result::Code::Unsupported, "Unsupported shader input type");
    return nullptr;
  }

  // Create shader module with bytecode
  auto module = std::make_shared<ShaderModule>(desc.info, std::move(bytecode));

  // Create shader reflection from DXIL bytecode.
  // This allows runtime queries of shader resources, bindings, and constant buffers
  IGL_D3D12_LOG_VERBOSE("  Attempting to create shader reflection (bytecode size=%zu)...\n", module->getBytecode().size());
  if (!module->getBytecode().empty()) {
    // Create IDxcUtils for reflection
    igl::d3d12::ComPtr<IDxcUtils> dxcUtils;
    IGL_D3D12_LOG_VERBOSE("    Creating IDxcUtils for reflection...\n");
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxcUtils.GetAddressOf()));
    IGL_D3D12_LOG_VERBOSE("    DxcCreateInstance result: 0x%08X\n", hr);

    if (SUCCEEDED(hr)) {
      // Prepare buffer for reflection
      DxcBuffer reflectionBuffer = {};
      reflectionBuffer.Ptr = module->getBytecode().data();
      reflectionBuffer.Size = module->getBytecode().size();
      reflectionBuffer.Encoding = 0;

      // Create reflection interface
      igl::d3d12::ComPtr<ID3D12ShaderReflection> reflection;
      hr = dxcUtils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(reflection.GetAddressOf()));

      if (SUCCEEDED(hr)) {
        module->setReflection(reflection);
        IGL_D3D12_LOG_VERBOSE("  Shader reflection created successfully (DXIL reflection)\n");
      } else {
        IGL_D3D12_LOG_VERBOSE("  Failed to create shader reflection: 0x%08X (non-fatal)\n", hr);
      }
    } else {
      IGL_D3D12_LOG_VERBOSE("  Failed to create DXC utils for reflection: 0x%08X (non-fatal)\n", hr);
    }
  }

  Result::setOk(outResult);
  return module;
}

// Framebuffer
std::shared_ptr<IFramebuffer> Device::createFramebuffer(const FramebufferDesc& desc,
                                                        Result* IGL_NULLABLE outResult) {
  Result::setOk(outResult);
  return std::make_shared<Framebuffer>(desc);
}

// Capabilities
const IPlatformDevice& Device::getPlatformDevice() const noexcept {
  return *platformDevice_;
}

bool Device::hasFeature(DeviceFeatures feature) const {
  IGL_D3D12_LOG_VERBOSE("[D3D12] hasFeature query: %d\n", static_cast<int>(feature));
  switch (feature) {
    // Expected true in tests (non-OpenGL branch)
    case DeviceFeatures::CopyBuffer:
    case DeviceFeatures::DrawInstanced:
    case DeviceFeatures::DrawFirstIndexFirstVertex: // D3D12 DrawIndexedInstanced supports first index/vertex
    case DeviceFeatures::SRGB:
    case DeviceFeatures::SRGBSwapchain:
    case DeviceFeatures::UniformBlocks:
    case DeviceFeatures::StandardDerivative: // ddx/ddy available in HLSL
    case DeviceFeatures::TextureFloat:
    case DeviceFeatures::TextureHalfFloat:
    case DeviceFeatures::ReadWriteFramebuffer:
    case DeviceFeatures::TextureNotPot:
    case DeviceFeatures::ShaderTextureLod:
    case DeviceFeatures::ExplicitBinding:
    case DeviceFeatures::MapBufferRange: // UPLOAD/READBACK buffers support mapping
    case DeviceFeatures::ShaderLibrary: // Support shader libraries in D3D12
    case DeviceFeatures::Texture3D: // D3D12 supports 3D textures (DIMENSION_TEXTURE3D).
    case DeviceFeatures::TexturePartialMipChain: // D3D12 supports partial mip chains via custom SRVs.
    case DeviceFeatures::TextureViews: // D3D12 supports createTextureView() via shared resources.
      return true;
    case DeviceFeatures::MultipleRenderTargets:
      return true; // D3D12 supports up to 8 simultaneous render targets.
    case DeviceFeatures::Compute:
      return true; // Compute shaders now supported with compute pipeline and dispatch
    case DeviceFeatures::Texture2DArray:
      IGL_D3D12_LOG_VERBOSE("[D3D12] hasFeature(Texture2DArray) returning TRUE\n");
      return true; // D3D12 supports 2D texture arrays via DepthOrArraySize in D3D12_RESOURCE_DESC
    case DeviceFeatures::PushConstants:
      return true; // Implemented via root constants at parameter 0 (shader register b2)
    case DeviceFeatures::SRGBWriteControl:
    case DeviceFeatures::TextureArrayExt:
    case DeviceFeatures::TextureExternalImage:
    case DeviceFeatures::Multiview:
    case DeviceFeatures::BindBytes:  // Not supported - use uniform buffers instead
    case DeviceFeatures::BindUniform:
    case DeviceFeatures::BufferRing:
    case DeviceFeatures::BufferNoCopy:
    case DeviceFeatures::BufferDeviceAddress:
    case DeviceFeatures::ShaderTextureLodExt:
    case DeviceFeatures::StandardDerivativeExt:
    case DeviceFeatures::SamplerMinMaxLod:
    case DeviceFeatures::DrawIndexedIndirect:
    case DeviceFeatures::ExplicitBindingExt:
    case DeviceFeatures::TextureFormatRG:
    case DeviceFeatures::ValidationLayersEnabled:
    case DeviceFeatures::ExternalMemoryObjects:
      return false;
    default:
      return false;
  }
}

bool Device::hasRequirement(DeviceRequirement /*requirement*/) const {
  return false;
}

bool Device::getFeatureLimits(DeviceFeatureLimits featureLimits, size_t& result) const {
  // Compile-time validation: IGL constant must not exceed D3D12 API limit
  static_assert(IGL_VERTEX_ATTRIBUTES_MAX <= D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
                "IGL_VERTEX_ATTRIBUTES_MAX exceeds D3D12 vertex input limit");

  switch (featureLimits) {
    case DeviceFeatureLimits::BufferAlignment:
      // D3D12 buffer alignment requirements vary by buffer type:
      // - Constant buffers: 256 bytes (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
      // - Storage buffers: 4 bytes (see ShaderStorageBufferOffsetAlignment)
      // - Vertex/index buffers: 4 bytes (DWORD alignment)
      // This returns the most restrictive alignment (constant buffers).
      // See: https://learn.microsoft.com/en-us/windows/win32/direct3d12/constants
      result = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT; // 256 bytes
      return true;

    case DeviceFeatureLimits::BufferNoCopyAlignment:
      // D3D12 doesn't support no-copy buffers in the same way as Metal
      result = 0;
      return true;

    case DeviceFeatureLimits::MaxBindBytesBytes:
      // bind-bytes (like Metal setVertexBytes) not supported on D3D12
      result = 0;
      return true;

    case DeviceFeatureLimits::MaxCubeMapDimension:
      // D3D12 cube map dimension limits (Feature Level 11_0+: 16384)
      result = 16384; // D3D12_REQ_TEXTURECUBE_DIMENSION
      return true;

    case DeviceFeatureLimits::MaxFragmentUniformVectors:
      // D3D12 allows 64KB constant buffers, each vec4 is 16 bytes
      // 64KB / 16 bytes = 4096 vec4s
      result = 4096;
      return true;

    case DeviceFeatureLimits::MaxMultisampleCount: {
      // Query the maximum MSAA sample count supported by the device.
      // Test common sample counts (1, 2, 4, 8, 16) for RGBA8 (most widely supported format).
      // This provides a conservative estimate; actual support varies by format.
      // Applications should use getMaxMSAASamplesForFormat() for format-specific queries.
      auto* device = ctx_->getDevice();
      if (!device) {
        result = 1;  // No MSAA support if device unavailable
        return false;
      }

      // Use RGBA8 as reference format (most widely supported)
      const DXGI_FORMAT referenceFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

      // Test sample counts in descending order: 16, 8, 4, 2, 1
      const uint32_t testCounts[] = {16, 8, 4, 2, 1};

      for (uint32_t sampleCount : testCounts) {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msqLevels = {};
        msqLevels.Format = referenceFormat;
        msqLevels.SampleCount = sampleCount;
        msqLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

        HRESULT hr = device->CheckFeatureSupport(
            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
            &msqLevels,
            sizeof(msqLevels));

        if (SUCCEEDED(hr) && msqLevels.NumQualityLevels > 0) {
          result = sampleCount;
          return true;
        }
      }

      // Fallback to 1x (no MSAA)
      result = 1;
      return true;
    }

    case DeviceFeatureLimits::MaxPushConstantBytes:
      // D3D12 root constants: each root constant is 4 bytes (DWORD)
      // D3D12 root signature limit is 64 DWORDs total, but not all for constants
      // Conservative limit: 256 bytes (64 DWORDs)
      result = 256;
      return true;

    case DeviceFeatureLimits::MaxTextureDimension1D2D:
      // D3D12 Feature Level 11_0+: 16384 for 1D and 2D textures
      // Feature Level 12+: still 16384
      result = 16384; // D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION
      return true;

    case DeviceFeatureLimits::MaxStorageBufferBytes:
      // D3D12 structured buffer max size: 128MB (2^27 bytes)
      // UAV structured buffer limit
      result = 128 * 1024 * 1024; // 128 MB
      return true;

    case DeviceFeatureLimits::MaxUniformBufferBytes:
      // D3D12 constant buffer size limit: 64KB (65536 bytes)
      result = 64 * 1024; // D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16
      return true;

    case DeviceFeatureLimits::MaxVertexUniformVectors:
      // Same as fragment uniform vectors for D3D12
      // 64KB / 16 bytes per vec4 = 4096 vec4s
      result = 4096;
      return true;

    case DeviceFeatureLimits::PushConstantsAlignment:
      // Root constants are aligned to DWORD (4 bytes)
      result = 4;
      return true;

    case DeviceFeatureLimits::ShaderStorageBufferOffsetAlignment:
      // D3D12 storage buffer (UAV/structured buffer) alignment.
      // D3D12 structured buffers require 4-byte (DWORD) alignment, unlike constant buffers (256 bytes)
      // This matches Vulkan's typical minStorageBufferOffsetAlignment (often 16-64 bytes, device-dependent)
      // See: https://learn.microsoft.com/en-us/windows/win32/direct3d12/alignment
      result = 4;
      return true;

    case DeviceFeatureLimits::MaxTextureDimension3D:
      // D3D12 3D texture dimension limits (Feature Level 11_0+: 2048)
      // Feature Level 10_0+: 2048
      result = 2048; // D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION
      return true;

    case DeviceFeatureLimits::MaxComputeWorkGroupSizeX:
      // D3D12 compute shader thread group limits
      result = D3D12_CS_THREAD_GROUP_MAX_X; // 1024
      return true;

    case DeviceFeatureLimits::MaxComputeWorkGroupSizeY:
      // D3D12 compute shader thread group limits
      result = D3D12_CS_THREAD_GROUP_MAX_Y; // 1024
      return true;

    case DeviceFeatureLimits::MaxComputeWorkGroupSizeZ:
      // D3D12 compute shader thread group limits
      result = D3D12_CS_THREAD_GROUP_MAX_Z; // 64
      return true;

    case DeviceFeatureLimits::MaxComputeWorkGroupInvocations:
      // D3D12 max threads per thread group
      result = D3D12_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP; // 1024
      return true;

    case DeviceFeatureLimits::MaxVertexInputAttributes:
      // D3D12 max vertex input slots (32 per D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
      result = D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; // 32
      IGL_DEBUG_ASSERT(IGL_VERTEX_ATTRIBUTES_MAX <= result,
                       "IGL_VERTEX_ATTRIBUTES_MAX exceeds D3D12 reported limit");
      return true;

    case DeviceFeatureLimits::MaxColorAttachments:
      // D3D12 max simultaneous render targets
      result = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; // 8
      return true;

    // Descriptor heap size limits chosen for cross-platform compatibility.
    case DeviceFeatureLimits::MaxDescriptorHeapCbvSrvUav:
      // D3D12 shader-visible CBV/SRV/UAV descriptor heap size
      // Hardware limit: 1,000,000+ descriptors
      // Current implementation uses 4096 descriptors (see DescriptorHeapManager::Sizes)
      // This reports the configured limit, not the hardware maximum
      result = 4096;
      return true;

    case DeviceFeatureLimits::MaxDescriptorHeapSamplers:
      // D3D12 shader-visible sampler descriptor heap size
      // Hardware limit: 2048 descriptors (D3D12 spec limit for sampler heaps)
      // Current implementation uses 2048 descriptors (see DescriptorHeapManager::Sizes)
      result = 2048;
      return true;

    case DeviceFeatureLimits::MaxDescriptorHeapRtvs:
      // D3D12 CPU-visible RTV descriptor heap size
      // Hardware limit: 16,384 descriptors
      // Current implementation uses 256 descriptors (see DescriptorHeapManager::Sizes)
      result = 256;
      return true;

    case DeviceFeatureLimits::MaxDescriptorHeapDsvs:
      // D3D12 CPU-visible DSV descriptor heap size
      // Hardware limit: 16,384 descriptors
      // Current implementation uses 128 descriptors (see DescriptorHeapManager::Sizes)
      result = 128;
      return true;
  }

  // Should never reach here - all cases handled
  result = 0;
  return false;
}


ICapabilities::TextureFormatCapabilities Device::getTextureFormatCapabilities(TextureFormat format) const {
  using CapBits = ICapabilities::TextureFormatCapabilityBits;
  uint8_t caps = 0;

  // Depth formats: guarantee they are sampleable in shaders for tests
  switch (format) {
  case TextureFormat::Z_UNorm16:
  case TextureFormat::Z_UNorm24:
  case TextureFormat::Z_UNorm32:
  case TextureFormat::S8_UInt_Z24_UNorm:
  case TextureFormat::S8_UInt_Z32_UNorm:
    caps |= CapBits::Sampled;
    return caps;
  default:
    break;
  }

  // D3D12 does not support 3-channel RGB formats natively - they are mapped to RGBA formats
  // However, 3-channel formats cannot be used as render targets because:
  // 1. RGB_F16/RGB_F32 map to RGBA equivalents, but D3D12 expects RGBA data layout for RT
  // 2. Rendering to these formats would require alpha channel handling that IGL doesn't expose
  // 3. Other backends (OpenGL, Metal) also don't support RGB formats as render targets
  // See also: OpenGL's DeviceFeatureSet.cpp line 1271 "RGB floating point textures are NOT renderable"
  const bool isThreeChannelRgbFormat =
      format == TextureFormat::RGB_F16 ||
      format == TextureFormat::RGB_F32;

  auto* dev = ctx_->getDevice();
  if (!dev) {
    return 0;
  }

  const DXGI_FORMAT dxgi = textureFormatToDXGIFormat(format);
  if (dxgi == DXGI_FORMAT_UNKNOWN) {
    return 0;
  }

  D3D12_FEATURE_DATA_FORMAT_SUPPORT fs = {};
  fs.Format = dxgi;
  if (FAILED(dev->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &fs, sizeof(fs)))) {
    return 0;
  }

  const auto s1 = fs.Support1;
  const auto s2 = fs.Support2;

  const auto props = TextureFormatProperties::fromTextureFormat(format);

  // Enhanced D3D12 format capability mapping.
  // Map D3D12_FORMAT_SUPPORT1 flags to IGL capabilities

  // Sampled: Can be used with texture sampling instructions
  if (s1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) {
    caps |= CapBits::Sampled;
  }

  // SampledFiltered: Supports linear filtering (only for non-integer color formats)
  // Also check D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE_COMPARISON for depth formats
  if (props.hasColor() && !props.isInteger()) {
    if (s1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) {
      caps |= CapBits::SampledFiltered;
    }
  } else if (props.hasDepth() || props.hasStencil()) {
    // Depth formats: check for comparison filtering support
    if (s1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE_COMPARISON) {
      caps |= CapBits::SampledFiltered;
    }
  }

  // Attachment: Can be used as render target or depth/stencil attachment
  // Also consider D3D12_FORMAT_SUPPORT1_BLENDABLE and D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RENDERTARGET
  // Don't report Attachment capability for 3-channel RGB formats even if D3D12 reports the
  // underlying RGBA format as renderable - using them as render targets causes device removal
  if (!isThreeChannelRgbFormat) {
    if ((s1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) || (s1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)) {
      caps |= CapBits::Attachment;
    }
  }

  // Storage: Can be used with unordered access (UAV)
  // Check for typed UAV load/store, or atomic operations
  // Enhanced UAV capability detection.
  const bool hasUAVTypedOps = (s2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) &&
                              (s2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
  const bool hasUAVAtomicOps = (s2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_ADD) ||
                               (s2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_BITWISE_OPS) ||
                               (s2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_COMPARE_STORE_OR_COMPARE_EXCHANGE) ||
                               (s2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE) ||
                               (s2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_SIGNED_MIN_OR_MAX) ||
                               (s2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_UNSIGNED_MIN_OR_MAX);

  if (hasFeature(DeviceFeatures::Compute) && (hasUAVTypedOps || hasUAVAtomicOps)) {
    caps |= CapBits::Storage;
  }

  // SampledAttachment: Can be both sampled and used as attachment
  if ((caps & CapBits::Sampled) && (caps & CapBits::Attachment)) {
    caps |= CapBits::SampledAttachment;
  }

#if IGL_DEBUG || defined(IGL_FORCE_ENABLE_LOGS)
  // Debug logging for unmapped D3D12 capabilities.
  // This helps identify format capabilities that D3D12 supports but IGL doesn't expose
  uint32_t unmappedS1 = 0;
  uint32_t unmappedS2 = 0;

  // Check unmapped D3D12_FORMAT_SUPPORT1 flags
  const uint32_t mappedS1 = D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE |
                            D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE_COMPARISON |
                            D3D12_FORMAT_SUPPORT1_RENDER_TARGET |
                            D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL |
                            D3D12_FORMAT_SUPPORT1_BLENDABLE |
                            D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RENDERTARGET;
  unmappedS1 = s1 & ~mappedS1;

  // Check unmapped D3D12_FORMAT_SUPPORT2 flags
  const uint32_t mappedS2 = D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD |
                            D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE |
                            D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_ADD |
                            D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_BITWISE_OPS |
                            D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_COMPARE_STORE_OR_COMPARE_EXCHANGE |
                            D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE |
                            D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_SIGNED_MIN_OR_MAX |
                            D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_UNSIGNED_MIN_OR_MAX;
  unmappedS2 = s2 & ~mappedS2;

  if (unmappedS1 != 0 || unmappedS2 != 0) {
    IGL_D3D12_LOG_VERBOSE("Format %d (DXGI %d) has unmapped D3D12 capabilities:\n",
                 static_cast<int>(format), static_cast<int>(dxgi));
    if (unmappedS1 != 0) {
      IGL_D3D12_LOG_VERBOSE("  Support1 unmapped flags: 0x%08X\n", unmappedS1);
      // Log specific unmapped flags that might be useful
      // Note: Some flags may not be defined in older Windows SDK versions
      const uint32_t MIP_AUTOGEN = 0x800;           // D3D12_FORMAT_SUPPORT1_MIP_AUTOGEN
      const uint32_t MULTISAMPLE_RESOLVE = 0x40;    // D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE
      const uint32_t MULTISAMPLE_LOAD = 0x100000;   // D3D12_FORMAT_SUPPORT1_MULTISAMPLE_LOAD

      if (unmappedS1 & MIP_AUTOGEN) {
        IGL_D3D12_LOG_VERBOSE("    - MIP_AUTOGEN (0x800)\n");
      }
      if (unmappedS1 & MULTISAMPLE_RESOLVE) {
        IGL_D3D12_LOG_VERBOSE("    - MULTISAMPLE_RESOLVE (0x40)\n");
      }
      if (unmappedS1 & MULTISAMPLE_LOAD) {
        IGL_D3D12_LOG_VERBOSE("    - MULTISAMPLE_LOAD (0x100000)\n");
      }
    }
    if (unmappedS2 != 0) {
      IGL_D3D12_LOG_VERBOSE("  Support2 unmapped flags: 0x%08X\n", unmappedS2);
      const uint32_t OUTPUT_MERGER_LOGIC_OP = 0x2;  // D3D12_FORMAT_SUPPORT2_OUTPUT_MERGER_LOGIC_OP
      if (unmappedS2 & OUTPUT_MERGER_LOGIC_OP) {
        IGL_D3D12_LOG_VERBOSE("    - OUTPUT_MERGER_LOGIC_OP (0x2)\n");
      }
    }
  }
#endif

  return caps;
}

ShaderVersion Device::getShaderVersion() const {
  // Report HLSL SM 6.0 if DXC is available; otherwise SM 5.0 (D3DCompile fallback)
  bool dxcAvailable = false;
#if IGL_PLATFORM_WINDOWS
  HMODULE h = GetModuleHandleA("dxcompiler.dll");
  if (!h) {
    h = LoadLibraryA("dxcompiler.dll");
  }
  if (h) {
    FARPROC proc = GetProcAddress(h, "DxcCreateInstance");
    dxcAvailable = (proc != nullptr);
  }
#endif
  if (dxcAvailable) {
    return ShaderVersion{ShaderFamily::Hlsl, 6, 0, 0};
  }
  return ShaderVersion{ShaderFamily::Hlsl, 5, 0, 0};
}

BackendVersion Device::getBackendVersion() const {
  // Query highest supported feature level to report backend version
  auto* dev = ctx_->getDevice();
  if (!dev) {
    return BackendVersion{BackendFlavor::D3D12, 0, 0};
  }

  static const D3D_FEATURE_LEVEL kLevels[] = {
      D3D_FEATURE_LEVEL_12_2,
      D3D_FEATURE_LEVEL_12_1,
      D3D_FEATURE_LEVEL_12_0,
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
  };
  D3D12_FEATURE_DATA_FEATURE_LEVELS fls = {};
  fls.NumFeatureLevels = static_cast<UINT>(sizeof(kLevels) / sizeof(kLevels[0]));
  fls.pFeatureLevelsRequested = kLevels;
  fls.MaxSupportedFeatureLevel = D3D_FEATURE_LEVEL_11_0;

  if (SUCCEEDED(dev->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &fls, sizeof(fls)))) {
    switch (fls.MaxSupportedFeatureLevel) {
    case D3D_FEATURE_LEVEL_12_2:
      return BackendVersion{BackendFlavor::D3D12, 12, 2};
    case D3D_FEATURE_LEVEL_12_1:
      return BackendVersion{BackendFlavor::D3D12, 12, 1};
    case D3D_FEATURE_LEVEL_12_0:
      return BackendVersion{BackendFlavor::D3D12, 12, 0};
    case D3D_FEATURE_LEVEL_11_1:
      return BackendVersion{BackendFlavor::D3D12, 11, 1};
    case D3D_FEATURE_LEVEL_11_0:
    default:
      return BackendVersion{BackendFlavor::D3D12, 11, 0};
    }
  }

  // Fallback if CheckFeatureSupport fails
  return BackendVersion{BackendFlavor::D3D12, 11, 0};
}

BackendType Device::getBackendType() const {
  return BackendType::D3D12;
}

// Get sampler cache statistics for telemetry and debugging.
SamplerCacheStats Device::getSamplerCacheStats() const {
  return samplerCache_.getStats();
}

// Query maximum MSAA sample count for a specific format.
uint32_t Device::getMaxMSAASamplesForFormat(TextureFormat format) const {
  auto* device = ctx_->getDevice();
  if (!device) {
    return 1;
  }

  // Convert IGL format to DXGI format
  const DXGI_FORMAT dxgiFormat = textureFormatToDXGIFormat(format);
  if (dxgiFormat == DXGI_FORMAT_UNKNOWN) {
    IGL_LOG_ERROR("Device::getMaxMSAASamplesForFormat: Unknown format %d\n", static_cast<int>(format));
    return 1;
  }

  // Test sample counts in descending order: 16, 8, 4, 2, 1
  const uint32_t testCounts[] = {16, 8, 4, 2, 1};

  for (uint32_t sampleCount : testCounts) {
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msqLevels = {};
    msqLevels.Format = dxgiFormat;
    msqLevels.SampleCount = sampleCount;
    msqLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

    HRESULT hr = device->CheckFeatureSupport(
        D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
        &msqLevels,
        sizeof(msqLevels));

    if (SUCCEEDED(hr) && msqLevels.NumQualityLevels > 0) {
      return sampleCount;
    }
  }

  return 1;  // No MSAA support
}

void Device::processCompletedUploads() {
  allocatorPool_.processCompletedUploads();
}

Result Device::waitForUploadFence(UINT64 fenceValue) const {
  return allocatorPool_.waitForUploadFence(*this, fenceValue);
}

void Device::trackUploadBuffer(igl::d3d12::ComPtr<ID3D12Resource> buffer, UINT64 fenceValue) {
  allocatorPool_.trackUploadBuffer(std::move(buffer), fenceValue);
}

igl::d3d12::ComPtr<ID3D12CommandAllocator> Device::getUploadCommandAllocator() {
  return allocatorPool_.getUploadCommandAllocator(*ctx_);
}

void Device::returnUploadCommandAllocator(igl::d3d12::ComPtr<ID3D12CommandAllocator> allocator,
                                          UINT64 fenceValue) {
  allocatorPool_.returnUploadCommandAllocator(std::move(allocator), fenceValue);
}

size_t Device::getCurrentDrawCount() const {
  return telemetry_.getDrawCount();
}

size_t Device::getShaderCompilationCount() const {
  return telemetry_.getShaderCompilationCount();
}

} // namespace igl::d3d12

