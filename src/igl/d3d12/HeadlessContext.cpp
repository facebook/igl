/*
 * Minimal headless D3D12 context for unit tests (no swapchain / no HWND).
 */

#include <igl/d3d12/HeadlessContext.h>
#include <igl/d3d12/DescriptorHeapManager.h>

namespace igl::d3d12 {

HeadlessD3D12Context::~HeadlessD3D12Context() = default;

Result HeadlessD3D12Context::initializeHeadless(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;

  // Try to enable debug layer if available (ignore failures)
  {
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())))) {
      debugController->EnableDebugLayer();
      IGL_LOG_INFO("HeadlessD3D12Context: Debug layer enabled\n");
    }
  }

  // Create DXGI factory and prefer a high-performance hardware adapter
  HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory_.GetAddressOf()));
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create DXGI factory");
  }
  Microsoft::WRL::ComPtr<IDXGIFactory6> factory6;
  (void)dxgiFactory_->QueryInterface(IID_PPV_ARGS(factory6.GetAddressOf()));

  bool created = false;
  if (factory6.Get()) {
    for (UINT i = 0;; ++i) {
      Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
      if (FAILED(factory6->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                      IID_PPV_ARGS(adapter.GetAddressOf())))) {
        break;
      }
      DXGI_ADAPTER_DESC1 desc{};
      adapter->GetDesc1(&desc);
      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        continue;
      }
      if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                                      IID_PPV_ARGS(device_.GetAddressOf())))) {
        created = true;
        IGL_LOG_INFO("HeadlessD3D12Context: Created device on HW adapter (FL 12.0)\n");
        break;
      }
    }
  }
  if (!created) {
    for (UINT i = 0;; ++i) {
      Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
      if (dxgiFactory_->EnumAdapters1(i, adapter.GetAddressOf()) == DXGI_ERROR_NOT_FOUND) {
        break;
      }
      DXGI_ADAPTER_DESC1 desc{};
      adapter->GetDesc1(&desc);
      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        continue;
      }
      if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                                      IID_PPV_ARGS(device_.GetAddressOf())))) {
        created = true;
        IGL_LOG_INFO("HeadlessD3D12Context: Created device on HW adapter via EnumAdapters1 (FL 12.0)\n");
        break;
      }
    }
  }
  if (!created) {
    Microsoft::WRL::ComPtr<IDXGIAdapter> warp;
    if (SUCCEEDED(dxgiFactory_->EnumWarpAdapter(IID_PPV_ARGS(warp.GetAddressOf()))) &&
        SUCCEEDED(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0,
                                    IID_PPV_ARGS(device_.GetAddressOf())))) {
      created = true;
      IGL_LOG_INFO("HeadlessD3D12Context: WARP device created (FL 11.0)\n");
    }
  }
  if (!created) {
    return Result(Result::Code::RuntimeError, "Failed to create any D3D12 device");
  }

#ifdef _DEBUG
  {
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf())))) {
      infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, FALSE);
      infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, FALSE);
      infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);
    }
  }
#endif

  // Create command queue
  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(commandQueue_.GetAddressOf()));
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create command queue");
  }

  // Create descriptor heaps (smaller defaults for tests). Allow override via env vars.
  UINT heapSize = 256; // default CBV/SRV/UAV
  {
    char buf[32] = {};
    const DWORD n = GetEnvironmentVariableA("IGL_D3D12_CBV_SRV_UAV_HEAP_SIZE", buf, sizeof(buf));
    if (n > 0) {
      heapSize = std::max<UINT>(64, static_cast<UINT>(strtoul(buf, nullptr, 10)));
    }
  }
  D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavHeapDesc = {};
  cbvSrvUavHeapDesc.NumDescriptors = heapSize;
  cbvSrvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  cbvSrvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  cbvSrvUavHeapDesc.NodeMask = 0;
  hr = device_->CreateDescriptorHeap(&cbvSrvUavHeapDesc, IID_PPV_ARGS(cbvSrvUavHeap_.GetAddressOf()));
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create CBV/SRV/UAV descriptor heap");
  }
  cbvSrvUavDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  UINT samplerHeapSize = 16; // default samplers
  {
    char buf[32] = {};
    const DWORD n = GetEnvironmentVariableA("IGL_D3D12_SAMPLER_HEAP_SIZE", buf, sizeof(buf));
    if (n > 0) {
      samplerHeapSize = std::max<UINT>(8, static_cast<UINT>(strtoul(buf, nullptr, 10)));
    }
  }
  D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
  samplerHeapDesc.NumDescriptors = samplerHeapSize;
  samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
  samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  samplerHeapDesc.NodeMask = 0;
  hr = device_->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(samplerHeap_.GetAddressOf()));
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create sampler descriptor heap");
  }
  samplerDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

  // Fence for GPU synchronization
  hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf()));
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create fence");
  }
  fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!fenceEvent_) {
    return Result(Result::Code::RuntimeError, "Failed to create fence event");
  }

  // Create descriptor heap manager with the same sizes for consistency
  {
    DescriptorHeapManager::Sizes sz{};
    sz.cbvSrvUav = heapSize;
    sz.samplers = samplerHeapSize;
    sz.rtvs = 64;
    sz.dsvs = 32;
    descriptorHeaps_ = std::make_unique<DescriptorHeapManager>();
    const Result r = descriptorHeaps_->initialize(device_.Get(), sz);
    if (!r.isOk()) {
      IGL_LOG_ERROR("HeadlessD3D12Context: Failed to initialize descriptor heap manager: %s\n",
                    r.message.c_str());
      // Not fatal for Phase 1, continue without manager
      descriptorHeaps_.reset();
    }
    // Expose manager to base context for consumers that only see D3D12Context
    heapMgr_ = descriptorHeaps_.get();
  }

  IGL_LOG_INFO("HeadlessD3D12Context: Initialization complete\n");
  return Result();
}

} // namespace igl::d3d12

