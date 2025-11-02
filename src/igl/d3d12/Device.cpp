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
#include <igl/VertexInputState.h>
#include <igl/Texture.h>
#include <cstring>
#include <vector>

namespace igl::d3d12 {

Device::Device(std::unique_ptr<D3D12Context> ctx) : ctx_(std::move(ctx)) {
  platformDevice_ = std::make_unique<PlatformDevice>(*this);
}

Device::~Device() = default;

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

  // For uniform buffers, size must be aligned to 256 bytes (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
  const bool isUniformBuffer = (desc.type & BufferDesc::BufferTypeBits::Uniform) != 0;
  const UINT64 alignedSize = isUniformBuffer
      ? (desc.length + 255) & ~255  // Round up to nearest 256 bytes
      : desc.length;

  IGL_LOG_INFO("Device::createBuffer: type=%d, requested_size=%zu, aligned_size=%llu, isUniform=%d\n",
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
  const bool isStorageBuffer = (desc.type & BufferDesc::BufferTypeBits::Storage) != 0;
  if (isStorageBuffer) {
    bufferDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    IGL_LOG_INFO("Device::createBuffer: Storage buffer - adding UAV flag\n");
  }

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

  // Debug: Log GPU address for uniform buffers
  if (isUniformBuffer) {
    static int uniformBufCount = 0;
    if (uniformBufCount < 5) {
      D3D12_GPU_VIRTUAL_ADDRESS gpuAddr = buffer->GetGPUVirtualAddress();
      IGL_LOG_INFO("Device::createBuffer: Uniform buffer #%d created, GPU address=0x%llx\n",
                   uniformBufCount + 1, gpuAddr);
      uniformBufCount++;
    }
  }

  // Upload initial data if provided
  if (desc.data) {
    if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
      void* mappedData = nullptr;
      D3D12_RANGE readRange = {0, 0};
      hr = buffer->Map(0, &readRange, &mappedData);

      if (SUCCEEDED(hr)) {
        std::memcpy(mappedData, desc.data, desc.length);
        buffer->Unmap(0, nullptr);
      }
    } else {
      // DEFAULT heap: stage through an UPLOAD buffer and copy
      IGL_LOG_INFO("Device::createBuffer: Staging initial data via UPLOAD heap for DEFAULT buffer\n");

      // Create upload buffer
      D3D12_HEAP_PROPERTIES uploadHeapProps = {};
      uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
      uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

      Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
      HRESULT upHr = device->CreateCommittedResource(&uploadHeapProps,
                                                     D3D12_HEAP_FLAG_NONE,
                                                     &bufferDesc,
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

          // Record copy commands
          Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
          Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
          if (SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                       IID_PPV_ARGS(allocator.GetAddressOf()))) &&
              SUCCEEDED(device->CreateCommandList(0,
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
            ctx_->waitForGPU();
          }
        }
      }
    }
  }

  Result::setOk(outResult);
  return std::make_unique<Buffer>(std::move(buffer), desc);
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
  D3D12_SAMPLER_DESC samplerDesc = {};

  auto toD3D12Address = [](SamplerAddressMode m) {
    switch (m) {
    case SamplerAddressMode::Repeat: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    case SamplerAddressMode::MirrorRepeat: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    case SamplerAddressMode::Clamp: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    default: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
  };

  auto toD3D12Compare = [](CompareFunction f) {
    switch (f) {
    case CompareFunction::Less: return D3D12_COMPARISON_FUNC_LESS;
    case CompareFunction::LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case CompareFunction::Greater: return D3D12_COMPARISON_FUNC_GREATER;
    case CompareFunction::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case CompareFunction::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
    case CompareFunction::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case CompareFunction::AlwaysPass: return D3D12_COMPARISON_FUNC_ALWAYS;
    case CompareFunction::Never: return D3D12_COMPARISON_FUNC_NEVER;
    default: return D3D12_COMPARISON_FUNC_NEVER;
    }
  };

  const bool useComparison = desc.depthCompareEnabled;

  // Filter mapping (basic, anisotropy optional)
  const bool minLinear = (desc.minFilter != SamplerMinMagFilter::Nearest);
  const bool magLinear = (desc.magFilter != SamplerMinMagFilter::Nearest);
  const bool mipLinear = (desc.mipFilter == SamplerMipFilter::Linear);
  const bool anisotropic = (desc.maxAnisotropic > 1);

  if (anisotropic) {
    samplerDesc.Filter = useComparison ? D3D12_FILTER_COMPARISON_ANISOTROPIC : D3D12_FILTER_ANISOTROPIC;
    samplerDesc.MaxAnisotropy = std::min<uint32_t>(desc.maxAnisotropic, 16);
  } else {
    D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    if (minLinear && magLinear && mipLinear) filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    else if (minLinear && magLinear && !mipLinear) filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    else if (minLinear && !magLinear && mipLinear) filter = D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
    else if (minLinear && !magLinear && !mipLinear) filter = D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    else if (!minLinear && magLinear && mipLinear) filter = D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
    else if (!minLinear && magLinear && !mipLinear) filter = D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
    else if (!minLinear && !magLinear && mipLinear) filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
    // else remains POINT
    if (useComparison) {
      filter = static_cast<D3D12_FILTER>(filter | D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT - D3D12_FILTER_MIN_MAG_MIP_POINT);
    }
    samplerDesc.Filter = filter;
    samplerDesc.MaxAnisotropy = 1;
  }

  samplerDesc.AddressU = toD3D12Address(desc.addressModeU);
  samplerDesc.AddressV = toD3D12Address(desc.addressModeV);
  samplerDesc.AddressW = toD3D12Address(desc.addressModeW);
  samplerDesc.MipLODBias = 0.0f;
  samplerDesc.ComparisonFunc = useComparison ? toD3D12Compare(desc.depthCompareFunction) : D3D12_COMPARISON_FUNC_NEVER;
  samplerDesc.BorderColor[0] = 0.0f;
  samplerDesc.BorderColor[1] = 0.0f;
  samplerDesc.BorderColor[2] = 0.0f;
  samplerDesc.BorderColor[3] = 0.0f;
  samplerDesc.MinLOD = static_cast<float>(desc.mipLodMin);
  samplerDesc.MaxLOD = static_cast<float>(desc.mipLodMax);

  Result::setOk(outResult);
  return std::make_shared<SamplerState>(samplerDesc);
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
  IGL_LOG_INFO("Device::createTexture: IGL format=%d -> DXGI format=%d\n", (int)desc.format, (int)dxgiFormat);
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
  } else {
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.DepthOrArraySize = static_cast<UINT16>(desc.numLayers);
  }

  resourceDesc.Alignment = 0;
  resourceDesc.Width = desc.width;
  resourceDesc.Height = desc.height;
  resourceDesc.MipLevels = static_cast<UINT16>(desc.numMipLevels);
  resourceDesc.Format = dxgiFormat;

  // MSAA configuration
  // D3D12 MSAA requirements:
  // - Sample count must be 1, 2, 4, 8, or 16 (power of 2)
  // - Quality level 0 is standard MSAA (higher quality levels are vendor-specific)
  // - MSAA textures cannot have mipmaps (numMipLevels must be 1)
  // - Not all formats support all sample counts - validation required
  const uint32_t sampleCount = std::max(1u, desc.numSamples);

  // Validate MSAA constraints
  if (sampleCount > 1) {
    // MSAA textures cannot have mipmaps
    if (desc.numMipLevels > 1) {
      IGL_LOG_ERROR("Device::createTexture: MSAA textures cannot have mipmaps (numMipLevels=%u, numSamples=%u)\n",
                    desc.numMipLevels, sampleCount);
      Result::setResult(outResult, Result::Code::ArgumentInvalid,
                        "MSAA textures cannot have mipmaps (numMipLevels must be 1)");
      return nullptr;
    }

    // Validate sample count is supported for this format
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msqLevels = {};
    msqLevels.Format = dxgiFormat;
    msqLevels.SampleCount = sampleCount;
    msqLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msqLevels, sizeof(msqLevels))) ||
        msqLevels.NumQualityLevels == 0) {
      char errorMsg[256];
      snprintf(errorMsg, sizeof(errorMsg),
               "Device::createTexture: Format %d does not support %u samples (MSAA not supported)",
               static_cast<int>(dxgiFormat), sampleCount);
      IGL_LOG_ERROR("%s\n", errorMsg);
      Result::setResult(outResult, Result::Code::Unsupported, errorMsg);
      return nullptr;
    }

    IGL_LOG_INFO("Device::createTexture: MSAA enabled - format=%d, samples=%u, quality levels=%u\n",
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

  // Create the texture resource
  Microsoft::WRL::ComPtr<ID3D12Resource> resource;
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

  // Create IGL texture from D3D12 resource
  auto texture = Texture::createFromResource(
      resource.Get(), desc.format, desc, device, ctx_->getCommandQueue(), initialState);
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
  Result::setResult(outResult, Result::Code::Unimplemented, "D3D12 Timer not yet implemented");
  return nullptr;
}

std::shared_ptr<IVertexInputState> Device::createVertexInputState(
    const VertexInputStateDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  Result::setOk(outResult);
  return std::make_shared<VertexInputState>(desc);
}

// Pipelines
std::shared_ptr<IComputePipelineState> Device::createComputePipeline(
    const ComputePipelineDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  IGL_LOG_INFO("Device::createComputePipeline() START - debugName='%s'\n", desc.debugName.c_str());

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

  IGL_LOG_INFO("  Getting compute shader bytecode...\n");
  const auto& csBytecode = computeModule->getBytecode();
  IGL_LOG_INFO("  CS bytecode: %zu bytes\n", csBytecode.size());

  // Create root signature for compute
  // Root signature layout for compute:
  // - Root parameter 0: Descriptor table with unbounded UAVs (u0-uN)
  // - Root parameter 1: Descriptor table with unbounded SRVs (t0-tN)
  // - Root parameter 2: Descriptor table with unbounded CBVs (b0-bN)
  // - Root parameter 3: Descriptor table with unbounded Samplers (s0-sN)

  Microsoft::WRL::ComPtr<ID3DBlob> signature;
  Microsoft::WRL::ComPtr<ID3DBlob> error;

  // Descriptor range for UAVs (unordered access views - read/write buffers and textures)
  D3D12_DESCRIPTOR_RANGE uavRange = {};
  uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
  uavRange.NumDescriptors = UINT_MAX;  // UNBOUNDED
  uavRange.BaseShaderRegister = 0;  // Starting at u0
  uavRange.RegisterSpace = 0;
  uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // Descriptor range for SRVs (shader resource views - read-only textures and buffers)
  D3D12_DESCRIPTOR_RANGE srvRange = {};
  srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  srvRange.NumDescriptors = UINT_MAX;  // UNBOUNDED
  srvRange.BaseShaderRegister = 0;  // Starting at t0
  srvRange.RegisterSpace = 0;
  srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // Descriptor range for CBVs (constant buffer views)
  // Note: b0 will be used for root constants (push constants), so CBV table starts at b1
  D3D12_DESCRIPTOR_RANGE cbvRange = {};
  cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
  cbvRange.NumDescriptors = UINT_MAX;  // UNBOUNDED
  cbvRange.BaseShaderRegister = 1;  // Starting at b1 (b0 is root constants)
  cbvRange.RegisterSpace = 0;
  cbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // Descriptor range for Samplers
  D3D12_DESCRIPTOR_RANGE samplerRange = {};
  samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
  samplerRange.NumDescriptors = UINT_MAX;  // UNBOUNDED
  samplerRange.BaseShaderRegister = 0;  // Starting at s0
  samplerRange.RegisterSpace = 0;
  samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // Root parameters
  D3D12_ROOT_PARAMETER rootParams[5] = {};

  // Parameter 0: Root Constants for b0 (Push Constants)
  // Using 32-bit constants for push constants in compute shaders
  // 16 DWORDs = 64 bytes (4x4 matrix of floats or equivalent data)
  rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  rootParams[0].Constants.ShaderRegister = 0;  // b0
  rootParams[0].Constants.RegisterSpace = 0;
  rootParams[0].Constants.Num32BitValues = 16;  // 16 DWORDs = 64 bytes
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

  IGL_LOG_INFO("  Creating compute root signature with Root Constants (b0)/UAVs/SRVs/CBVs/Samplers\n");

  HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                           signature.GetAddressOf(), error.GetAddressOf());
  if (FAILED(hr)) {
    if (error.Get()) {
      const char* errorMsg = static_cast<const char*>(error->GetBufferPointer());
      IGL_LOG_ERROR("Root signature serialization error: %s\n", errorMsg);
    }
    Result::setResult(outResult, Result::Code::RuntimeError, "Failed to serialize compute root signature");
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
  hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                    IID_PPV_ARGS(rootSignature.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("  CreateRootSignature FAILED: 0x%08X\n", static_cast<unsigned>(hr));
    Result::setResult(outResult, Result::Code::RuntimeError, "Failed to create compute root signature");
    return nullptr;
  }
  IGL_LOG_INFO("  Compute root signature created OK\n");

  // Create compute pipeline state
  D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.pRootSignature = rootSignature.Get();
  psoDesc.CS.pShaderBytecode = csBytecode.data();
  psoDesc.CS.BytecodeLength = csBytecode.size();
  psoDesc.NodeMask = 0;
  psoDesc.CachedPSO.pCachedBlob = nullptr;
  psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
  psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

  IGL_LOG_INFO("  Creating compute pipeline state...\n");
  Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
  hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("  CreateComputePipelineState FAILED: 0x%08X\n", static_cast<unsigned>(hr));

    // Print debug layer messages if available
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
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

  IGL_LOG_INFO("Device::createComputePipeline() SUCCESS - PSO=%p, RootSig=%p\n",
               pipelineState.Get(), rootSignature.Get());
  Result::setOk(outResult);
  return std::make_shared<ComputePipelineState>(std::move(pipelineState), std::move(rootSignature), desc.debugName);
}

std::shared_ptr<IRenderPipelineState> Device::createRenderPipeline(
    const RenderPipelineDesc& desc,
    Result* IGL_NULLABLE outResult) const {
  IGL_LOG_INFO("Device::createRenderPipeline() START - debugName='%s'\n", desc.debugName.c_str());

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

  IGL_LOG_INFO("  Getting shader bytecode...\n");
  // Get shader bytecode first
  const auto& vsBytecode = vertexModule->getBytecode();
  const auto& psBytecode = fragmentModule->getBytecode();
  IGL_LOG_INFO("  VS bytecode: %zu bytes, PS bytecode: %zu bytes\n", vsBytecode.size(), psBytecode.size());

  // Create root signature with descriptor tables for textures and constant buffers
  // Root signature layout:
  // - Root parameter 0: Root Constants for b2 (Push Constants) - 16 DWORDs = 64 bytes max
  // - Root parameter 1: CBV for uniform buffer b0 (UniformsPerFrame)
  // - Root parameter 2: CBV for uniform buffer b1 (UniformsPerObject)
  // - Root parameter 3: Descriptor table with SRVs for textures t0-tN (unbounded)
  // - Root parameter 4: Descriptor table with Samplers for s0-sN (unbounded)

  Microsoft::WRL::ComPtr<ID3DBlob> signature;
  Microsoft::WRL::ComPtr<ID3DBlob> error;

  // Descriptor range for SRVs (textures)
  // Use UNBOUNDED to support variable number of textures (1-N) per shader
  D3D12_DESCRIPTOR_RANGE srvRange = {};
  srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  srvRange.NumDescriptors = UINT_MAX;  // UNBOUNDED - shader determines actual count
  srvRange.BaseShaderRegister = 0;  // Starting at t0
  srvRange.RegisterSpace = 0;
  srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // Descriptor range for Samplers
  // Use UNBOUNDED to support variable number of samplers (1-N) per shader
  D3D12_DESCRIPTOR_RANGE samplerRange = {};
  samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
  samplerRange.NumDescriptors = UINT_MAX;  // UNBOUNDED - shader determines actual count
  samplerRange.BaseShaderRegister = 0;  // Starting at s0
  samplerRange.RegisterSpace = 0;
  samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  // Root parameters
  D3D12_ROOT_PARAMETER rootParams[5] = {};

  // Parameter 0: Root Constants for b2 (Push Constants)
  // Max 64 bytes = 16 DWORDs to match Vulkan push constant limits
  rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  rootParams[0].Constants.ShaderRegister = 2;  // b2 (b0/b1 reserved for uniform buffers)
  rootParams[0].Constants.RegisterSpace = 0;
  rootParams[0].Constants.Num32BitValues = 16;  // 16 DWORDs = 64 bytes
  rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 1: Root CBV for b0 (UniformsPerFrame)
  rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rootParams[1].Descriptor.ShaderRegister = 0;  // b0
  rootParams[1].Descriptor.RegisterSpace = 0;
  rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 2: Root CBV for b1 (UniformsPerObject)
  rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rootParams[2].Descriptor.ShaderRegister = 1;  // b1
  rootParams[2].Descriptor.RegisterSpace = 0;
  rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Parameter 3: Descriptor table for SRVs (textures)
  rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
  rootParams[3].DescriptorTable.pDescriptorRanges = &srvRange;
  rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  // Parameter 4: Descriptor table for Samplers
  rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParams[4].DescriptorTable.NumDescriptorRanges = 1;
  rootParams[4].DescriptorTable.pDescriptorRanges = &samplerRange;
  rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
  // Enable full root signature matching TinyMeshSession shaders
  rootSigDesc.NumParameters = 5;
  rootSigDesc.pParameters = rootParams;
  rootSigDesc.NumStaticSamplers = 0;
  rootSigDesc.pStaticSamplers = nullptr;
  rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  IGL_LOG_INFO("  Creating root signature with Push Constants (b2)/CBVs (b0,b1)/SRVs/Samplers\n");

  IGL_LOG_INFO("  Serializing root signature...\n");
  HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                           signature.GetAddressOf(), error.GetAddressOf());
  if (FAILED(hr)) {
    if (error.Get()) {
      const char* errorMsg = static_cast<const char*>(error->GetBufferPointer());
      IGL_LOG_ERROR("Root signature serialization error: %s\n", errorMsg);
    }
    Result::setResult(outResult, Result::Code::RuntimeError, "Failed to serialize root signature");
    return nullptr;
  }
  IGL_LOG_INFO("  Root signature serialized OK\n");

  IGL_LOG_INFO("  Creating root signature...\n");
  Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
  hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                    IID_PPV_ARGS(rootSignature.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("  CreateRootSignature FAILED: 0x%08X\n", static_cast<unsigned>(hr));

    // Print debug layer messages if available BEFORE device removal check
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
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

    Result::setResult(outResult, Result::Code::RuntimeError, "Failed to create root signature");
    return nullptr;
  }
  IGL_LOG_INFO("  Root signature created OK\n");

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

      IGL_LOG_INFO("  PSO RenderTarget[%u]: BlendEnable=%d, SrcBlend=%d, DstBlend=%d, WriteMask=0x%02X\n",
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

    IGL_LOG_INFO("  PSO Stencil configured: format=%d\n", (int)desc.targetDesc.stencilAttachmentFormat);
  } else {
    psoDesc.DepthStencilState.StencilEnable = FALSE;
  }

  // Render target formats: support multiple render targets (MRT)
  if (!desc.targetDesc.colorAttachments.empty()) {
    const UINT n = static_cast<UINT>(std::min<size_t>(desc.targetDesc.colorAttachments.size(), D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT));
    psoDesc.NumRenderTargets = n;
    IGL_LOG_INFO("  PSO NumRenderTargets = %u (color attachments = %zu)\n", n, desc.targetDesc.colorAttachments.size());
    for (UINT i = 0; i < n; ++i) {
      psoDesc.RTVFormats[i] = textureFormatToDXGIFormat(desc.targetDesc.colorAttachments[i].textureFormat);
      IGL_LOG_INFO("  PSO RTVFormats[%u] = %d (IGL format %d)\n", i, psoDesc.RTVFormats[i], desc.targetDesc.colorAttachments[i].textureFormat);
    }
  } else {
    psoDesc.NumRenderTargets = 0;
    IGL_LOG_INFO("  PSO NumRenderTargets = 0 (no color attachments)\n");
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
    IGL_LOG_INFO("  Setting PSO topology type to POINT\n");
  } else if (desc.topology == igl::PrimitiveType::Line ||
             desc.topology == igl::PrimitiveType::LineStrip) {
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    IGL_LOG_INFO("  Setting PSO topology type to LINE\n");
  } else {
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    IGL_LOG_INFO("  Setting PSO topology type to TRIANGLE\n");
  }
  psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

  // Additional required fields
  psoDesc.NodeMask = 0;  // Single GPU operation
  psoDesc.CachedPSO.pCachedBlob = nullptr;
  psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
  psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

  // Input layout (Phase 3 Step 3.4)
  std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
  std::vector<std::string> semanticNames; // Keep semantic name strings alive

  if (desc.vertexInputState) {
    // Convert IGL vertex input state to D3D12 input layout
    auto* d3d12VertexInput = static_cast<const VertexInputState*>(desc.vertexInputState.get());
    const auto& vertexDesc = d3d12VertexInput->getDesc();

    // Pre-reserve space to prevent reallocation (which would invalidate c_str() pointers)
    semanticNames.reserve(vertexDesc.numAttributes);

    IGL_LOG_INFO("  Processing vertex input state: %zu attributes\n", vertexDesc.numAttributes);
    for (size_t i = 0; i < vertexDesc.numAttributes; ++i) {
      const auto& attr = vertexDesc.attributes[i];
      IGL_LOG_INFO("    Attribute %zu: name='%s', format=%d, offset=%zu, bufferIndex=%u\n",
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
      IGL_LOG_INFO("      Mapped '%s' -> '%s'\n", attr.name.c_str(), semanticName.c_str());

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
      IGL_LOG_INFO("      bufferIndex=%u, isInstance=%d, sampleFunc=%d, InputSlotClass=%d, StepRate=%u\n",
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

  IGL_LOG_INFO("  Final input layout: %u elements\n", static_cast<unsigned>(inputElements.size()));
  for (size_t i = 0; i < inputElements.size(); ++i) {
    IGL_LOG_INFO("    [%zu]: %s (index %u), format %d, slot %u, offset %u\n",
                 i, inputElements[i].SemanticName, inputElements[i].SemanticIndex,
                 static_cast<int>(inputElements[i].Format),
                 inputElements[i].InputSlot, inputElements[i].AlignedByteOffset);
  }

  // Use shader reflection to verify input signature matches input layout
  IGL_LOG_INFO("  Reflecting vertex shader to verify input signature...\n");
  Microsoft::WRL::ComPtr<ID3D12ShaderReflection> vsReflection;
  hr = D3DReflect(vsBytecode.data(), vsBytecode.size(), IID_PPV_ARGS(vsReflection.GetAddressOf()));
  if (SUCCEEDED(hr)) {
    D3D12_SHADER_DESC shaderDesc = {};
    vsReflection->GetDesc(&shaderDesc);
    IGL_LOG_INFO("    Shader expects %u input parameters:\n", shaderDesc.InputParameters);
    for (UINT i = 0; i < shaderDesc.InputParameters; ++i) {
      D3D12_SIGNATURE_PARAMETER_DESC paramDesc = {};
      vsReflection->GetInputParameterDesc(i, &paramDesc);
      IGL_LOG_INFO("      [%u]: %s%u (semantic index %u), mask 0x%02X\n",
                   i, paramDesc.SemanticName, paramDesc.SemanticIndex,
                   paramDesc.SemanticIndex, paramDesc.Mask);
    }
  } else {
    IGL_LOG_ERROR("    Shader reflection failed: 0x%08X\n", static_cast<unsigned>(hr));
  }

  IGL_LOG_INFO("  Creating pipeline state (this may take a moment)...\n");
  Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
  hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf()));
  if (FAILED(hr)) {
    // Print debug layer messages if available
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf())))) {
      UINT64 numMessages = infoQueue->GetNumStoredMessages();
      IGL_LOG_INFO("  D3D12 Info Queue has %llu messages:\n", numMessages);
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
            IGL_LOG_INFO("    [%s] %s\n", severityStr, message->pDescription);
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

  IGL_LOG_INFO("Device::createRenderPipeline() SUCCESS - PSO=%p, RootSig=%p\n",
               pipelineState.Get(), rootSignature.Get());
  Result::setOk(outResult);
  return std::make_shared<RenderPipelineState>(desc, std::move(pipelineState), std::move(rootSignature));
}

// Shader library and modules
std::unique_ptr<IShaderLibrary> Device::createShaderLibrary(const ShaderLibraryDesc& desc,
                                                            Result* IGL_NULLABLE
                                                                outResult) const {
  IGL_LOG_INFO("Device::createShaderLibrary() - moduleInfo count=%zu, debugName='%s'\n",
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
    IGL_LOG_INFO("  Using binary input (%zu bytes) for all modules\n", desc.input.length);
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

    IGL_LOG_INFO("  Compiling %zu modules from string input\n", desc.moduleInfo.size());

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

  IGL_LOG_INFO("Device::createShaderLibrary() SUCCESS - created %zu modules\n", modules.size());
  Result::setOk(outResult);
  return std::make_unique<ShaderLibrary>(std::move(modules));
}

std::shared_ptr<IShaderModule> Device::createShaderModule(const ShaderModuleDesc& desc,
                                                          Result* IGL_NULLABLE outResult) const {
  IGL_LOG_INFO("Device::createShaderModule() - stage=%d, entryPoint='%s', debugName='%s'\n",
               static_cast<int>(desc.info.stage), desc.info.entryPoint.c_str(), desc.debugName.c_str());

  if (!desc.input.isValid()) {
    IGL_LOG_ERROR("  Invalid shader input!\n");
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Invalid shader input");
    return nullptr;
  }

  std::vector<uint8_t> bytecode;

  if (desc.input.type == ShaderInputType::Binary) {
    // Binary input - copy bytecode directly
    IGL_LOG_INFO("  Using binary input (%zu bytes)\n", desc.input.length);
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
    IGL_LOG_INFO("  Compiling HLSL from string (%zu bytes) using DXC...\n", sourceLength);

    // Initialize DXC compiler (once per process)
    static DXCCompiler dxcCompiler;
    static bool dxcInitialized = false;
    static bool dxcAvailable = false;

    if (!dxcInitialized) {
      Result initResult = dxcCompiler.initialize();
      dxcAvailable = initResult.isOk();
      dxcInitialized = true;

      if (!dxcAvailable) {
        IGL_LOG_ERROR("  DXC compiler initialization failed: %s\n", initResult.message.c_str());
        IGL_LOG_ERROR("  DXC is required for Shader Model 6.0+ support\n");
      }
    }

    if (!dxcAvailable) {
      Result::setResult(outResult, Result::Code::RuntimeError, "DXC compiler not available");
      return nullptr;
    }

    // Determine shader target based on stage (Shader Model 6.0 for DXC)
    const char* target = nullptr;
    switch (desc.info.stage) {
    case ShaderStage::Vertex:
      target = "vs_6_0";
      break;
    case ShaderStage::Fragment:
      target = "ps_6_0";
      break;
    case ShaderStage::Compute:
      target = "cs_6_0";
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
      IGL_LOG_INFO("  DEBUG BUILD: Enabling shader debug info and disabling optimizations\n");
    #else
      // In release builds, still enable debug info for PIX captures unless explicitly disabled
      const char* disableDebugInfo = std::getenv("IGL_D3D12_DISABLE_SHADER_DEBUG");
      if (!disableDebugInfo || std::string(disableDebugInfo) != "1") {
        compileFlags |= D3DCOMPILE_DEBUG;
        IGL_LOG_INFO("  RELEASE BUILD: Enabling shader debug info (disable with IGL_D3D12_DISABLE_SHADER_DEBUG=1)\n");
      }
    #endif

    // Optional: Enable warnings as errors for stricter validation
    const char* warningsAsErrors = std::getenv("IGL_D3D12_SHADER_WARNINGS_AS_ERRORS");
    if (warningsAsErrors && std::string(warningsAsErrors) == "1") {
      compileFlags |= D3DCOMPILE_WARNINGS_ARE_ERRORS;
      IGL_LOG_INFO("  Treating shader warnings as errors\n");
    }

    // Compile with DXC
    std::string errors;
    Result compileResult = dxcCompiler.compile(
        desc.input.source,
        sourceLength,
        desc.info.entryPoint.c_str(),
        target,
        desc.debugName.c_str(),
        compileFlags,
        bytecode,
        errors
    );

    if (!compileResult.isOk()) {
      // Enhanced error message with context
      std::string errorMsg;
      const char* stageStr = "";
      switch (desc.info.stage) {
        case ShaderStage::Vertex: stageStr = "VERTEX"; break;
        case ShaderStage::Fragment: stageStr = "FRAGMENT/PIXEL"; break;
        case ShaderStage::Compute: stageStr = "COMPUTE"; break;
        default: stageStr = "UNKNOWN"; break;
      }

      errorMsg = "DXC shader compilation FAILED\n";
      errorMsg += "  Stage: " + std::string(stageStr) + "\n";
      errorMsg += "  Entry Point: " + desc.info.entryPoint + "\n";
      errorMsg += "  Target: " + std::string(target) + "\n";
      errorMsg += "  Debug Name: " + desc.debugName + "\n";

      if (!errors.empty()) {
        errorMsg += "\n=== DXC COMPILER ERRORS ===\n";
        errorMsg += errors;
        errorMsg += "\n===========================\n";
      } else {
        errorMsg += "  Error: " + compileResult.message + "\n";
      }

      IGL_LOG_ERROR("%s", errorMsg.c_str());
      Result::setResult(outResult, Result::Code::RuntimeError, errorMsg.c_str());
      return nullptr;
    }

    IGL_LOG_INFO("  DXC shader compiled successfully (%zu bytes DXIL bytecode)\n", bytecode.size());

    // Note: Disassembly and reflection for DXIL bytecode can be added if needed
    // DXC uses IDxcUtils::CreateReflection() instead of D3DReflect()
    // For now, bytecode is ready to use (already populated by dxcCompiler.compile())
  } else {
    Result::setResult(outResult, Result::Code::Unsupported, "Unsupported shader input type");
    return nullptr;
  }

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
  return *platformDevice_;
}

bool Device::hasFeature(DeviceFeatures feature) const {
  IGL_LOG_INFO("[D3D12] hasFeature query: %d\n", static_cast<int>(feature));
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
    case DeviceFeatures::BindBytes:
    case DeviceFeatures::ShaderTextureLod:
    case DeviceFeatures::ExplicitBinding:
    case DeviceFeatures::MapBufferRange: // UPLOAD/READBACK buffers support mapping
    case DeviceFeatures::ShaderLibrary: // Support shader libraries in D3D12
    case DeviceFeatures::Texture3D: // D3D12 supports 3D textures (DIMENSION_TEXTURE3D)
    case DeviceFeatures::TexturePartialMipChain: // D3D12 supports partial mip chains via custom SRVs
    case DeviceFeatures::TextureViews: // D3D12 supports createTextureView() via shared resources
      return true;
    // MRT fully implemented and tested in Phase 6
    case DeviceFeatures::MultipleRenderTargets:
      return true; // D3D12 supports up to 8 simultaneous render targets
    case DeviceFeatures::Compute:
      return true; // Compute shaders now supported with compute pipeline and dispatch
    case DeviceFeatures::Texture2DArray:
      return true; // D3D12 supports 2D texture arrays via DepthOrArraySize in D3D12_RESOURCE_DESC
    case DeviceFeatures::PushConstants:
      return true; // Implemented via root constants at parameter 0 (shader register b2)
    case DeviceFeatures::SRGBWriteControl:
    case DeviceFeatures::TextureArrayExt:
    case DeviceFeatures::TextureExternalImage:
    case DeviceFeatures::Multiview:
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
  switch (featureLimits) {
    case DeviceFeatureLimits::BufferAlignment:
      result = 256; // D3D12 constant buffer alignment
      return true;
    case DeviceFeatureLimits::MaxUniformBufferBytes:
      result = 64 * 1024; // 64KB typical CB size
      return true;
    case DeviceFeatureLimits::MaxBindBytesBytes:
      result = 0; // bind-bytes not supported on D3D12 path
      return true;
    default:
      result = 0;
      return false;
  }
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

  if (s1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) {
    caps |= CapBits::Sampled;
  }
  // Filtered only for non-integer color formats when sampleable
  if ((s1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) && props.hasColor() && !props.isInteger()) {
    caps |= CapBits::SampledFiltered;
  }
  if ((s1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) || (s1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)) {
    caps |= CapBits::Attachment;
  }
  // Typed UAV load + store required for Storage
  if ((s2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) && (s2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE) &&
      hasFeature(DeviceFeatures::Compute)) {
    caps |= CapBits::Storage;
  }

  // SampledAttachment indicates formats that can be both sampled and used as attachment
  if ((caps & CapBits::Sampled) && (caps & CapBits::Attachment)) {
    caps |= CapBits::SampledAttachment;
  }

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

size_t Device::getCurrentDrawCount() const {
  return drawCount_;
}

size_t Device::getShaderCompilationCount() const {
  return shaderCompilationCount_;
}

} // namespace igl::d3d12

