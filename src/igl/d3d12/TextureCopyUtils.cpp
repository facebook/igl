/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/TextureCopyUtils.h>

#include <igl/d3d12/D3D12Headers.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12Context.h>
#include <igl/d3d12/Texture.h>
#include <igl/d3d12/Buffer.h>

#include <cstring>

namespace igl::d3d12::TextureCopyUtils {

Result executeCopyTextureToBuffer(D3D12Context& ctx,
                                   Device& iglDevice,
                                   Texture& srcTex,
                                   Buffer& dstBuf,
                                   uint64_t destinationOffset,
                                   uint32_t mipLevel,
                                   uint32_t layer) {
  ID3D12Resource* srcRes = srcTex.getResource();
  ID3D12Resource* dstRes = dstBuf.getResource();

  if (!srcRes || !dstRes) {
    return Result{Result::Code::ArgumentInvalid, "Invalid source or destination resource"};
  }

  ID3D12Device* device = ctx.getDevice();
  ID3D12CommandQueue* queue = ctx.getCommandQueue();

  if (!device || !queue) {
    return Result{Result::Code::RuntimeError, "Device or command queue is null"};
  }

  // Get texture description for GetCopyableFootprints
  D3D12_RESOURCE_DESC srcDesc = srcRes->GetDesc();

  // Calculate subresource index
  const uint32_t subresource = srcTex.calcSubresourceIndex(mipLevel, layer);

  // Get copyable footprint for this subresource
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
  UINT numRows = 0;
  UINT64 rowSizeInBytes = 0;
  UINT64 totalBytes = 0;

  device->GetCopyableFootprints(&srcDesc,
                                subresource,
                                1,
                                destinationOffset,
                                &layout,
                                &numRows,
                                &rowSizeInBytes,
                                &totalBytes);

  // Calculate the unpacked texture data size (without D3D12 padding)
  // rowSizeInBytes is the unpadded row size, so we can use it directly
  const UINT64 unpackedDataSize = rowSizeInBytes * numRows * layout.Footprint.Depth;

  // Check if destination buffer is large enough for the unpacked data
  if (destinationOffset + unpackedDataSize > dstBuf.getSizeInBytes()) {
    return Result{Result::Code::ArgumentOutOfRange, "Destination buffer too small"};
  }

  // ALWAYS use a readback staging buffer because D3D12 requires row-pitch alignment (256 bytes)
  // The staging buffer accommodates D3D12's padding, then we copy the unpacked data to the destination
  // This is necessary even for DEFAULT heap buffers that can't be CPU-mapped directly
  bool needsReadbackStaging = true;

  Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer;
  ID3D12Resource* copyDestination = dstRes;

  if (needsReadbackStaging) {
    // Create a READBACK buffer for staging
    // The readback buffer needs to be large enough to hold the data at layout.Offset
    D3D12_HEAP_PROPERTIES readbackHeap{};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC readbackDesc{};
    readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    readbackDesc.Width = layout.Offset + totalBytes;  // Use layout.Offset, not destinationOffset
    readbackDesc.Height = 1;
    readbackDesc.DepthOrArraySize = 1;
    readbackDesc.MipLevels = 1;
    readbackDesc.Format = DXGI_FORMAT_UNKNOWN;
    readbackDesc.SampleDesc.Count = 1;
    readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = device->CreateCommittedResource(&readbackHeap,
                                                  D3D12_HEAP_FLAG_NONE,
                                                  &readbackDesc,
                                                  D3D12_RESOURCE_STATE_COPY_DEST,
                                                  nullptr,
                                                  IID_PPV_ARGS(readbackBuffer.GetAddressOf()));
    if (FAILED(hr)) {
      return Result{Result::Code::RuntimeError, "Failed to create readback buffer"};
    }

    copyDestination = readbackBuffer.Get();
  }

  // D-001: Use pooled allocator instead of creating transient one
  auto allocator = iglDevice.getUploadCommandAllocator();
  if (!allocator.Get()) {
    return Result{Result::Code::RuntimeError, "Failed to get allocator from pool"};
  }

  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
  if (FAILED(device->CreateCommandList(0,
                                       D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       allocator.Get(),
                                       nullptr,
                                       IID_PPV_ARGS(cmdList.GetAddressOf())))) {
    // D-001: Return allocator to pool even on failure (with fence value 0)
    iglDevice.returnUploadCommandAllocator(allocator, 0);
    return Result{Result::Code::RuntimeError, "Failed to create command list"};
  }

  // Get current texture state
  D3D12_RESOURCE_STATES srcStateBefore = srcTex.getSubresourceState(mipLevel, layer);

  // Transition texture to COPY_SOURCE if needed
  if (srcStateBefore != D3D12_RESOURCE_STATE_COPY_SOURCE) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = srcRes;
    barrier.Transition.Subresource = subresource;
    barrier.Transition.StateBefore = srcStateBefore;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    cmdList->ResourceBarrier(1, &barrier);
  }

  // Setup source texture copy location
  D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
  srcLocation.pResource = srcRes;
  srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  srcLocation.SubresourceIndex = subresource;

  // Setup destination buffer copy location
  D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
  dstLocation.pResource = copyDestination;
  dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dstLocation.PlacedFootprint = layout;

  // Perform the copy
  cmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

  // Transition texture back to original state
  if (srcStateBefore != D3D12_RESOURCE_STATE_COPY_SOURCE) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = srcRes;
    barrier.Transition.Subresource = subresource;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter = srcStateBefore;
    cmdList->ResourceBarrier(1, &barrier);
  }

  cmdList->Close();

  // Execute the command list
  ID3D12CommandList* lists[] = {cmdList.Get()};
  queue->ExecuteCommandLists(1, lists);

  // Wait for GPU to complete
  ctx.waitForGPU();

  // D-001: Return allocator to pool after synchronous GPU wait
  // Since waitForGPU() completes, the allocator is safe to reuse
  iglDevice.returnUploadCommandAllocator(allocator, 0);

  // If we used a readback staging buffer, copy to the final destination
  if (needsReadbackStaging) {
    void* readbackData = nullptr;
    // Map the readback buffer region containing the texture data
    D3D12_RANGE readRange{static_cast<SIZE_T>(layout.Offset),
                         static_cast<SIZE_T>(layout.Offset + totalBytes)};

    if (SUCCEEDED(readbackBuffer->Map(0, &readRange, &readbackData)) && readbackData) {
      // Check if destination buffer is in DEFAULT heap (Storage buffers)
      // We cannot call map() on DEFAULT heap buffers because Buffer::map() would
      // create its own staging buffer and copy FROM (empty) DEFAULT buffer first
      D3D12_HEAP_PROPERTIES heapProps;
      dstRes->GetHeapProperties(&heapProps, nullptr);
      const bool isDefaultHeap = (heapProps.Type == D3D12_HEAP_TYPE_DEFAULT);

#ifdef IGL_DEBUG
      IGL_LOG_INFO("copyTextureToBuffer: Destination heap type = %d (1=UPLOAD, 2=DEFAULT, 3=READBACK), isDefaultHeap=%d\n",
                   heapProps.Type, isDefaultHeap);
#endif

      if (!isDefaultHeap) {
        // Destination is CPU-mappable (UPLOAD/READBACK heap) - copy via CPU
        // Copy row-by-row, removing D3D12's row pitch padding
        Result mapResult;
        void* dstData = dstBuf.map(BufferRange(unpackedDataSize, destinationOffset), &mapResult);
        if (dstData && mapResult.isOk()) {
          const uint8_t* src = static_cast<uint8_t*>(readbackData) + layout.Offset;
          uint8_t* dst = static_cast<uint8_t*>(dstData);
          const UINT64 srcRowPitch = layout.Footprint.RowPitch;
          const UINT64 dstRowPitch = rowSizeInBytes;  // Unpadded row size

          for (UINT z = 0; z < layout.Footprint.Depth; ++z) {
            for (UINT row = 0; row < numRows; ++row) {
              std::memcpy(dst, src, dstRowPitch);
              src += srcRowPitch;
              dst += dstRowPitch;
            }
          }

          dstBuf.unmap();
        } else {
          readbackBuffer->Unmap(0, nullptr);
          return Result{Result::Code::RuntimeError, "Failed to map destination buffer"};
        }
      } else {
        // Destination is NOT CPU-mappable (DEFAULT heap) - need GPU copy
        // Create temporary UPLOAD buffer with unpacked data, then GPU copy to destination
        D3D12_HEAP_PROPERTIES uploadHeap{};
        uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC uploadDesc{};
        uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDesc.Width = unpackedDataSize;
        uploadDesc.Height = 1;
        uploadDesc.DepthOrArraySize = 1;
        uploadDesc.MipLevels = 1;
        uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
        uploadDesc.SampleDesc.Count = 1;
        uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
        HRESULT hr = device->CreateCommittedResource(&uploadHeap,
                                                      D3D12_HEAP_FLAG_NONE,
                                                      &uploadDesc,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ,
                                                      nullptr,
                                                      IID_PPV_ARGS(uploadBuffer.GetAddressOf()));
        if (FAILED(hr)) {
          readbackBuffer->Unmap(0, nullptr);
          return Result{Result::Code::RuntimeError, "Failed to create upload buffer"};
        }

        // Map upload buffer and unpack data from readback
        void* uploadData = nullptr;
        if (SUCCEEDED(uploadBuffer->Map(0, nullptr, &uploadData)) && uploadData) {
          const uint8_t* src = static_cast<uint8_t*>(readbackData) + layout.Offset;
          uint8_t* dst = static_cast<uint8_t*>(uploadData);
          const UINT64 srcRowPitch = layout.Footprint.RowPitch;
          const UINT64 dstRowPitch = rowSizeInBytes;

          for (UINT z = 0; z < layout.Footprint.Depth; ++z) {
            for (UINT row = 0; row < numRows; ++row) {
              std::memcpy(dst, src, dstRowPitch);
              src += srcRowPitch;
              dst += dstRowPitch;
            }
          }
          uploadBuffer->Unmap(0, nullptr);

          // GPU copy from upload buffer to destination DEFAULT buffer
          // D-001: Use pooled allocator instead of creating transient one
          auto copyAllocator = iglDevice.getUploadCommandAllocator();
          if (!copyAllocator.Get()) {
            readbackBuffer->Unmap(0, nullptr);
            return Result{Result::Code::RuntimeError, "Failed to get allocator for GPU copy"};
          } else {
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> copyList;
            if (FAILED(device->CreateCommandList(0,
                                                  D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                  copyAllocator.Get(),
                                                  nullptr,
                                                  IID_PPV_ARGS(copyList.GetAddressOf())))) {
              // D-001: Return allocator to pool even on failure
              iglDevice.returnUploadCommandAllocator(copyAllocator, 0);
              readbackBuffer->Unmap(0, nullptr);
              return Result{Result::Code::RuntimeError, "Failed to create command list for GPU copy"};
            } else {
            // Transition destination buffer to COPY_DEST state
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = dstRes;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            copyList->ResourceBarrier(1, &barrier);

            // Copy unpacked data to destination
#ifdef IGL_DEBUG
            IGL_LOG_INFO("copyTextureToBuffer: GPU copy %llu bytes from upload buffer to DEFAULT buffer at offset %llu\n",
                         unpackedDataSize, destinationOffset);
#endif
            copyList->CopyBufferRegion(dstRes, destinationOffset, uploadBuffer.Get(), 0, unpackedDataSize);

            // Transition destination buffer back to UAV state (Storage buffer)
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            copyList->ResourceBarrier(1, &barrier);

              copyList->Close();
              ID3D12CommandList* lists[] = {copyList.Get()};
              queue->ExecuteCommandLists(1, lists);
#ifdef IGL_DEBUG
              IGL_LOG_INFO("copyTextureToBuffer: Waiting for GPU copy to complete...\n");
#endif
              ctx.waitForGPU();
#ifdef IGL_DEBUG
              IGL_LOG_INFO("copyTextureToBuffer: GPU copy complete!\n");
#endif

              // D-001: Return allocator to pool after synchronous GPU wait
              iglDevice.returnUploadCommandAllocator(copyAllocator, 0);
            }
          }
        } else {
          readbackBuffer->Unmap(0, nullptr);
          return Result{Result::Code::RuntimeError, "Failed to map upload buffer"};
        }
      }

      readbackBuffer->Unmap(0, nullptr);
    } else {
      return Result{Result::Code::RuntimeError, "Failed to map readback buffer"};
    }
  }

  return Result{};
}

} // namespace igl::d3d12::TextureCopyUtils
