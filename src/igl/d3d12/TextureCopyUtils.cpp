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
#include <igl/d3d12/D3D12ImmediateCommands.h>  // T07
#include <igl/d3d12/D3D12StagingDevice.h>  // T07

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

  if (!device) {
    return Result{Result::Code::RuntimeError, "Device is null"};
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

  // T07: Use centralized staging device for readback buffer allocation
  auto* stagingDevice = iglDevice.getStagingDevice();
  if (!stagingDevice) {
    return Result{Result::Code::RuntimeError, "Staging device not available"};
  }

  // Allocate readback staging buffer (D3D12 requires row-pitch alignment)
  auto staging = stagingDevice->allocateReadback(layout.Offset + totalBytes);
  if (!staging.valid) {
    return Result{Result::Code::RuntimeError, "Failed to allocate readback staging buffer"};
  }

  ID3D12Resource* readbackBuffer = staging.buffer.Get();
  ID3D12Resource* copyDestination = readbackBuffer;

  // T07: Use centralized immediate commands instead of creating transient allocator/list
  auto* immediateCommands = iglDevice.getImmediateCommands();
  if (!immediateCommands) {
    return Result{Result::Code::RuntimeError, "Immediate commands not available"};
  }

  Result cmdResult;
  ID3D12GraphicsCommandList* cmdList = immediateCommands->begin(&cmdResult);
  if (!cmdList || !cmdResult.isOk()) {
    return Result{Result::Code::RuntimeError, "Failed to begin immediate command list"};
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

  // T07: Submit via immediate commands with synchronous wait
  Result submitResult;
  const uint64_t fenceValue = immediateCommands->submit(true, &submitResult);
  if (!submitResult.isOk() || fenceValue == 0) {
    return Result{Result::Code::RuntimeError,
                  "Failed to submit immediate commands: " + submitResult.message};
  }

  // Copy from readback staging buffer to final destination
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
      IGL_LOG_INFO("copyTextureToBuffer: Destination heap type = %d (1=DEFAULT, 2=UPLOAD, 3=READBACK), isDefaultHeap=%d\n",
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

          // T07: GPU copy from upload buffer to destination DEFAULT buffer using immediate commands
          Result gpuCopyResult;
          ID3D12GraphicsCommandList* copyList = immediateCommands->begin(&gpuCopyResult);
          if (!copyList || !gpuCopyResult.isOk()) {
            readbackBuffer->Unmap(0, nullptr);
            return Result{Result::Code::RuntimeError, "Failed to begin immediate command list for GPU copy"};
          }

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

          // T07: Submit and wait for GPU copy
          Result copySubmitResult;
          const uint64_t copyFenceValue = immediateCommands->submit(true, &copySubmitResult);
#ifdef IGL_DEBUG
          IGL_LOG_INFO("copyTextureToBuffer: GPU copy complete!\n");
#endif
          if (!copySubmitResult.isOk() || copyFenceValue == 0) {
            readbackBuffer->Unmap(0, nullptr);
            return Result{Result::Code::RuntimeError,
                          "Failed to submit GPU copy: " + copySubmitResult.message};
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

  // T07: Return staging buffer to pool
  stagingDevice->free(staging, fenceValue);

  return Result{};
}

} // namespace igl::d3d12::TextureCopyUtils
