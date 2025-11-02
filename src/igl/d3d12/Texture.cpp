/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>
#include <igl/d3d12/Texture.h>
#include <igl/d3d12/DXCCompiler.h>

namespace {
bool needsRGBAChannelSwap(igl::TextureFormat format) {
  using igl::TextureFormat;
  switch (format) {
  case TextureFormat::RGBA_UNorm8:
  case TextureFormat::RGBA_SRGB:
    return true;
  default:
    return false;
  }
}
} // namespace

namespace igl::d3d12 {

std::shared_ptr<Texture> Texture::createFromResource(ID3D12Resource* resource,
                                                     TextureFormat format,
                                                     const TextureDesc& desc,
                                                     ID3D12Device* device,
                                                     ID3D12CommandQueue* queue,
                                                     D3D12_RESOURCE_STATES initialState) {
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

  texture->initializeStateTracking(initialState);

  IGL_LOG_INFO("Texture::createFromResource - SUCCESS: %dx%d format=%d\n",
               desc.width, desc.height, (int)format);

  return texture;
}

std::shared_ptr<Texture> Texture::createTextureView(std::shared_ptr<Texture> parent,
                                                     const TextureViewDesc& desc) {
  if (!parent) {
    IGL_LOG_ERROR("Texture::createTextureView - parent is NULL!\n");
    return nullptr;
  }

  // Determine the format to use for the view
  TextureFormat viewFormat = (desc.format != TextureFormat::Invalid) ? desc.format : parent->format_;

  auto view = std::make_shared<Texture>(viewFormat);

  // Share the D3D12 resource (don't create new one)
  // ComPtr doesn't have copy assignment, so we need to use Attach() and AddRef()
  auto* parentResource = parent->resource_.Get();
  if (parentResource) {
    parentResource->AddRef();
    view->resource_.Attach(parentResource);
  }
  view->isView_ = true;
  view->parentTexture_ = parent;

  // Store view parameters
  view->mipLevelOffset_ = desc.mipLevel;
  view->numMipLevelsInView_ = desc.numMipLevels;
  view->arraySliceOffset_ = desc.layer;
  view->numArraySlicesInView_ = desc.numLayers;

  // Copy properties from parent
  view->device_ = parent->device_;
  view->queue_ = parent->queue_;
  view->format_ = viewFormat;
  view->type_ = desc.type;
  view->usage_ = parent->usage_;
  view->samples_ = parent->samples_;

  // Calculate view dimensions based on mip level
  const uint32_t mipDivisor = 1u << desc.mipLevel;
  view->dimensions_ = Dimensions{
      std::max(1u, parent->dimensions_.width >> desc.mipLevel),
      std::max(1u, parent->dimensions_.height >> desc.mipLevel),
      std::max(1u, parent->dimensions_.depth >> desc.mipLevel)
  };
  view->numLayers_ = desc.numLayers;
  view->numMipLevels_ = desc.numMipLevels;

  // Share state tracking with parent
  view->subresourceStates_ = parent->subresourceStates_;
  view->defaultState_ = parent->defaultState_;

  IGL_LOG_INFO("Texture::createTextureView - SUCCESS: view of %dx%d, mips %u-%u, layers %u-%u\n",
               view->dimensions_.width, view->dimensions_.height,
               desc.mipLevel, desc.mipLevel + desc.numMipLevels - 1,
               desc.layer, desc.layer + desc.numLayers - 1);

  return view;
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
  const uint32_t depth = range.depth > 0 ? range.depth : dimensions_.depth;

  const auto props = TextureFormatProperties::fromTextureFormat(format_);
  // Calculate bytes per row if not provided
  if (bytesPerRow == 0) {
    const size_t bpp = std::max<uint8_t>(props.bytesPerBlock, 1);
    bytesPerRow = static_cast<size_t>(width) * bpp;
  }

  // Get the resource description to calculate required size
  D3D12_RESOURCE_DESC resourceDesc = resource_->GetDesc();

  // Determine how many layers/faces and mip levels we need to upload
  // For cube textures, use range.numFaces; for other textures, use range.numLayers
  const uint32_t numSlicesToUpload = (type_ == TextureType::Cube) ? range.numFaces : range.numLayers;
  const uint32_t baseSlice = (type_ == TextureType::Cube) ? range.face : range.layer;
  const uint32_t numMipsToUpload = range.numMipLevels;
  const uint32_t baseMip = range.mipLevel;

  IGL_LOG_INFO("Texture::upload() - Uploading %u slices x %u mips starting from slice %u, mip %u\n",
               numSlicesToUpload, numMipsToUpload, baseSlice, baseMip);

  // Calculate current width/height/depth for mip level iteration
  uint32_t currentWidth = width;
  uint32_t currentHeight = height;
  uint32_t currentDepth = depth;
  size_t currentDataOffset = 0;

  // Loop through all mip levels to upload
  for (uint32_t mipOffset = 0; mipOffset < numMipsToUpload; ++mipOffset) {
    const uint32_t currentMip = baseMip + mipOffset;

    // Loop through all layers/faces for this mip level
    for (uint32_t sliceOffset = 0; sliceOffset < numSlicesToUpload; ++sliceOffset) {
      const uint32_t currentSlice = baseSlice + sliceOffset;
      const uint32_t subresourceIndex = calcSubresourceIndex(currentMip, currentSlice);

      IGL_LOG_INFO("Texture::upload() - Processing mip %u, slice %u (subresource %u)\n",
                   currentMip, currentSlice, subresourceIndex);

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
    UINT numRows;
    UINT64 rowSizeInBytes;
    UINT64 totalBytes;

    device_->GetCopyableFootprints(&resourceDesc, subresourceIndex, 1, 0,
                                    &layout, &numRows, &rowSizeInBytes, &totalBytes);

      // Use current mip dimensions for this upload
      const uint32_t mipWidth = std::max(currentWidth >> mipOffset, 1u);
      const uint32_t mipHeight = std::max(currentHeight >> mipOffset, 1u);
      const uint32_t mipDepth = std::max(currentDepth >> mipOffset, 1u);
      const size_t mipBytesPerRow = (bytesPerRow * mipWidth) / width;  // Scale bytes per row for mip level

      IGL_LOG_INFO("Texture::upload() - Dimensions: %ux%ux%u, bytesPerRow=%zu, numRows=%u, totalBytes=%llu\n",
                   mipWidth, mipHeight, mipDepth, mipBytesPerRow, numRows, totalBytes);
      IGL_LOG_INFO("Texture::upload() - Layout: RowPitch=%u, Depth=%u, Format=%d, Dimension=%d\n",
                   layout.Footprint.RowPitch, layout.Footprint.Depth, layout.Footprint.Format, resourceDesc.Dimension);

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

      // Copy data for each depth slice (for 2D textures, depth=1)
      // Calculate source data offset for this specific mip level and layer/face
      const uint8_t* srcData = static_cast<const uint8_t*>(data) + currentDataOffset;
      uint8_t* dstData = static_cast<uint8_t*>(mappedData) + layout.Offset;
      const UINT rowsToCopy = (range.height > 0) ? std::min<UINT>(mipHeight, numRows) : numRows;
      const UINT depthToCopy = mipDepth;  // For 2D textures, depth=1; for 3D textures, depth=actual depth
      const size_t copyBytes = std::min(static_cast<size_t>(rowSizeInBytes), mipBytesPerRow);
      const bool swapRB = needsRGBAChannelSwap(format_);  // Swap R<->B for RGBA textures to match D3D12's BGRA format

      // For 3D textures: copy data slice by slice
      // For 2D textures: depthToCopy=1, so this loops once
      const size_t srcDepthPitch = mipBytesPerRow * mipHeight;  // Size of one depth slice in source data
      const size_t dstDepthPitch = layout.Footprint.RowPitch * layout.Footprint.Height;  // Size of one depth slice in staging buffer

      IGL_LOG_INFO("Texture::upload() - Copy loop: depth=%u, rows=%u, srcDepthPitch=%zu, dstDepthPitch=%zu\n",
                   depthToCopy, rowsToCopy, srcDepthPitch, dstDepthPitch);

      for (UINT z = 0; z < depthToCopy; ++z) {
    const uint8_t* srcSlice = srcData + z * srcDepthPitch;
    uint8_t* dstSlice = dstData + z * dstDepthPitch;

        for (UINT row = 0; row < rowsToCopy; ++row) {
          uint8_t* dstRow = dstSlice + row * layout.Footprint.RowPitch;
          const uint8_t* srcRow = srcSlice + row * mipBytesPerRow;
          if (swapRB) {
            const uint32_t pixels = mipWidth;
            for (uint32_t col = 0; col < pixels; ++col) {
              const uint32_t idx = col * 4;
              dstRow[idx + 0] = srcRow[idx + 2]; // B <- R
              dstRow[idx + 1] = srcRow[idx + 1]; // G stays
              dstRow[idx + 2] = srcRow[idx + 0]; // R <- B
              dstRow[idx + 3] = srcRow[idx + 3]; // A
            }
          } else {
            memcpy(dstRow, srcRow, copyBytes);
          }
        }
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
      transitionTo(cmdList.Get(), D3D12_RESOURCE_STATE_COPY_DEST, currentMip, currentSlice);

      // Copy from staging buffer to texture
      D3D12_TEXTURE_COPY_LOCATION dst = {};
      dst.pResource = resource_.Get();
      dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      dst.SubresourceIndex = subresourceIndex;  // Already calculated earlier with both mip and layer

      D3D12_TEXTURE_COPY_LOCATION src = {};
      src.pResource = stagingBuffer.Get();
      src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      src.PlacedFootprint = layout;

      // Define source box for the copy region
      // For 3D textures, srcBox.back must be set to the depth value
      // For 2D textures, depth=1, so srcBox.back=1 (as before)
      D3D12_BOX srcBox = {};
      srcBox.left = 0;
      srcBox.top = 0;
      srcBox.front = 0;
      srcBox.right = mipWidth;
      srcBox.bottom = mipHeight;
      srcBox.back = mipDepth;  // CRITICAL: Use actual depth, not hardcoded 1

      IGL_LOG_INFO("Texture::upload() - CopyTextureRegion: srcBox (%u,%u,%u)->(%u,%u,%u), dst offset (%u,%u,%u)\n",
                   srcBox.left, srcBox.top, srcBox.front, srcBox.right, srcBox.bottom, srcBox.back,
                   range.x, range.y, range.z);

      cmdList->CopyTextureRegion(&dst, range.x, range.y, range.z, &src, &srcBox);

      // Transition texture to PIXEL_SHADER_RESOURCE state for rendering
      transitionTo(cmdList.Get(),
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                   currentMip,
                   currentSlice);

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

      // Update data offset for next slice
      currentDataOffset += mipBytesPerRow * mipHeight * mipDepth;

    } // End of layer/face loop
  } // End of mip level loop

  IGL_LOG_INFO("Texture::upload() - All %u mips x %u slices uploaded successfully\n", numMipsToUpload, numSlicesToUpload);
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
  if (!(type == TextureType::TwoD || type == TextureType::TwoDArray || type == TextureType::ThreeD || type == TextureType::Cube)) {
    return Result(Result::Code::Unimplemented, "Upload not implemented for this texture type");
  }

  // Delegate to upload() which now handles multi-mip, multi-layer, and cube textures natively
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
  IGL_LOG_INFO("Texture::generateMipmap(cmdQueue) - START: numMips=%u\n", numMipLevels_);

  if (!device_ || !queue_ || !resource_.Get() || numMipLevels_ < 2) {
    IGL_LOG_INFO("Texture::generateMipmap() - Skipping: device=%p queue=%p resource=%p numMips=%u\n",
                 device_, queue_, resource_.Get(), numMipLevels_);
    return;
  }

  D3D12_RESOURCE_DESC resourceDesc = resource_->GetDesc();

  // Only support 2D textures for mipmap generation
  if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
    IGL_LOG_INFO("Texture::generateMipmap() - Skipping: only 2D textures supported (dimension=%d)\n",
                 (int)resourceDesc.Dimension);
    return;
  }

  // Check if texture was created with RENDER_TARGET flag (required for mipmap generation)
  if (!(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
    IGL_LOG_INFO("Texture::generateMipmap() - Skipping: texture not created with RENDER_TARGET usage\n");
    IGL_LOG_INFO("  To enable mipmap generation, create texture with TextureDesc::TextureUsageBits::Attachment\n");
    return;
  }

  IGL_LOG_INFO("Texture::generateMipmap() - Proceeding with mipmap generation\n");

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

  // Initialize DXC compiler (static, initialized once)
  static DXCCompiler dxcCompiler;
  static bool dxcInitialized = false;
  if (!dxcInitialized) {
    Result initResult = dxcCompiler.initialize();
    if (!initResult.isOk()) {
      IGL_LOG_ERROR("Texture::generateMipMaps - DXC initialization failed: %s\n", initResult.message.c_str());
      return;
    }
    dxcInitialized = true;
  }

  // Compile shaders with DXC (Shader Model 6.0)
  std::vector<uint8_t> vsBytecode, psBytecode;
  std::string vsErrors, psErrors;

  Result vsResult = dxcCompiler.compile(kVS, strlen(kVS), "main", "vs_6_0", "MipmapGenerationVS", 0, vsBytecode, vsErrors);
  if (!vsResult.isOk()) {
    IGL_LOG_ERROR("Texture::generateMipMaps - VS compilation failed: %s\n%s\n", vsResult.message.c_str(), vsErrors.c_str());
    return;
  }

  Result psResult = dxcCompiler.compile(kPS, strlen(kPS), "main", "ps_6_0", "MipmapGenerationPS", 0, psBytecode, psErrors);
  if (!psResult.isOk()) {
    IGL_LOG_ERROR("Texture::generateMipMaps - PS compilation failed: %s\n%s\n", psResult.message.c_str(), psErrors.c_str());
    return;
  }

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
  pso.pRootSignature = rootSig.Get();
  pso.VS = {vsBytecode.data(), vsBytecode.size()};
  pso.PS = {psBytecode.data(), psBytecode.size()};
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

  // Create descriptor heap large enough for all mip levels
  // We need one SRV descriptor per mip level (numMipLevels_ - 1 blits)
  D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
  srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvHeapDesc.NumDescriptors = numMipLevels_ - 1;  // One SRV per source mip level
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

  // Get descriptor size for incrementing through the heap
  const UINT srvDescriptorSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart = srvHeap->GetCPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart = srvHeap->GetGPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE smpGpu = smpHeap->GetGPUDescriptorHandleForHeapStart();

  // Ensure mip 0 is in PIXEL_SHADER_RESOURCE state for first SRV read
  // Use the Texture's state tracking to ensure correct transition
  transitionTo(list.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0);

  for (UINT mip = 0; mip + 1 < numMipLevels_; ++mip) {
    // Calculate descriptor handle for this mip level
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = srvCpuStart;
    srvCpu.ptr += mip * srvDescriptorSize;
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpu = srvGpuStart;
    srvGpu.ptr += mip * srvDescriptorSize;

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

    // Transition mip level to render target using state tracking
    transitionTo(list.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, mip + 1, 0);

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

    // Transition mip level to shader resource for next iteration
    transitionTo(list.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, mip + 1, 0);
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
  IGL_LOG_INFO("Texture::generateMipmap(cmdBuffer) - START: numMips=%u\n", numMipLevels_);

  if (!device_ || !queue_ || !resource_.Get() || numMipLevels_ < 2) {
    IGL_LOG_INFO("Texture::generateMipmap(cmdBuffer) - Skipping: device=%p queue=%p resource=%p numMips=%u\n",
                 device_, queue_, resource_.Get(), numMipLevels_);
    return;
  }

  D3D12_RESOURCE_DESC resourceDesc = resource_->GetDesc();

  // Only support 2D textures for mipmap generation
  if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
    IGL_LOG_INFO("Texture::generateMipmap(cmdBuffer) - Skipping: only 2D textures supported\n");
    return;
  }

  // Check if texture was created with RENDER_TARGET flag (required for mipmap generation)
  if (!(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
    IGL_LOG_INFO("Texture::generateMipmap(cmdBuffer) - Skipping: texture not created with RENDER_TARGET usage\n");
    IGL_LOG_INFO("  To enable mipmap generation, create texture with TextureDesc::TextureUsageBits::Attachment\n");
    return;
  }
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

  // Initialize DXC compiler (static, initialized once)
  static DXCCompiler dxcCompiler;
  static bool dxcInitialized = false;
  if (!dxcInitialized) {
    Result initResult = dxcCompiler.initialize();
    if (!initResult.isOk()) {
      IGL_LOG_ERROR("Texture::upload - DXC initialization failed: %s\n", initResult.message.c_str());
      return;
    }
    dxcInitialized = true;
  }

  // Compile shaders with DXC (Shader Model 6.0)
  std::vector<uint8_t> vsBytecode, psBytecode;
  std::string vsErrors, psErrors;

  Result vsResult = dxcCompiler.compile(kVS, strlen(kVS), "main", "vs_6_0", "TextureUploadVS", 0, vsBytecode, vsErrors);
  if (!vsResult.isOk()) {
    IGL_LOG_ERROR("Texture::upload - VS compilation failed: %s\n%s\n", vsResult.message.c_str(), vsErrors.c_str());
    return;
  }

  Result psResult = dxcCompiler.compile(kPS, strlen(kPS), "main", "ps_6_0", "TextureUploadPS", 0, psBytecode, psErrors);
  if (!psResult.isOk()) {
    IGL_LOG_ERROR("Texture::upload - PS compilation failed: %s\n%s\n", psResult.message.c_str(), psErrors.c_str());
    return;
  }

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
  pso.pRootSignature = rootSig.Get();
  pso.VS = {vsBytecode.data(), vsBytecode.size()};
  pso.PS = {psBytecode.data(), psBytecode.size()};
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
  // Create descriptor heap large enough for all mip levels
  // We need one SRV descriptor per mip level (numMipLevels_ - 1 blits)
  D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
  srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvHeapDesc.NumDescriptors = numMipLevels_ - 1;  // One SRV per source mip level
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
  // Get descriptor size for incrementing through the heap
  const UINT srvDescriptorSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart = srvHeap->GetCPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart = srvHeap->GetGPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE smpGpu = smpHeap->GetGPUDescriptorHandleForHeapStart();

  // Ensure mip 0 is in PIXEL_SHADER_RESOURCE state for first SRV read
  // Use the Texture's state tracking to ensure correct transition
  transitionTo(list.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0);

  for (UINT mip = 0; mip + 1 < numMipLevels_; ++mip) {
    // Calculate descriptor handle for this mip level
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = srvCpuStart;
    srvCpu.ptr += mip * srvDescriptorSize;
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpu = srvGpuStart;
    srvGpu.ptr += mip * srvDescriptorSize;

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

    // Transition mip level to render target using state tracking
    transitionTo(list.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, mip + 1, 0);

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

    // Transition mip level to shader resource for next iteration
    transitionTo(list.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, mip + 1, 0);
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

void Texture::initializeStateTracking(D3D12_RESOURCE_STATES initialState) const {
  defaultState_ = initialState;
  subresourceStates_.clear();
  ensureStateStorage();
  std::fill(subresourceStates_.begin(), subresourceStates_.end(), initialState);
}

void Texture::ensureStateStorage() const {
  if (!resource_.Get()) {
    subresourceStates_.clear();
    return;
  }
  const uint32_t mipLevels = static_cast<uint32_t>(std::max<size_t>(numMipLevels_, 1));
  const uint32_t arraySize =
      (type_ == TextureType::ThreeD) ? 1u : static_cast<uint32_t>(std::max<size_t>(numLayers_, 1));
  const size_t required = static_cast<size_t>(mipLevels) * arraySize;
  if (subresourceStates_.size() != required) {
    subresourceStates_.assign(required, defaultState_);
  }
}

uint32_t Texture::calcSubresourceIndex(uint32_t mipLevel, uint32_t layer) const {
  const uint32_t mipLevels = static_cast<uint32_t>(std::max<size_t>(numMipLevels_, 1));
  uint32_t arraySize;
  if (type_ == TextureType::ThreeD) {
    arraySize = 1u;
  } else if (type_ == TextureType::Cube) {
    arraySize = 6u;  // Cube textures always have 6 faces
  } else {
    arraySize = static_cast<uint32_t>(std::max<size_t>(numLayers_, 1));
  }
  const uint32_t clampedMip = std::min(mipLevel, mipLevels - 1);
  const uint32_t clampedLayer = std::min(layer, arraySize - 1);
  return clampedLayer * mipLevels + clampedMip;
}

void Texture::transitionTo(ID3D12GraphicsCommandList* commandList,
                           D3D12_RESOURCE_STATES newState,
                           uint32_t mipLevel,
                           uint32_t layer) const {
  if (!commandList || !resource_.Get()) {
    return;
  }
  ensureStateStorage();
  if (subresourceStates_.empty()) {
    return;
  }
  const uint32_t subresource = calcSubresourceIndex(mipLevel, layer);
  if (subresource >= subresourceStates_.size()) {
    return;
  }
  auto& currentState = subresourceStates_[subresource];
  if (currentState == newState) {
    return;
  }

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = resource_.Get();
  barrier.Transition.Subresource = subresource;
  barrier.Transition.StateBefore = currentState;
  barrier.Transition.StateAfter = newState;
  commandList->ResourceBarrier(1, &barrier);
  currentState = newState;
}

void Texture::transitionAll(ID3D12GraphicsCommandList* commandList,
                            D3D12_RESOURCE_STATES newState) const {
  if (!commandList || !resource_.Get()) {
    return;
  }
  ensureStateStorage();
  if (subresourceStates_.empty()) {
    return;
  }

  std::vector<D3D12_RESOURCE_BARRIER> barriers;
  barriers.reserve(subresourceStates_.size());
  for (uint32_t i = 0; i < subresourceStates_.size(); ++i) {
    auto& state = subresourceStates_[i];
    if (state == newState) {
      continue;
    }
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource_.Get();
    barrier.Transition.Subresource = i;
    barrier.Transition.StateBefore = state;
    barrier.Transition.StateAfter = newState;
    barriers.push_back(barrier);
    state = newState;
  }
  if (!barriers.empty()) {
    commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
  }
}

D3D12_RESOURCE_STATES Texture::getSubresourceState(uint32_t mipLevel, uint32_t layer) const {
  ensureStateStorage();
  if (subresourceStates_.empty()) {
    return defaultState_;
  }
  const uint32_t index = calcSubresourceIndex(mipLevel, layer);
  if (index >= subresourceStates_.size()) {
    return defaultState_;
  }
  return subresourceStates_[index];
}

} // namespace igl::d3d12


