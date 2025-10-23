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
#include <igl/d3d12/VertexInputState.h>
#include <igl/d3d12/DepthStencilState.h>
#include <igl/d3d12/SamplerState.h>
#include <igl/d3d12/Texture.h>
#include <igl/d3d12/PlatformDevice.h>
#include <igl/VertexInputState.h>
#include <cstring>
#include <vector>

namespace igl::d3d12 {

Device::Device(std::unique_ptr<D3D12Context> ctx) : ctx_(std::move(ctx)) {
  platformDevice_ = std::make_unique<PlatformDevice>(*this);
}

Device::~Device() = default;

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
