/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/Texture.h>
#include <d3dcompiler.h>

namespace igl::d3d12 {

std::shared_ptr<Texture> Texture::createFromResource(ID3D12Resource* resource,
                                                       TextureFormat format,
                                                       const TextureDesc& desc,
                                                       ID3D12Device* device,
                                                       ID3D12CommandQueue* queue) {
  if (!resource) {
    IGL_LOG_ERROR("Texture::createFromResource - resource is NULL!\n");
    return nullptr;
  }

  auto texture = std::make_shared<Texture>(format);

  // Attach the resource to ComPtr (takes ownership, AddRefs)
  resource->AddRef();
  texture->resource_.Attach(resource);

  texture->device_ = device;
  texture->queue_ = queue;
  texture->format_ = format;
  texture->dimensions_ = Dimensions{desc.width, desc.height, desc.depth};
  texture->type_ = desc.type;
  texture->numLayers_ = desc.numLayers;
  texture->numMipLevels_ = desc.numMipLevels;
  texture->samples_ = desc.numSamples;
  texture->usage_ = desc.usage;

  IGL_LOG_INFO("Texture::createFromResource - SUCCESS: %dx%d format=%d\n",
               desc.width, desc.height, (int)format);

  return texture;
}

Result Texture::upload(const TextureRangeDesc& range,
                      const void* data,
                      size_t bytesPerRow) const {
  IGL_LOG_INFO("Texture::upload() - START: %dx%d\n", range.width, range.height);

  if (!device_ || !queue_ || !resource_.Get()) {
    IGL_LOG_ERROR("Texture::upload() - FAILED: device, queue, or resource not available\n");
    return Result(Result::Code::RuntimeError, "Device, queue, or resource not available for upload");
  }

  if (!data) {
    IGL_LOG_ERROR("Texture::upload() - FAILED: data is null\n");
    return Result(Result::Code::ArgumentInvalid, "Upload data is null");
  }

  IGL_LOG_INFO("Texture::upload() - Proceeding with upload\n");

  // Calculate dimensions and data size
  const uint32_t width = range.width > 0 ? range.width : dimensions_.width;
  const uint32_t height = range.height > 0 ? range.height : dimensions_.height;

  // Calculate bytes per row if not provided
  if (bytesPerRow == 0) {
    const auto props = TextureFormatProperties::fromTextureFormat(format_);
    const size_t bpp = std::max<uint8_t>(props.bytesPerBlock, 1);
    bytesPerRow = static_cast<size_t>(width) * bpp;
  }

  // Get the resource description to calculate required size
  D3D12_RESOURCE_DESC resourceDesc = resource_->GetDesc();

  // Get the layout information for the subresource
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
  UINT numRows;
  UINT64 rowSizeInBytes;
  UINT64 totalBytes;

  device_->GetCopyableFootprints(&resourceDesc, range.mipLevel, 1, 0,
                                  &layout, &numRows, &rowSizeInBytes, &totalBytes);

  // Use footprint returned by GetCopyableFootprints for this subresource without overriding
  // Limit copy region via srcBox and row count below

  // Create staging buffer (upload heap)
  D3D12_HEAP_PROPERTIES uploadHeapProps = {};
  uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
  uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  uploadHeapProps.CreationNodeMask = 1;
  uploadHeapProps.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC stagingDesc = {};
  stagingDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  stagingDesc.Alignment = 0;
  stagingDesc.Width = totalBytes;
  stagingDesc.Height = 1;
  stagingDesc.DepthOrArraySize = 1;
  stagingDesc.MipLevels = 1;
  stagingDesc.Format = DXGI_FORMAT_UNKNOWN;
  stagingDesc.SampleDesc.Count = 1;
  stagingDesc.SampleDesc.Quality = 0;
  stagingDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  stagingDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  Microsoft::WRL::ComPtr<ID3D12Resource> stagingBuffer;
  HRESULT hr = device_->CreateCommittedResource(
      &uploadHeapProps,
      D3D12_HEAP_FLAG_NONE,
      &stagingDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(stagingBuffer.GetAddressOf()));

  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create staging buffer for texture upload");
  }

  // Map and copy data to staging buffer
  void* mappedData = nullptr;
  hr = stagingBuffer->Map(0, nullptr, &mappedData);
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to map staging buffer");
  }

  // Copy data row by row (limit to requested region height)
  const uint8_t* srcData = static_cast<const uint8_t*>(data);
  uint8_t* dstData = static_cast<uint8_t*>(mappedData) + layout.Offset;
  const UINT rowsToCopy = (range.height > 0) ? std::min<UINT>(range.height, numRows) : numRows;
  const size_t copyBytes = std::min(static_cast<size_t>(rowSizeInBytes), bytesPerRow);
  for (UINT row = 0; row < rowsToCopy; ++row) {
    memcpy(dstData + row * layout.Footprint.RowPitch,
           srcData + row * bytesPerRow,
           copyBytes);
  }

  stagingBuffer->Unmap(0, nullptr);

  // Create command allocator and command list for upload
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAlloc;
  hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                        IID_PPV_ARGS(cmdAlloc.GetAddressOf()));
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create command allocator for upload");
  }

  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
  hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(), nullptr,
                                   IID_PPV_ARGS(cmdList.GetAddressOf()));
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create command list for upload");
  }

  // Transition texture to COPY_DEST state
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = resource_.Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
  barrier.Transition.Subresource = range.mipLevel;

  cmdList->ResourceBarrier(1, &barrier);

  // Copy from staging buffer to texture
  D3D12_TEXTURE_COPY_LOCATION dst = {};
  dst.pResource = resource_.Get();
  dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dst.SubresourceIndex = range.mipLevel;

  D3D12_TEXTURE_COPY_LOCATION src = {};
  src.pResource = stagingBuffer.Get();
  src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  src.PlacedFootprint = layout;

  // Define source box for the copy region
  D3D12_BOX srcBox = {};
  srcBox.left = 0;
  srcBox.top = 0;
  srcBox.front = 0;
  srcBox.right = width;
  srcBox.bottom = height;
  srcBox.back = 1;

  cmdList->CopyTextureRegion(&dst, range.x, range.y, range.z, &src, &srcBox);

  // Transition texture to PIXEL_SHADER_RESOURCE state for rendering
  // This is more explicit than relying on COMMON state implicit promotion
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  cmdList->ResourceBarrier(1, &barrier);

  cmdList->Close();

  // Execute command list
  ID3D12CommandList* cmdLists[] = {cmdList.Get()};
  queue_->ExecuteCommandLists(1, cmdLists);

  // Wait for upload to complete (synchronous for now)
  IGL_LOG_INFO("Texture::upload() - Creating fence for GPU sync\n");
  Microsoft::WRL::ComPtr<ID3D12Fence> fence;
  hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("Texture::upload() - Failed to create fence\n");
    return Result(Result::Code::RuntimeError, "Failed to create fence for upload sync");
  }

  HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!fenceEvent) {
    IGL_LOG_ERROR("Texture::upload() - Failed to create fence event\n");
    return Result(Result::Code::RuntimeError, "Failed to create fence event");
  }

  IGL_LOG_INFO("Texture::upload() - Signaling fence and waiting...\n");
  queue_->Signal(fence.Get(), 1);
  fence->SetEventOnCompletion(1, fenceEvent);
  WaitForSingleObject(fenceEvent, INFINITE);
  IGL_LOG_INFO("Texture::upload() - Fence signaled, upload complete!\n");
  CloseHandle(fenceEvent);

  return Result();
}

Result Texture::uploadCube(const TextureRangeDesc& /*range*/,
                          TextureCubeFace /*face*/,
                          const void* /*data*/,
                          size_t /*bytesPerRow*/) const {
  return Result(Result::Code::Unimplemented, "D3D12 Texture::uploadCube not yet implemented");
}

Result Texture::uploadInternal(TextureType type,
                                const TextureRangeDesc& range,
                                const void* data,
                                size_t bytesPerRow,
                                const uint32_t* mipLevelBytes) const {
  if (!(type == TextureType::TwoD || type == TextureType::TwoDArray)) {
    return Result(Result::Code::Unimplemented, "Upload not implemented for this texture type");
  }

  // Handle multi-mip upload in a single call
  if (range.numMipLevels > 1) {
    const auto props = TextureFormatProperties::fromTextureFormat(format_);
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    Result r;
    for (uint32_t i = 0; i < range.numMipLevels; ++i) {
      const uint32_t mip = range.mipLevel + i;
      auto subRange = TextureRangeDesc::new2D(range.x >> i,
                                              range.y >> i,
                                              std::max(1u, range.width >> i),
                                              std::max(1u, range.height >> i),
                                              mip,
                                              1);
      // Compute bytes per row for this mip if not provided
      size_t bpr = bytesPerRow;
      if (bpr == 0) {
        bpr = props.getBytesPerRow(subRange);
      }

      r = upload(subRange, ptr, bpr);
      if (!r.isOk()) {
        return r;
      }

      // Advance pointer by mip size
      if (mipLevelBytes) {
        ptr += mipLevelBytes[i];
      } else {
        const size_t rows = subRange.height;
        ptr += bpr * rows;
      }
    }
    return Result();
  }

  // Single-mip upload
  return upload(range, data, bytesPerRow);
}

Dimensions Texture::getDimensions() const {
  return dimensions_;
}

uint32_t Texture::getNumLayers() const {
  return static_cast<uint32_t>(numLayers_);
}

TextureType Texture::getType() const {
  return type_;
}

TextureDesc::TextureUsage Texture::getUsage() const {
  return usage_;
}

uint32_t Texture::getSamples() const {
  return static_cast<uint32_t>(samples_);
}

uint32_t Texture::getNumMipLevels() const {
  return static_cast<uint32_t>(numMipLevels_);
}

uint64_t Texture::getTextureId() const {
  return reinterpret_cast<uint64_t>(resource_.Get());
}

TextureFormat Texture::getFormat() const {
  return format_;
}

bool Texture::isRequiredGenerateMipmap() const {
  return false;
}

void Texture::generateMipmap(ICommandQueue& /*cmdQueue*/, const TextureRangeDesc* /*range*/) const {
  if (!device_ || !queue_ || !resource_.Get() || numMipLevels_ < 2) {
    return;
  }

  D3D12_RESOURCE_DESC resourceDesc = resource_->GetDesc();

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
  params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[1].DescriptorTable.NumDescriptorRanges = 1;
  params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
  params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
  rsDesc.NumParameters = 2;
  rsDesc.pParameters = params;
  rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  Microsoft::WRL::ComPtr<ID3DBlob> sig, err;
  if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, sig.GetAddressOf(), err.GetAddressOf()))) {
    return;
  }
  Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;
  if (FAILED(device_->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(rootSig.GetAddressOf())))) {
    return;
  }

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

  Microsoft::WRL::ComPtr<ID3DBlob> vs, ps, errs;
  if (FAILED(D3DCompile(kVS, strlen(kVS), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, vs.GetAddressOf(), errs.GetAddressOf()))) {
    return;
  }
  errs.Reset();
  if (FAILED(D3DCompile(kPS, strlen(kPS), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, ps.GetAddressOf(), errs.GetAddressOf()))) {
    return;
  }

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
  pso.pRootSignature = rootSig.Get();
  pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
  pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
  pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pso.RasterizerState.DepthClipEnable = TRUE;
  pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  pso.SampleMask = UINT_MAX;
  pso.SampleDesc.Count = 1;
  pso.NumRenderTargets = 1;
  pso.RTVFormats[0] = resourceDesc.Format;
  pso.DSVFormat = DXGI_FORMAT_UNKNOWN;

  Microsoft::WRL::ComPtr<ID3D12PipelineState> psoObj;
  if (FAILED(device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(psoObj.GetAddressOf())))) {
    return;
  }

  D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
  srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvHeapDesc.NumDescriptors = 1;
  srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap;
  if (FAILED(device_->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(srvHeap.GetAddressOf())))) return;

  D3D12_DESCRIPTOR_HEAP_DESC smpHeapDesc = {};
  smpHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
  smpHeapDesc.NumDescriptors = 1;
  smpHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> smpHeap;
  if (FAILED(device_->CreateDescriptorHeap(&smpHeapDesc, IID_PPV_ARGS(smpHeap.GetAddressOf())))) return;

  // Fixed sampler
  D3D12_SAMPLER_DESC samp = {};
  samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  samp.MinLOD = 0; samp.MaxLOD = D3D12_FLOAT32_MAX;
  device_->CreateSampler(&samp, smpHeap->GetCPUDescriptorHandleForHeapStart());

  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> alloc;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
  if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(alloc.GetAddressOf())))) return;
  if (FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), psoObj.Get(), IID_PPV_ARGS(list.GetAddressOf())))) return;

  ID3D12DescriptorHeap* heaps[] = {srvHeap.Get(), smpHeap.Get()};
  list->SetDescriptorHeaps(2, heaps);
  list->SetPipelineState(psoObj.Get());
  list->SetGraphicsRootSignature(rootSig.Get());
  list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = srvHeap->GetCPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE srvGpu = srvHeap->GetGPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE smpGpu = smpHeap->GetGPUDescriptorHandleForHeapStart();

  for (UINT mip = 0; mip + 1 < numMipLevels_; ++mip) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = resourceDesc.Format;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MostDetailedMip = mip;
    srv.Texture2D.MipLevels = 1;
    device_->CreateShaderResourceView(resource_.Get(), &srv, srvCpu);

    D3D12_RENDER_TARGET_VIEW_DESC rtv = {};
    rtv.Format = resourceDesc.Format;
    rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtv.Texture2D.MipSlice = mip + 1;

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
    if (FAILED(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHeap.GetAddressOf())))) return;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvCpu = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    device_->CreateRenderTargetView(resource_.Get(), &rtv, rtvCpu);

    D3D12_RESOURCE_BARRIER toRT = {};
    toRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRT.Transition.pResource = resource_.Get();
    toRT.Transition.Subresource = mip + 1;
    toRT.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    toRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    list->ResourceBarrier(1, &toRT);

    list->OMSetRenderTargets(1, &rtvCpu, FALSE, nullptr);
    const UINT w = std::max<UINT>(1u, (UINT)(resourceDesc.Width >> (mip + 1)));
    const UINT h = std::max<UINT>(1u, (UINT)(resourceDesc.Height >> (mip + 1)));
    D3D12_VIEWPORT vp{0.0f, 0.0f, (FLOAT)w, (FLOAT)h, 0.0f, 1.0f};
    D3D12_RECT sc{0, 0, (LONG)w, (LONG)h};
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &sc);

    list->SetGraphicsRootDescriptorTable(0, srvGpu);
    list->SetGraphicsRootDescriptorTable(1, smpGpu);
    list->DrawInstanced(3, 1, 0, 0);

    D3D12_RESOURCE_BARRIER toSRV = {};
    toSRV.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toSRV.Transition.pResource = resource_.Get();
    toSRV.Transition.Subresource = mip + 1;
    toSRV.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toSRV.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    list->ResourceBarrier(1, &toSRV);
  }

  list->Close();
  ID3D12CommandList* lists[] = {list.Get()};
  queue_->ExecuteCommandLists(1, lists);

  Microsoft::WRL::ComPtr<ID3D12Fence> fence;
  if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())))) return;
  HANDLE evt = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!evt) return;
  queue_->Signal(fence.Get(), 1);
  fence->SetEventOnCompletion(1, evt);
  WaitForSingleObject(evt, INFINITE);
  CloseHandle(evt);
}

void Texture::generateMipmap(ICommandBuffer& /*cmdBuffer*/, const TextureRangeDesc* /*range*/) const {
  if (!device_ || !queue_ || !resource_.Get() || numMipLevels_ < 2) {
    return;
  }
  D3D12_RESOURCE_DESC resourceDesc = resource_->GetDesc();
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
  params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[1].DescriptorTable.NumDescriptorRanges = 1;
  params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
  params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
  rsDesc.NumParameters = 2;
  rsDesc.pParameters = params;
  rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  Microsoft::WRL::ComPtr<ID3DBlob> sig, err;
  if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, sig.GetAddressOf(), err.GetAddressOf()))) {
    return;
  }
  Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;
  if (FAILED(device_->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(rootSig.GetAddressOf())))) {
    return;
  }
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
  Microsoft::WRL::ComPtr<ID3DBlob> vs, ps, errs;
  if (FAILED(D3DCompile(kVS, strlen(kVS), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, vs.GetAddressOf(), errs.GetAddressOf()))) return;
  errs.Reset();
  if (FAILED(D3DCompile(kPS, strlen(kPS), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, ps.GetAddressOf(), errs.GetAddressOf()))) return;
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
  pso.pRootSignature = rootSig.Get();
  pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
  pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
  pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pso.RasterizerState.DepthClipEnable = TRUE;
  pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  pso.SampleMask = UINT_MAX;
  pso.SampleDesc.Count = 1;
  pso.NumRenderTargets = 1;
  pso.RTVFormats[0] = resourceDesc.Format;
  pso.DSVFormat = DXGI_FORMAT_UNKNOWN;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> psoObj;
  if (FAILED(device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(psoObj.GetAddressOf())))) return;
  D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
  srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvHeapDesc.NumDescriptors = 1;
  srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap;
  if (FAILED(device_->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(srvHeap.GetAddressOf())))) return;
  D3D12_DESCRIPTOR_HEAP_DESC smpHeapDesc = {};
  smpHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
  smpHeapDesc.NumDescriptors = 1;
  smpHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> smpHeap;
  if (FAILED(device_->CreateDescriptorHeap(&smpHeapDesc, IID_PPV_ARGS(smpHeap.GetAddressOf())))) return;
  D3D12_SAMPLER_DESC samp = {};
  samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  samp.MinLOD = 0; samp.MaxLOD = D3D12_FLOAT32_MAX;
  device_->CreateSampler(&samp, smpHeap->GetCPUDescriptorHandleForHeapStart());
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> alloc;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
  if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(alloc.GetAddressOf())))) return;
  if (FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), psoObj.Get(), IID_PPV_ARGS(list.GetAddressOf())))) return;
  ID3D12DescriptorHeap* heaps[] = {srvHeap.Get(), smpHeap.Get()};
  list->SetDescriptorHeaps(2, heaps);
  list->SetPipelineState(psoObj.Get());
  list->SetGraphicsRootSignature(rootSig.Get());
  list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = srvHeap->GetCPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE srvGpu = srvHeap->GetGPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE smpGpu = smpHeap->GetGPUDescriptorHandleForHeapStart();
  for (UINT mip = 0; mip + 1 < numMipLevels_; ++mip) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = resourceDesc.Format;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MostDetailedMip = mip;
    srv.Texture2D.MipLevels = 1;
    device_->CreateShaderResourceView(resource_.Get(), &srv, srvCpu);
    D3D12_RENDER_TARGET_VIEW_DESC rtv = {};
    rtv.Format = resourceDesc.Format;
    rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtv.Texture2D.MipSlice = mip + 1;
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
    if (FAILED(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHeap.GetAddressOf())))) return;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvCpu = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    device_->CreateRenderTargetView(resource_.Get(), &rtv, rtvCpu);
    D3D12_RESOURCE_BARRIER toRT = {};
    toRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRT.Transition.pResource = resource_.Get();
    toRT.Transition.Subresource = mip + 1;
    toRT.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    toRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    list->ResourceBarrier(1, &toRT);
    list->OMSetRenderTargets(1, &rtvCpu, FALSE, nullptr);
    const UINT w = std::max<UINT>(1u, (UINT)(resourceDesc.Width >> (mip + 1)));
    const UINT h = std::max<UINT>(1u, (UINT)(resourceDesc.Height >> (mip + 1)));
    D3D12_VIEWPORT vp{0.0f, 0.0f, (FLOAT)w, (FLOAT)h, 0.0f, 1.0f};
    D3D12_RECT sc{0, 0, (LONG)w, (LONG)h};
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &sc);
    list->SetGraphicsRootDescriptorTable(0, srvGpu);
    list->SetGraphicsRootDescriptorTable(1, smpGpu);
    list->DrawInstanced(3, 1, 0, 0);
    D3D12_RESOURCE_BARRIER toSRV = {};
    toSRV.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toSRV.Transition.pResource = resource_.Get();
    toSRV.Transition.Subresource = mip + 1;
    toSRV.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toSRV.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    list->ResourceBarrier(1, &toSRV);
  }
  list->Close();
  ID3D12CommandList* lists[] = {list.Get()};
  queue_->ExecuteCommandLists(1, lists);
  Microsoft::WRL::ComPtr<ID3D12Fence> fence;
  if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())))) return;
  HANDLE evt = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!evt) return;
  queue_->Signal(fence.Get(), 1);
  fence->SetEventOnCompletion(1, evt);
  WaitForSingleObject(evt, INFINITE);
  CloseHandle(evt);
}

} // namespace igl::d3d12


