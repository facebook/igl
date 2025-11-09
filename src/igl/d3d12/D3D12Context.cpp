/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/D3D12Context.h>
#include <igl/d3d12/DescriptorHeapManager.h>

#include <stdexcept>
#include <string>

namespace igl::d3d12 {

// Static member initialization
D3D12Context::ResourceStats D3D12Context::resourceStats_;
std::mutex D3D12Context::resourceStatsMutex_;

D3D12Context::~D3D12Context() {
  // Wait for GPU to finish before cleanup
  waitForGPU();

  // Clean up owned descriptor heap manager
  delete ownedHeapMgr_;
  ownedHeapMgr_ = nullptr;
  heapMgr_ = nullptr;

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

    IGL_LOG_INFO("D3D12Context: Creating command signatures...\n");
    createCommandSignatures();
    IGL_LOG_INFO("D3D12Context: Command signatures created successfully\n");

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

    // Create per-frame command allocators (following Microsoft's D3D12HelloFrameBuffering pattern)
    IGL_LOG_INFO("D3D12Context: Creating per-frame command allocators...\n");
    for (UINT i = 0; i < kMaxFramesInFlight; i++) {
      hr = device_->CreateCommandAllocator(
          D3D12_COMMAND_LIST_TYPE_DIRECT,
          IID_PPV_ARGS(frameContexts_[i].allocator.GetAddressOf()));
      if (FAILED(hr)) {
        throw std::runtime_error("Failed to create command allocator for frame " + std::to_string(i));
      }
      IGL_LOG_INFO("D3D12Context: Created command allocator for frame %u\n", i);
    }
    IGL_LOG_INFO("D3D12Context: Per-frame command allocators created successfully\n");

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
    // Wait for all GPU work to complete before releasing backbuffers
    // This prevents DXGI_ERROR_DEVICE_REMOVED when GPU is still rendering to old buffers
    if (fence_.Get() && fenceEvent_) {
      const UINT64 currentFence = fenceValue_;
      commandQueue_->Signal(fence_.Get(), currentFence);

      if (fence_->GetCompletedValue() < currentFence) {
        fence_->SetEventOnCompletion(currentFence, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
      }
    }

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
  // DO NOT enable experimental features in windowed mode - it breaks swapchain creation!
  // Experimental features are ONLY enabled in HeadlessD3D12Context for unit tests
  // Windowed render sessions use signed DXIL (via IDxcValidator) which doesn't need experimental mode

  // Initialize DXGI factory flags for debug builds
  UINT dxgiFactoryFlags = 0;

  // Re-enable debug layer to capture validation messages
  Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())))) {
    debugController->EnableDebugLayer();
    IGL_LOG_INFO("D3D12Context: Debug layer ENABLED (to capture validation messages)\n");

#ifdef _DEBUG
    // Enable DXGI debug layer (C-009: critical for DXGI validation)
    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    IGL_LOG_INFO("D3D12Context: DXGI debug layer ENABLED (swap chain validation)\n");
#endif

    // Optional: Enable GPU-Based Validation (controlled by env var)
    // WARNING: This significantly impacts performance (10-100x slower)
    const char* enableGBV = std::getenv("IGL_D3D12_GPU_BASED_VALIDATION");
    if (enableGBV && std::string(enableGBV) == "1") {
      Microsoft::WRL::ComPtr<ID3D12Debug1> debugController1;
      if (SUCCEEDED(debugController->QueryInterface(IID_PPV_ARGS(debugController1.GetAddressOf())))) {
        debugController1->SetEnableGPUBasedValidation(TRUE);
        IGL_LOG_INFO("D3D12Context: GPU-Based Validation ENABLED (may slow down rendering significantly)\n");
      }
    }
  } else {
    IGL_LOG_ERROR("D3D12Context: Failed to get D3D12 debug interface - Graphics Tools may not be installed\n");
  }

#ifdef _DEBUG
  // Enable DRED (Device Removed Extended Data) for better crash diagnostics
  Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dredSettings;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(dredSettings.GetAddressOf())))) {
    dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    IGL_LOG_INFO("D3D12Context: DRED enabled (AutoBreadcrumbs + PageFault tracking)\n");
  }
#endif

  // Create DXGI factory with debug flag in debug builds (C-009)
  HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(dxgiFactory_.GetAddressOf()));
  if (FAILED(hr)) {
    throw std::runtime_error("Failed to create DXGI factory");
  }

  // Helper lambda to query highest supported feature level for an adapter
  auto getHighestFeatureLevel = [](IDXGIAdapter1* adapter) -> D3D_FEATURE_LEVEL {
    // Try creating device with FL 11.0 first (minimum required)
    Microsoft::WRL::ComPtr<ID3D12Device> tempDevice;
    if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(tempDevice.GetAddressOf())))) {
      return static_cast<D3D_FEATURE_LEVEL>(0); // Adapter doesn't support D3D12
    }

    // Query supported feature levels (check from highest to lowest)
    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevelsInfo = {};
    featureLevelsInfo.NumFeatureLevels = static_cast<UINT>(std::size(featureLevels));
    featureLevelsInfo.pFeatureLevelsRequested = featureLevels;

    if (SUCCEEDED(tempDevice->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS,
                                                   &featureLevelsInfo,
                                                   sizeof(featureLevelsInfo)))) {
      return featureLevelsInfo.MaxSupportedFeatureLevel;
    }

    return D3D_FEATURE_LEVEL_11_0; // Fallback to minimum
  };

  // Helper to get feature level string
  auto featureLevelToString = [](D3D_FEATURE_LEVEL level) -> const char* {
    switch (level) {
      case D3D_FEATURE_LEVEL_12_2: return "12.2";
      case D3D_FEATURE_LEVEL_12_1: return "12.1";
      case D3D_FEATURE_LEVEL_12_0: return "12.0";
      case D3D_FEATURE_LEVEL_11_1: return "11.1";
      case D3D_FEATURE_LEVEL_11_0: return "11.0";
      default: return "Unknown";
    }
  };

  // Prefer high-performance hardware adapter first; fallback to WARP
  bool created = false;
  D3D_FEATURE_LEVEL selectedFeatureLevel = D3D_FEATURE_LEVEL_11_0;

  Microsoft::WRL::ComPtr<IDXGIFactory6> factory6;
  (void)dxgiFactory_->QueryInterface(IID_PPV_ARGS(factory6.GetAddressOf()));
  if (factory6.Get()) {
    for (UINT i = 0;; ++i) {
      Microsoft::WRL::ComPtr<IDXGIAdapter1> cand;
      if (FAILED(factory6->EnumAdapterByGpuPreference(i,
                                                      DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                      IID_PPV_ARGS(cand.GetAddressOf())))) {
        break;
      }
      DXGI_ADAPTER_DESC1 desc{};
      cand->GetDesc1(&desc);
      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        continue;
      }

      // Probe for highest supported feature level
      D3D_FEATURE_LEVEL featureLevel = getHighestFeatureLevel(cand.Get());
      if (featureLevel == static_cast<D3D_FEATURE_LEVEL>(0)) {
        continue; // Adapter doesn't support D3D12
      }

      if (SUCCEEDED(D3D12CreateDevice(cand.Get(), featureLevel,
                                      IID_PPV_ARGS(device_.GetAddressOf())))) {
        created = true;
        selectedFeatureLevel = featureLevel;
        IGL_LOG_INFO("D3D12Context: Using HW adapter (FL %s)\n", featureLevelToString(featureLevel));
        break;
      }
    }
  }
  if (!created) {
    // Fallback: enumerate adapters
    for (UINT i = 0; dxgiFactory_->EnumAdapters1(i, adapter_.GetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i) {
      DXGI_ADAPTER_DESC1 desc{};
      adapter_->GetDesc1(&desc);
      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        continue;
      }

      // Probe for highest supported feature level
      D3D_FEATURE_LEVEL featureLevel = getHighestFeatureLevel(adapter_.Get());
      if (featureLevel == static_cast<D3D_FEATURE_LEVEL>(0)) {
        continue; // Adapter doesn't support D3D12
      }

      if (SUCCEEDED(D3D12CreateDevice(adapter_.Get(), featureLevel,
                                      IID_PPV_ARGS(device_.GetAddressOf())))) {
        created = true;
        selectedFeatureLevel = featureLevel;
        IGL_LOG_INFO("D3D12Context: Using HW adapter via EnumAdapters1 (FL %s)\n", featureLevelToString(featureLevel));
        break;
      }
    }
  }
  if (!created) {
    // WARP fallback - probe for highest supported FL
    Microsoft::WRL::ComPtr<IDXGIAdapter1> warp;
    if (SUCCEEDED(dxgiFactory_->EnumWarpAdapter(IID_PPV_ARGS(warp.GetAddressOf())))) {
      D3D_FEATURE_LEVEL featureLevel = getHighestFeatureLevel(warp.Get());
      if (featureLevel != static_cast<D3D_FEATURE_LEVEL>(0) &&
          SUCCEEDED(D3D12CreateDevice(warp.Get(), featureLevel,
                                      IID_PPV_ARGS(device_.GetAddressOf())))) {
        created = true;
        selectedFeatureLevel = featureLevel;
        IGL_LOG_INFO("D3D12Context: Using WARP adapter (FL %s)\n", featureLevelToString(featureLevel));
      }
    }
  }
  if (!created) {
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

    // Filter out INFO messages and unsigned shader messages (for DXC development)
    D3D12_MESSAGE_SEVERITY severities[] = {
      D3D12_MESSAGE_SEVERITY_INFO
    };

    // Filter out messages about unsigned shaders (DXC in development mode)
    D3D12_MESSAGE_ID denyIds[] = {
      D3D12_MESSAGE_ID_CREATEVERTEXSHADER_INVALIDSHADERBYTECODE,       // Unsigned VS
      D3D12_MESSAGE_ID_CREATEPIXELSHADER_INVALIDSHADERBYTECODE,        // Unsigned PS
      D3D12_MESSAGE_ID_CREATECOMPUTESHADER_INVALIDSHADERBYTECODE,      // Unsigned CS
      D3D12_MESSAGE_ID_CREATEINPUTLAYOUT_UNPARSEABLEINPUTSIGNATURE     // DX IL input signature
    };

    D3D12_INFO_QUEUE_FILTER filter = {};
    filter.DenyList.NumSeverities = 1;
    filter.DenyList.pSeverityList = severities;
    filter.DenyList.NumIDs = 4;
    filter.DenyList.pIDList = denyIds;
    infoQueue->PushStorageFilter(&filter);

    IGL_LOG_INFO("D3D12Context: Info queue configured (severity breaks DISABLED, unsigned shader messages filtered)\n");
  }
#endif

  // Query root signature capabilities (P0_DX12-003)
  // This is critical for Tier-1 devices which don't support unbounded descriptor ranges
  IGL_LOG_INFO("D3D12Context: Querying root signature capabilities...\n");

  // Query highest supported root signature version
  D3D12_FEATURE_DATA_ROOT_SIGNATURE featureDataRootSig = {};
  featureDataRootSig.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

  hr = device_->CheckFeatureSupport(
      D3D12_FEATURE_ROOT_SIGNATURE,
      &featureDataRootSig,
      sizeof(featureDataRootSig));

  if (SUCCEEDED(hr)) {
    highestRootSignatureVersion_ = featureDataRootSig.HighestVersion;
    IGL_LOG_INFO("  Highest Root Signature Version: %s\n",
                 highestRootSignatureVersion_ == D3D_ROOT_SIGNATURE_VERSION_1_1 ? "1.1" : "1.0");
  } else {
    // If query fails, assume v1.0 (most conservative)
    highestRootSignatureVersion_ = D3D_ROOT_SIGNATURE_VERSION_1_0;
    IGL_LOG_INFO("  Root Signature query failed (assuming v1.0)\n");
  }

  // Query resource binding tier
  D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
  hr = device_->CheckFeatureSupport(
      D3D12_FEATURE_D3D12_OPTIONS,
      &options,
      sizeof(options));

  if (SUCCEEDED(hr)) {
    resourceBindingTier_ = options.ResourceBindingTier;
    const char* tierName = "Unknown";
    switch (resourceBindingTier_) {
      case D3D12_RESOURCE_BINDING_TIER_1: tierName = "Tier 1 (bounded descriptors required)"; break;
      case D3D12_RESOURCE_BINDING_TIER_2: tierName = "Tier 2 (unbounded arrays except samplers)"; break;
      case D3D12_RESOURCE_BINDING_TIER_3: tierName = "Tier 3 (fully unbounded)"; break;
    }
    IGL_LOG_INFO("  Resource Binding Tier: %s\n", tierName);
  } else {
    // If query fails, assume Tier 1 (most conservative)
    resourceBindingTier_ = D3D12_RESOURCE_BINDING_TIER_1;
    IGL_LOG_INFO("  Resource Binding Tier query failed (assuming Tier 1)\n");
  }

  IGL_LOG_INFO("D3D12Context: Root signature capabilities detected successfully\n");
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
  // Use BGRA_UNORM (non-sRGB) for maximum compatibility with all display adapters
  // Vulkan baselines use BGRA channel ordering for swapchain and MRT targets
  swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  swapChainDesc.Stereo = FALSE;
  swapChainDesc.SampleDesc.Count = 1;
  swapChainDesc.SampleDesc.Quality = 0;
  swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.BufferCount = kMaxFramesInFlight;
  swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

  // Query tearing support capability (required for variable refresh rate displays)
  // This capability must be queried before creating the swapchain
  BOOL allowTearing = FALSE;
  Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
  if (SUCCEEDED(dxgiFactory_.Get()->QueryInterface(IID_PPV_ARGS(factory5.GetAddressOf())))) {
    if (SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                                &allowTearing,
                                                sizeof(allowTearing)))) {
      tearingSupported_ = (allowTearing == TRUE);
      if (tearingSupported_) {
        IGL_LOG_INFO("D3D12Context: Tearing support available (variable refresh rate)\n");
      }
    }
  }

  // Set swapchain tearing flag if supported (required to use DXGI_PRESENT_ALLOW_TEARING)
  // Without this flag, using DXGI_PRESENT_ALLOW_TEARING in Present() is invalid
  swapChainDesc.Flags = tearingSupported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

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
    IGL_LOG_ERROR("CreateSwapChainForHwnd failed: 0x%08X, trying legacy CreateSwapChain\n", (unsigned)hr);
    // Fallback: legacy CreateSwapChain
    DXGI_SWAP_CHAIN_DESC legacy = {};
    legacy.BufferDesc.Width = width;
    legacy.BufferDesc.Height = height;
    legacy.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    legacy.BufferDesc.RefreshRate.Numerator = 60;
    legacy.BufferDesc.RefreshRate.Denominator = 1;
    legacy.SampleDesc.Count = 1;
    legacy.SampleDesc.Quality = 0;
    legacy.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    legacy.BufferCount = kMaxFramesInFlight;
    legacy.OutputWindow = hwnd;
    legacy.Windowed = TRUE;
    legacy.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    legacy.Flags = 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain> legacySwap;
    HRESULT hr2 = dxgiFactory_->CreateSwapChain(commandQueue_.Get(), &legacy, legacySwap.GetAddressOf());
    if (FAILED(hr2)) {
      char buf2[160] = {};
      snprintf(buf2, sizeof(buf2), "Failed to create swapchain (hr=0x%08X / 0x%08X)", (unsigned)hr, (unsigned)hr2);
      throw std::runtime_error(buf2);
    }
    // Try to QI to IDXGISwapChain3
    hr2 = legacySwap->QueryInterface(IID_PPV_ARGS(swapChain_.GetAddressOf()));
    if (FAILED(hr2)) {
      char buf3[160] = {};
      snprintf(buf3, sizeof(buf3), "Failed to query IDXGISwapChain3 (hr=0x%08X)", (unsigned)hr2);
      throw std::runtime_error(buf3);
    }
    return;
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

    // Pre-creation validation (TASK_P0_DX12-004)
    IGL_DEBUG_ASSERT(device_.Get() != nullptr, "Device is null before CreateRenderTargetView");
    IGL_DEBUG_ASSERT(renderTargets_[i].Get() != nullptr, "Swapchain buffer is null");
    IGL_DEBUG_ASSERT(rtvHandle.ptr != 0, "RTV descriptor handle is invalid");

    device_->CreateRenderTargetView(renderTargets_[i].Get(), nullptr, rtvHandle);
    rtvHandle.ptr += rtvDescriptorSize_;
  }
}

void D3D12Context::createDescriptorHeaps() {
  // Cache descriptor sizes
  cbvSrvUavDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  samplerDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

  // Create per-frame shader-visible descriptor heaps (following Microsoft MiniEngine pattern)
  // Each frame gets its own isolated heaps to prevent descriptor conflicts between frames
  // C-001: Now creates initial page with dynamic growth support
  IGL_LOG_INFO("D3D12Context: Creating per-frame descriptor heaps with dynamic growth support...\n");

  for (UINT i = 0; i < kMaxFramesInFlight; i++) {
    // CBV/SRV/UAV heap: Start with one page of kDescriptorsPerPage descriptors
    // Additional pages will be allocated on-demand up to kMaxHeapPages
    {
      Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> initialHeap;
      Result result = allocateDescriptorHeapPage(
          D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
          kDescriptorsPerPage,
          &initialHeap);

      if (!result.isOk()) {
        throw std::runtime_error("Failed to create initial CBV/SRV/UAV heap page for frame " + std::to_string(i));
      }

      // Initialize page vector with first page
      frameContexts_[i].cbvSrvUavHeapPages.clear();
      frameContexts_[i].cbvSrvUavHeapPages.emplace_back(initialHeap, kDescriptorsPerPage);
      frameContexts_[i].currentCbvSrvUavPageIndex = 0;

      IGL_LOG_INFO("  Frame %u: Created initial CBV/SRV/UAV heap page (%u descriptors, max %u pages = %u total)\n",
                   i, kDescriptorsPerPage, kMaxHeapPages, kMaxDescriptorsPerFrame);
    }

    // Sampler heap: kSamplerHeapSize descriptors
    {
      D3D12_DESCRIPTOR_HEAP_DESC desc = {};
      desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
      desc.NumDescriptors = kSamplerHeapSize;  // P0_DX12-FIND-02: Use constant for bounds checking
      desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
      desc.NodeMask = 0;

      HRESULT hr = device_->CreateDescriptorHeap(&desc,
          IID_PPV_ARGS(frameContexts_[i].samplerHeap.GetAddressOf()));
      if (FAILED(hr)) {
        throw std::runtime_error("Failed to create per-frame Sampler heap for frame " + std::to_string(i));
      }
      IGL_LOG_INFO("  Frame %u: Created Sampler heap (%u descriptors)\n", i, kSamplerHeapSize);
    }
  }

  IGL_LOG_INFO("D3D12Context: Per-frame descriptor heaps created successfully\n");
  IGL_LOG_INFO("  Total memory: 3 frames × (1024 CBV/SRV/UAV + %u Samplers) × 32 bytes ≈ %u KB\n",
               kMaxSamplers, (3 * (kCbvSrvUavHeapSize + kMaxSamplers) * 32) / 1024);

  IGL_LOG_INFO("D3D12Context: Creating descriptor heap manager...\n");

  // Create descriptor heap manager to manage allocations for CPU-visible heaps (RTV/DSV)
  DescriptorHeapManager::Sizes sizes{};
  sizes.cbvSrvUav = 256;  // For CPU-visible staging (not used for shader-visible)
  sizes.samplers = 16;    // For CPU-visible staging (not used for shader-visible)
  sizes.rtvs = 64;        // Reasonable defaults for windowed rendering
  sizes.dsvs = 32;

  ownedHeapMgr_ = new DescriptorHeapManager();
  Result result = ownedHeapMgr_->initialize(device_.Get(), sizes);
  if (!result.isOk()) {
    IGL_LOG_ERROR("D3D12Context: Failed to initialize descriptor heap manager: %s\n",
                  result.message.c_str());
    delete ownedHeapMgr_;
    ownedHeapMgr_ = nullptr;
  } else {
    heapMgr_ = ownedHeapMgr_;
    IGL_LOG_INFO("D3D12Context: Descriptor heap manager created successfully\n");
  }
}

void D3D12Context::createCommandSignatures() {
  // Create command signature for DrawInstanced (multiDrawIndirect)
  // D3D12_DRAW_ARGUMENTS: { VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation }
  {
    D3D12_INDIRECT_ARGUMENT_DESC drawArg = {};
    drawArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

    D3D12_COMMAND_SIGNATURE_DESC drawSigDesc = {};
    drawSigDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);  // 16 bytes (4 x UINT)
    drawSigDesc.NumArgumentDescs = 1;
    drawSigDesc.pArgumentDescs = &drawArg;
    drawSigDesc.NodeMask = 0;

    HRESULT hr = device_->CreateCommandSignature(
        &drawSigDesc,
        nullptr,  // No root signature needed for simple draw commands
        IID_PPV_ARGS(drawIndirectSignature_.GetAddressOf()));

    if (FAILED(hr)) {
      throw std::runtime_error("Failed to create draw indirect command signature");
    }
    IGL_LOG_INFO("D3D12Context: Created draw indirect command signature (stride: %u bytes)\n",
                 drawSigDesc.ByteStride);
  }

  // Create command signature for DrawIndexedInstanced (multiDrawIndexedIndirect)
  // D3D12_DRAW_INDEXED_ARGUMENTS: { IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation }
  {
    D3D12_INDIRECT_ARGUMENT_DESC drawIndexedArg = {};
    drawIndexedArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

    D3D12_COMMAND_SIGNATURE_DESC drawIndexedSigDesc = {};
    drawIndexedSigDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);  // 20 bytes (5 x UINT)
    drawIndexedSigDesc.NumArgumentDescs = 1;
    drawIndexedSigDesc.pArgumentDescs = &drawIndexedArg;
    drawIndexedSigDesc.NodeMask = 0;

    HRESULT hr = device_->CreateCommandSignature(
        &drawIndexedSigDesc,
        nullptr,  // No root signature needed for simple draw commands
        IID_PPV_ARGS(drawIndexedIndirectSignature_.GetAddressOf()));

    if (FAILED(hr)) {
      throw std::runtime_error("Failed to create draw indexed indirect command signature");
    }
    IGL_LOG_INFO("D3D12Context: Created draw indexed indirect command signature (stride: %u bytes)\n",
                 drawIndexedSigDesc.ByteStride);
  }
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

void D3D12Context::trackResourceCreation(const char* type, size_t sizeBytes) {
  std::lock_guard<std::mutex> lock(resourceStatsMutex_);
  if (strcmp(type, "Buffer") == 0) {
    resourceStats_.totalBuffersCreated++;
    resourceStats_.bufferMemoryBytes += sizeBytes;
  } else if (strcmp(type, "Texture") == 0) {
    resourceStats_.totalTexturesCreated++;
    resourceStats_.textureMemoryBytes += sizeBytes;
  } else if (strcmp(type, "SRV") == 0) {
    resourceStats_.totalSRVsCreated++;
  } else if (strcmp(type, "Sampler") == 0) {
    resourceStats_.totalSamplersCreated++;
  }
}

void D3D12Context::trackResourceDestruction(const char* type, size_t sizeBytes) {
  std::lock_guard<std::mutex> lock(resourceStatsMutex_);
  if (strcmp(type, "Buffer") == 0) {
    resourceStats_.totalBuffersDestroyed++;
    resourceStats_.bufferMemoryBytes -= sizeBytes;
  } else if (strcmp(type, "Texture") == 0) {
    resourceStats_.totalTexturesDestroyed++;
    resourceStats_.textureMemoryBytes -= sizeBytes;
  }
}

void D3D12Context::logResourceStats() {
  std::lock_guard<std::mutex> lock(resourceStatsMutex_);
  IGL_LOG_INFO("=== D3D12 Resource Statistics ===\n");
  IGL_LOG_INFO("  Buffers: %zu created, %zu destroyed (leaked: %zd)\n",
               resourceStats_.totalBuffersCreated,
               resourceStats_.totalBuffersDestroyed,
               (int64_t)resourceStats_.totalBuffersCreated - (int64_t)resourceStats_.totalBuffersDestroyed);
  IGL_LOG_INFO("  Textures: %zu created, %zu destroyed (leaked: %zd)\n",
               resourceStats_.totalTexturesCreated,
               resourceStats_.totalTexturesDestroyed,
               (int64_t)resourceStats_.totalTexturesCreated - (int64_t)resourceStats_.totalTexturesDestroyed);
  IGL_LOG_INFO("  SRVs created: %zu\n", resourceStats_.totalSRVsCreated);
  IGL_LOG_INFO("  Samplers created: %zu\n", resourceStats_.totalSamplersCreated);
  IGL_LOG_INFO("  Buffer memory: %.2f MB\n", resourceStats_.bufferMemoryBytes / (1024.0 * 1024.0));
  IGL_LOG_INFO("  Texture memory: %.2f MB\n", resourceStats_.textureMemoryBytes / (1024.0 * 1024.0));
  IGL_LOG_INFO("==================================\n");
}

// C-001: Allocate a new descriptor heap page for dynamic growth
Result D3D12Context::allocateDescriptorHeapPage(
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    uint32_t numDescriptors,
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>* outHeap) {
  if (!device_.Get()) {
    return Result{Result::Code::RuntimeError, "Device is null"};
  }

  D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
  heapDesc.Type = type;
  heapDesc.NumDescriptors = numDescriptors;
  heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  heapDesc.NodeMask = 0;

  HRESULT hr = device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(outHeap->GetAddressOf()));
  if (FAILED(hr)) {
    char errorMsg[256];
    snprintf(errorMsg, sizeof(errorMsg),
             "Failed to create descriptor heap page (type=%d, numDescriptors=%u): HRESULT=0x%08X",
             static_cast<int>(type), numDescriptors, static_cast<unsigned>(hr));
    return Result{Result::Code::RuntimeError, errorMsg};
  }

  return Result{};
}

} // namespace igl::d3d12
