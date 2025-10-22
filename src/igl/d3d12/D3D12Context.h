/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12Headers.h>

namespace igl::d3d12 {

class D3D12Context {
 public:
  D3D12Context() = default;
  ~D3D12Context();

  Result initialize(HWND hwnd, uint32_t width, uint32_t height);
  Result resize(uint32_t width, uint32_t height);

  ID3D12Device* getDevice() const { return device_.Get(); }
  ID3D12CommandQueue* getCommandQueue() const { return commandQueue_.Get(); }
  IDXGISwapChain3* getSwapChain() const { return swapChain_.Get(); }
  ID3D12DescriptorHeap* getCbvSrvUavHeap() const { return cbvSrvUavHeap_.Get(); }
  ID3D12DescriptorHeap* getSamplerHeap() const { return samplerHeap_.Get(); }

  uint32_t getCurrentBackBufferIndex() const;
  ID3D12Resource* getCurrentBackBuffer() const;
  D3D12_CPU_DESCRIPTOR_HANDLE getCurrentRTV() const;

  void waitForGPU();

 protected:
  void createDevice();
  void createCommandQueue();
  void createSwapChain(HWND hwnd, uint32_t width, uint32_t height);
  void createRTVHeap();
  void createBackBuffers();
  void createDescriptorHeaps();

  Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory_;
  Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter_;
  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
  Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain_;

  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
  Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets_[kMaxFramesInFlight];
  UINT rtvDescriptorSize_ = 0;

  // Descriptor heaps for resource binding
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap_;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> samplerHeap_;
  UINT cbvSrvUavDescriptorSize_ = 0;
  UINT samplerDescriptorSize_ = 0;

  // Synchronization
  Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
  UINT64 fenceValue_ = 0;
  HANDLE fenceEvent_ = nullptr;

  uint32_t width_ = 0;
  uint32_t height_ = 0;
};

} // namespace igl::d3d12
