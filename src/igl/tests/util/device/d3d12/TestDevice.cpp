/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/tests/util/device/d3d12/TestDevice.h>

#include <igl/Common.h>
#include <igl/d3d12/D3D12Context.h>
#include <igl/d3d12/D3D12Headers.h>

namespace igl::tests::util::device::d3d12 {

namespace {

/// Headless D3D12Context for unit testing (no swapchain required)
class HeadlessD3D12Context : public igl::d3d12::D3D12Context {
 public:
  Result initializeHeadless(bool enableDebugLayer) {
    try {
      // Enable debug layer if requested
      if (enableDebugLayer) {
        Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
          debugController->EnableDebugLayer();
          IGL_LOG_INFO("D3D12 Debug Layer enabled for testing\n");
        }
      }

      // Create DXGI factory
      UINT dxgiFactoryFlags = 0;
      if (enableDebugLayer) {
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
      }

      Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
      HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory));
      if (FAILED(hr)) {
        return Result(Result::Code::RuntimeError, "Failed to create DXGI factory");
      }

      // Find adapter (prefer discrete GPU)
      Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
      for (UINT i = 0;; ++i) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> currentAdapter;
        if (factory->EnumAdapters1(i, &currentAdapter) == DXGI_ERROR_NOT_FOUND) {
          break;
        }

        DXGI_ADAPTER_DESC1 desc;
        currentAdapter->GetDesc1(&desc);

        // Skip software adapter
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
          continue;
        }

        // Check if adapter supports D3D12
        if (SUCCEEDED(D3D12CreateDevice(currentAdapter.Get(),
                                        D3D_FEATURE_LEVEL_11_0,
                                        _uuidof(ID3D12Device),
                                        nullptr))) {
          adapter = currentAdapter;
          IGL_LOG_INFO("D3D12 Test Device: Using adapter '%S'\n", desc.Description);
          break;
        }
      }

      if (!adapter) {
        return Result(Result::Code::RuntimeError, "No D3D12 compatible adapter found");
      }

      // Create D3D12 device
      Microsoft::WRL::ComPtr<ID3D12Device> device;
      hr = D3D12CreateDevice(adapter.Get(),
                             D3D_FEATURE_LEVEL_11_0,
                             IID_PPV_ARGS(&device));
      if (FAILED(hr)) {
        return Result(Result::Code::RuntimeError, "Failed to create D3D12 device");
      }

      // Create command queue
      D3D12_COMMAND_QUEUE_DESC queueDesc = {};
      queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
      queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

      Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
      hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
      if (FAILED(hr)) {
        return Result(Result::Code::RuntimeError, "Failed to create command queue");
      }

      // Create descriptor heaps for resource binding
      D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
      heapDesc.NumDescriptors = 1000; // Sufficient for tests
      heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
      heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

      Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap;
      hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&cbvSrvUavHeap));
      if (FAILED(hr)) {
        return Result(Result::Code::RuntimeError, "Failed to create CBV/SRV/UAV heap");
      }

      // Create sampler heap
      heapDesc.NumDescriptors = 100;
      heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
      heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

      Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> samplerHeap;
      hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&samplerHeap));
      if (FAILED(hr)) {
        return Result(Result::Code::RuntimeError, "Failed to create sampler heap");
      }

      // Create fence for synchronization
      Microsoft::WRL::ComPtr<ID3D12Fence> fence;
      hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
      if (FAILED(hr)) {
        return Result(Result::Code::RuntimeError, "Failed to create fence");
      }

      HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
      if (!fenceEvent) {
        return Result(Result::Code::RuntimeError, "Failed to create fence event");
      }

      // Store the created objects (using internal access to base class members)
      // Since we can't access private members directly, we'll use a workaround
      // by storing them in a derived context structure
      device_ = device;
      commandQueue_ = commandQueue;
      cbvSrvUavHeap_ = cbvSrvUavHeap;
      samplerHeap_ = samplerHeap;
      fence_ = fence;
      fenceEvent_ = fenceEvent;

      cbvSrvUavDescriptorSize_ =
          device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
      samplerDescriptorSize_ =
          device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

      IGL_LOG_INFO("D3D12 Test Device: Headless context initialized successfully\n");

      return Result();
    } catch (const std::exception& e) {
      return Result(Result::Code::RuntimeError, e.what());
    }
  }

 private:
  // Using protected/public members from base class via friend access
  using D3D12Context::device_;
  using D3D12Context::commandQueue_;
  using D3D12Context::cbvSrvUavHeap_;
  using D3D12Context::samplerHeap_;
  using D3D12Context::fence_;
  using D3D12Context::fenceEvent_;
  using D3D12Context::cbvSrvUavDescriptorSize_;
  using D3D12Context::samplerDescriptorSize_;
};

} // namespace

std::unique_ptr<igl::d3d12::Device> createTestDevice(bool enableDebugLayer) {
  auto ctx = std::make_unique<HeadlessD3D12Context>();

  Result result = ctx->initializeHeadless(enableDebugLayer);
  if (!result.isOk()) {
    IGL_LOG_ERROR("Failed to create D3D12 test device: %s\n", result.message.c_str());
    return nullptr;
  }

  return std::make_unique<igl::d3d12::Device>(std::move(ctx));
}

} // namespace igl::tests::util::device::d3d12
