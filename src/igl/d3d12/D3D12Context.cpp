/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/D3D12Context.h>

#include <stdexcept>

namespace igl::d3d12 {

D3D12Context::~D3D12Context() {
  // Wait for GPU to finish before cleanup
  waitForGPU();

  if (fenceEvent_) {
    CloseHandle(fenceEvent_);
  }

  // ComPtr handles cleanup automatically
}

Result D3D12Context::initialize(HWND hwnd, uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;

  try {
    IGL_LOG_INFO("D3D12Context: Creating D3D12 device...\n");
    createDevice();
    IGL_LOG_INFO("D3D12Context: Device created successfully\n");

    IGL_LOG_INFO("D3D12Context: Creating command queue...\n");
    createCommandQueue();
    IGL_LOG_INFO("D3D12Context: Command queue created successfully\n");

    IGL_LOG_INFO("D3D12Context: Creating swapchain (%ux%u)...\n", width, height);
    createSwapChain(hwnd, width, height);
    IGL_LOG_INFO("D3D12Context: Swapchain created successfully\n");

    IGL_LOG_INFO("D3D12Context: Creating RTV heap...\n");
    createRTVHeap();
    IGL_LOG_INFO("D3D12Context: RTV heap created successfully\n");

    IGL_LOG_INFO("D3D12Context: Creating back buffers...\n");
    createBackBuffers();
    IGL_LOG_INFO("D3D12Context: Back buffers created successfully\n");

    IGL_LOG_INFO("D3D12Context: Creating descriptor heaps...\n");
    createDescriptorHeaps();
    IGL_LOG_INFO("D3D12Context: Descriptor heaps created successfully\n");

    IGL_LOG_INFO("D3D12Context: Creating fence for GPU synchronization...\n");
    HRESULT hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf()));
    if (FAILED(hr)) {
      throw std::runtime_error("Failed to create fence");
    }
    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent_) {
      throw std::runtime_error("Failed to create fence event");
    }
    IGL_LOG_INFO("D3D12Context: Fence created successfully\n");

    IGL_LOG_INFO("D3D12Context: Initialization complete!\n");
  } catch (const std::exception& e) {
    IGL_LOG_ERROR("D3D12Context initialization failed: %s\n", e.what());
    return Result(Result::Code::RuntimeError, e.what());
  }

  return Result();
}

Result D3D12Context::resize(uint32_t width, uint32_t height) {
  if (width == width_ && height == height_) {
    return Result();
  }

  width_ = width;
  height_ = height;

  try {
    // Release old back buffers
    for (UINT i = 0; i < kMaxFramesInFlight; i++) {
      renderTargets_[i].Reset();
    }

    // Resize swapchain buffers
    HRESULT hr = swapChain_->ResizeBuffers(
        kMaxFramesInFlight,
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        0);

    if (FAILED(hr)) {
      throw std::runtime_error("Failed to resize swapchain buffers");
    }

    // Recreate back buffer views
    createBackBuffers();
  } catch (const std::exception& e) {
    return Result(Result::Code::RuntimeError, e.what());
  }

  return Result();
}

void D3D12Context::createDevice() {
  // Re-enable debug layer to capture validation messages
  Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())))) {
    debugController->EnableDebugLayer();
    IGL_LOG_INFO("D3D12Context: Debug layer ENABLED (to capture validation messages)\n");
  } else {
    IGL_LOG_ERROR("D3D12Context: Failed to get D3D12 debug interface - Graphics Tools may not be installed\n");
  }

  // Create DXGI factory
  HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory_.GetAddressOf()));
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to create DXGI factory");
  }

  // Enumerate adapters
  for (UINT i = 0; dxgiFactory_->EnumAdapters1(i, adapter_.GetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i) {
    DXGI_ADAPTER_DESC1 desc;
    adapter_->GetDesc1(&desc);

    // Skip software adapter
    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
      continue;
    }

    // Try to create device
    if (SUCCEEDED(D3D12CreateDevice(adapter_.Get(), D3D_FEATURE_LEVEL_12_0,
                                     _uuidof(ID3D12Device), nullptr))) {
      break;
    }
  }

  // Create D3D12 device
  hr = D3D12CreateDevice(adapter_.Get(), D3D_FEATURE_LEVEL_12_0,
                          IID_PPV_ARGS(device_.GetAddressOf()));
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to create D3D12 device");
  }

#ifdef _DEBUG
  // Setup info queue to print validation messages without breaking
  Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
  if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf())))) {
    // DO NOT break on errors - this causes hangs when no debugger is attached
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, FALSE);
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, FALSE);
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);

    // Filter out INFO messages to reduce noise (still show CORRUPTION, ERROR, WARNING)
    D3D12_MESSAGE_SEVERITY severities[] = {
      D3D12_MESSAGE_SEVERITY_INFO
    };

    D3D12_INFO_QUEUE_FILTER filter = {};
    filter.DenyList.NumSeverities = 1;
    filter.DenyList.pSeverityList = severities;
    infoQueue->PushStorageFilter(&filter);

    IGL_LOG_INFO("D3D12Context: Info queue configured (break on error: DISABLED)\n");
  }
#endif
}

void D3D12Context::createCommandQueue() {
  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

  HRESULT hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(commandQueue_.GetAddressOf()));
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to create command queue");
  }
}

void D3D12Context::createSwapChain(HWND hwnd, uint32_t width, uint32_t height) {
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
  swapChainDesc.Width = width;
  swapChainDesc.Height = height;
  swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapChainDesc.Stereo = FALSE;
  swapChainDesc.SampleDesc.Count = 1;
  swapChainDesc.SampleDesc.Quality = 0;
  swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.BufferCount = kMaxFramesInFlight;
  swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

  Microsoft::WRL::ComPtr<IDXGISwapChain1> tempSwapChain;
  HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(
      commandQueue_.Get(),
      hwnd,
      &swapChainDesc,
      nullptr,
      nullptr,
      tempSwapChain.GetAddressOf()
  );

  if (FAILED(hr)) {
    throw std::runtime_error("Failed to create swapchain");
  }

  // Cast to IDXGISwapChain3
  hr = tempSwapChain->QueryInterface(IID_PPV_ARGS(swapChain_.GetAddressOf()));
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to query IDXGISwapChain3 interface");
  }
}

void D3D12Context::createRTVHeap() {
  D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
  heapDesc.NumDescriptors = kMaxFramesInFlight;
  heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

  HRESULT hr = device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(rtvHeap_.GetAddressOf()));
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to create RTV heap");
  }

  rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void D3D12Context::createBackBuffers() {
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();

  for (UINT i = 0; i < kMaxFramesInFlight; i++) {
    HRESULT hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(renderTargets_[i].GetAddressOf()));
    if (FAILED(hr)) {
      throw std::runtime_error("Failed to get swapchain buffer");
    }

    device_->CreateRenderTargetView(renderTargets_[i].Get(), nullptr, rtvHandle);
    rtvHandle.ptr += rtvDescriptorSize_;
  }
}

void D3D12Context::createDescriptorHeaps() {
  // Create CBV/SRV/UAV descriptor heap (for textures and constant buffers)
  D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavHeapDesc = {};
  cbvSrvUavHeapDesc.NumDescriptors = 1000; // Large pool for textures and constant buffers
  cbvSrvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  cbvSrvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  cbvSrvUavHeapDesc.NodeMask = 0;

  HRESULT hr = device_->CreateDescriptorHeap(&cbvSrvUavHeapDesc,
                                             IID_PPV_ARGS(cbvSrvUavHeap_.GetAddressOf()));
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to create CBV/SRV/UAV descriptor heap");
  }

  cbvSrvUavDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  // Create Sampler descriptor heap
  D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
  samplerHeapDesc.NumDescriptors = 16; // Sufficient for typical sampler needs
  samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
  samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  samplerHeapDesc.NodeMask = 0;

  hr = device_->CreateDescriptorHeap(&samplerHeapDesc,
                                     IID_PPV_ARGS(samplerHeap_.GetAddressOf()));
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to create Sampler descriptor heap");
  }

  samplerDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
}

uint32_t D3D12Context::getCurrentBackBufferIndex() const {
  if (swapChain_.Get() == nullptr) {
    return 0;
  }
  return swapChain_->GetCurrentBackBufferIndex();
}

ID3D12Resource* D3D12Context::getCurrentBackBuffer() const {
  uint32_t index = getCurrentBackBufferIndex();
  if (index >= kMaxFramesInFlight) {
    IGL_LOG_ERROR("getCurrentBackBuffer(): index %u >= kMaxFramesInFlight %u\n", index, kMaxFramesInFlight);
    return nullptr;
  }

  ID3D12Resource* resource = renderTargets_[index].Get();
  static int logCount = 0;
  if (logCount < 3) {
    IGL_LOG_INFO("getCurrentBackBuffer(): index=%u, resource=%p\n", index, (void*)resource);
    logCount++;
  }

  return resource;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Context::getCurrentRTV() const {
  if (rtvHeap_.Get() == nullptr) {
    return {0};
  }
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
  rtv.ptr += getCurrentBackBufferIndex() * rtvDescriptorSize_;
  return rtv;
}

void D3D12Context::waitForGPU() {
  if (!fence_.Get() || !commandQueue_.Get()) {
    return;
  }

  // Signal and increment the fence value
  const UINT64 fenceToWaitFor = ++fenceValue_;
  commandQueue_->Signal(fence_.Get(), fenceToWaitFor);

  // Wait until the fence is crossed
  if (fence_->GetCompletedValue() < fenceToWaitFor) {
    fence_->SetEventOnCompletion(fenceToWaitFor, fenceEvent_);
    WaitForSingleObject(fenceEvent_, INFINITE);
  }
}

} // namespace igl::d3d12
