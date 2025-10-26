/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/CommandBuffer.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/RenderCommandEncoder.h>
#include <igl/d3d12/ComputeCommandEncoder.h>
#include <igl/d3d12/Buffer.h>

namespace igl::d3d12 {

CommandBuffer::CommandBuffer(Device& device, const CommandBufferDesc& desc)
    : ICommandBuffer(desc), device_(device) {
  auto* d3dDevice = device_.getD3D12Context().getDevice();

  if (!d3dDevice) {
    IGL_DEBUG_ABORT("D3D12 device is null - context not initialized");
    return;
  }

  // Check if device is in good state
  HRESULT deviceRemovedReason = d3dDevice->GetDeviceRemovedReason();
  if (FAILED(deviceRemovedReason)) {
    char errorMsg[512];
    snprintf(errorMsg, sizeof(errorMsg),
             "D3D12 device was removed before creating command buffer. Reason: 0x%08X\n"
             "  0x887A0005 = DXGI_ERROR_DEVICE_REMOVED\n"
             "  0x887A0006 = DXGI_ERROR_DEVICE_HUNG\n"
             "  0x887A0007 = DXGI_ERROR_DEVICE_RESET\n"
             "  0x887A0020 = DXGI_ERROR_DRIVER_INTERNAL_ERROR",
             static_cast<unsigned>(deviceRemovedReason));
    IGL_LOG_ERROR(errorMsg);
    IGL_DEBUG_ABORT("Device removed - see error above");
    return;
  }

  // Create command allocator
  HRESULT hr = d3dDevice->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      IID_PPV_ARGS(commandAllocator_.GetAddressOf()));

  if (FAILED(hr)) {
    char errorMsg[256];
    snprintf(errorMsg, sizeof(errorMsg), "Failed to create command allocator: HRESULT = 0x%08X", static_cast<unsigned>(hr));
    IGL_DEBUG_ABORT(errorMsg);
  }

  // Create command list
  hr = d3dDevice->CreateCommandList(
      0,
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      commandAllocator_.Get(),
      nullptr,
      IID_PPV_ARGS(commandList_.GetAddressOf()));

  if (FAILED(hr)) {
    char errorMsg[256];
    snprintf(errorMsg, sizeof(errorMsg), "Failed to create command list: HRESULT = 0x%08X", static_cast<unsigned>(hr));
    IGL_DEBUG_ABORT(errorMsg);
  }

  // Command lists are created in recording state, close it for now
  commandList_->Close();
}

void CommandBuffer::begin() {
  if (recording_) {
    return;
  }
  IGL_LOG_INFO("CommandBuffer::begin() - Resetting allocator...\n");
  HRESULT hr = commandAllocator_->Reset();
  if (FAILED(hr)) {
    IGL_LOG_ERROR("CommandBuffer::begin() - Reset allocator FAILED: 0x%08X\n", static_cast<unsigned>(hr));
    return;
  }
  IGL_LOG_INFO("CommandBuffer::begin() - Allocator reset OK, resetting command list...\n");
  hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
  if (FAILED(hr)) {
    IGL_LOG_ERROR("CommandBuffer::begin() - Reset command list FAILED: 0x%08X\n", static_cast<unsigned>(hr));
    return;
  }
  IGL_LOG_INFO("CommandBuffer::begin() - Command list reset OK\n");
  recording_ = true;
}

void CommandBuffer::end() {
  if (!recording_) {
    return;
  }
  commandList_->Close();
  recording_ = false;
}

D3D12Context& CommandBuffer::getContext() {
  return device_.getD3D12Context();
}

const D3D12Context& CommandBuffer::getContext() const {
  return device_.getD3D12Context();
}

// Device draw count is incremented by CommandQueue::submit() using this buffer's count

std::unique_ptr<IRenderCommandEncoder> CommandBuffer::createRenderCommandEncoder(
    const RenderPassDesc& renderPass,
    const std::shared_ptr<IFramebuffer>& framebuffer,
    const Dependencies& /*dependencies*/,
    Result* IGL_NULLABLE outResult) {
  Result::setOk(outResult);

  // Begin command buffer if not already begun
  begin();

  return std::make_unique<RenderCommandEncoder>(*this, renderPass, framebuffer);
}

std::unique_ptr<IComputeCommandEncoder> CommandBuffer::createComputeCommandEncoder() {
  // Begin command buffer if not already begun
  begin();

  return std::make_unique<ComputeCommandEncoder>(*this);
}

void CommandBuffer::present(const std::shared_ptr<ITexture>& /*surface*/) const {
  // Note: Actual present happens in CommandQueue::submit()
  // This is just a marker to indicate that this command buffer will present when submitted
  // For D3D12, the present call must happen AFTER ExecuteCommandLists, so it's handled in submit()
}

void CommandBuffer::waitUntilScheduled() {
  // Stub: Not yet implemented
}

void CommandBuffer::waitUntilCompleted() {
  // Stub: Not yet implemented
}

void CommandBuffer::pushDebugGroupLabel(const char* /*label*/,
                                        const igl::Color& /*color*/) const {
  // Stub: Not yet implemented
}

void CommandBuffer::popDebugGroupLabel() const {
  // Stub: Not yet implemented
}

void CommandBuffer::copyBuffer(IBuffer& source,
                               IBuffer& destination,
                               uint64_t sourceOffset,
                               uint64_t destinationOffset,
                               uint64_t size) {
  auto* src = static_cast<Buffer*>(&source);
  auto* dst = static_cast<Buffer*>(&destination);
  ID3D12Resource* srcRes = src->getResource();
  ID3D12Resource* dstRes = dst->getResource();
  if (!srcRes || !dstRes || size == 0) {
    return;
  }

  // Use a transient copy with appropriate heap handling
  auto& ctx = getContext();
  ID3D12Device* device = ctx.getDevice();
  ID3D12CommandQueue* queue = ctx.getCommandQueue();
  if (!device || !queue) {
    return;
  }

  auto doCopyOnList = [&](ID3D12GraphicsCommandList* list,
                          ID3D12Resource* dstResLocal,
                          uint64_t dstOffsetLocal) {
    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = srcRes;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = dstResLocal;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    list->ResourceBarrier(2, barriers);

    list->CopyBufferRegion(dstResLocal, dstOffsetLocal, srcRes, sourceOffset, size);

    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    list->ResourceBarrier(2, barriers);
  };

  if (dst->storage() == ResourceStorage::Shared) {
    // GPU cannot write into UPLOAD heap; use a READBACK staging buffer and then memcpy into UPLOAD
    D3D12_HEAP_PROPERTIES readbackHeap{};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size + destinationOffset;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Microsoft::WRL::ComPtr<ID3D12Resource> readback;
    HRESULT hr = device->CreateCommittedResource(&readbackHeap,
                                                  D3D12_HEAP_FLAG_NONE,
                                                  &desc,
                                                  D3D12_RESOURCE_STATE_COPY_DEST,
                                                  nullptr,
                                                  IID_PPV_ARGS(readback.GetAddressOf()));
    if (FAILED(hr)) {
      IGL_LOG_ERROR("copyBuffer: Failed to create READBACK buffer, hr=0x%08X\n", static_cast<unsigned>(hr));
      return;
    }

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                              IID_PPV_ARGS(allocator.GetAddressOf()))) ||
        FAILED(device->CreateCommandList(0,
                                         D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         allocator.Get(),
                                         nullptr,
                                         IID_PPV_ARGS(list.GetAddressOf())))) {
      IGL_LOG_ERROR("copyBuffer: Failed to create transient command list\n");
      return;
    }

    // Transition source to COPY_SOURCE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = srcRes;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    list->ResourceBarrier(1, &barrier);

    // Copy from source to readback (readback is already in COPY_DEST state)
    list->CopyBufferRegion(readback.Get(), destinationOffset, srcRes, sourceOffset, size);

    // Transition source back
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
    list->ResourceBarrier(1, &barrier);
    list->Close();
    ID3D12CommandList* lists[] = {list.Get()};
    queue->ExecuteCommandLists(1, lists);
    ctx.waitForGPU();

    // Map readback and copy into the UPLOAD buffer
    void* rbPtr = nullptr;
    D3D12_RANGE readRange{static_cast<SIZE_T>(destinationOffset), static_cast<SIZE_T>(destinationOffset + size)};
    if (SUCCEEDED(readback->Map(0, &readRange, &rbPtr)) && rbPtr) {
      // Map destination upload buffer
      Result r1;
      void* dstPtr = dst->map(BufferRange(size, destinationOffset), &r1);
      if (dstPtr && r1.isOk()) {
        std::memcpy(dstPtr, static_cast<uint8_t*>(rbPtr) + destinationOffset, size);
        dst->unmap();
      }
      readback->Unmap(0, nullptr);
    }
    return;
  }

  // Default path: copy using a transient command list to DEFAULT/COMMON destinations
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
  if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            IID_PPV_ARGS(allocator.GetAddressOf()))) ||
      FAILED(device->CreateCommandList(0,
                                       D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       allocator.Get(),
                                       nullptr,
                                       IID_PPV_ARGS(list.GetAddressOf())))) {
    return;
  }

  doCopyOnList(list.Get(), dstRes, destinationOffset);
  list->Close();
  ID3D12CommandList* lists2[] = {list.Get()};
  queue->ExecuteCommandLists(1, lists2);
  ctx.waitForGPU();
}

void CommandBuffer::copyTextureToBuffer(ITexture& /*source*/,
                                       IBuffer& /*destination*/,
                                       uint64_t /*destinationOffset*/,
                                       uint32_t /*mipLevel*/,
                                       uint32_t /*layer*/) {
  // Stub: Not yet implemented
}

} // namespace igl::d3d12
