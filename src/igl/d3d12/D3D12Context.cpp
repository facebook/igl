/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/D3D12Context.h>
#include <igl/d3d12/DescriptorHeapManager.h>

#include <cstdlib>
#include <string>

namespace igl::d3d12 {

// Static member initialization
D3D12Context::ResourceStats D3D12Context::resourceStats_;
std::mutex D3D12Context::resourceStatsMutex_;

// A-011: Helper function to probe highest supported feature level for an adapter
D3D_FEATURE_LEVEL D3D12Context::getHighestFeatureLevel(IDXGIAdapter1* adapter) {
  const D3D_FEATURE_LEVEL featureLevels[] = {
    D3D_FEATURE_LEVEL_12_2,
    D3D_FEATURE_LEVEL_12_1,
    D3D_FEATURE_LEVEL_12_0,
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
  };

  for (D3D_FEATURE_LEVEL fl : featureLevels) {
    if (SUCCEEDED(D3D12CreateDevice(adapter, fl, _uuidof(ID3D12Device), nullptr))) {
      return fl;
    }
  }

  return static_cast<D3D_FEATURE_LEVEL>(0);  // No supported feature level
}

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

  IGL_LOG_INFO("D3D12Context: Creating D3D12 device...\n");
  Result deviceResult = createDevice();
  if (!deviceResult.isOk()) {
    return deviceResult;
  }
  IGL_LOG_INFO("D3D12Context: Device created successfully\n");

  IGL_LOG_INFO("D3D12Context: Creating command queue...\n");
  Result queueResult = createCommandQueue();
  if (!queueResult.isOk()) {
    return queueResult;
  }
  IGL_LOG_INFO("D3D12Context: Command queue created successfully\n");

  IGL_LOG_INFO("D3D12Context: Creating swapchain (%ux%u)...\n", width, height);
  Result swapChainResult = createSwapChain(hwnd, width, height);
  if (!swapChainResult.isOk()) {
    return swapChainResult;
  }
  IGL_LOG_INFO("D3D12Context: Swapchain created successfully\n");

  IGL_LOG_INFO("D3D12Context: Creating RTV heap...\n");
  Result rtvResult = createRTVHeap();
  if (!rtvResult.isOk()) {
    return rtvResult;
  }
  IGL_LOG_INFO("D3D12Context: RTV heap created successfully\n");

  IGL_LOG_INFO("D3D12Context: Creating back buffers...\n");
  Result backBufferResult = createBackBuffers();
  if (!backBufferResult.isOk()) {
    return backBufferResult;
  }
  IGL_LOG_INFO("D3D12Context: Back buffers created successfully\n");

  IGL_LOG_INFO("D3D12Context: Creating descriptor heaps...\n");
  Result descriptorHeapResult = createDescriptorHeaps();
  if (!descriptorHeapResult.isOk()) {
    return descriptorHeapResult;
  }
  IGL_LOG_INFO("D3D12Context: Descriptor heaps created successfully\n");

  IGL_LOG_INFO("D3D12Context: Creating command signatures...\n");
  Result commandSigResult = createCommandSignatures();
  if (!commandSigResult.isOk()) {
    return commandSigResult;
  }
  IGL_LOG_INFO("D3D12Context: Command signatures created successfully\n");

  IGL_LOG_INFO("D3D12Context: Creating fence for GPU synchronization...\n");
  HRESULT hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12Context: Failed to create fence (HRESULT: 0x%08X)\n", static_cast<unsigned>(hr));
    IGL_DEBUG_ASSERT(false);
    return Result(Result::Code::RuntimeError, "Failed to create fence");
  }
  fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!fenceEvent_) {
    IGL_LOG_ERROR("D3D12Context: Failed to create fence event\n");
    IGL_DEBUG_ASSERT(false);
    return Result(Result::Code::RuntimeError, "Failed to create fence event");
  }
  IGL_LOG_INFO("D3D12Context: Fence created successfully\n");

  // Create per-frame command allocators (following Microsoft's D3D12HelloFrameBuffering pattern)
  IGL_LOG_INFO("D3D12Context: Creating per-frame command allocators...\n");
  for (UINT i = 0; i < kMaxFramesInFlight; i++) {
    hr = device_->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(frameContexts_[i].allocator.GetAddressOf()));
    if (FAILED(hr)) {
      IGL_LOG_ERROR("D3D12Context: Failed to create command allocator for frame %u (HRESULT: 0x%08X)\n", i, static_cast<unsigned>(hr));
      IGL_DEBUG_ASSERT(false);
      return Result(Result::Code::RuntimeError, "Failed to create command allocator for frame " + std::to_string(i));
    }
    IGL_LOG_INFO("D3D12Context: Created command allocator for frame %u\n", i);
  }
  IGL_LOG_INFO("D3D12Context: Per-frame command allocators created successfully\n");

  IGL_LOG_INFO("D3D12Context: Initialization complete!\n");

  return Result();
}

Result D3D12Context::resize(uint32_t width, uint32_t height) {
  // Validate dimensions
  if (width == 0 || height == 0) {
    return Result{Result::Code::ArgumentInvalid,
                 "Invalid resize dimensions: width and height must be non-zero"};
  }

  if (width == width_ && height == height_) {
    return Result();
  }

  IGL_LOG_INFO("D3D12Context: Resizing swapchain from %ux%u to %ux%u\n",
               width_, height_, width, height);

  width_ = width;
  height_ = height;

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

  // Store swapchain format and flags for potential recreation
  DXGI_SWAP_CHAIN_DESC1 currentDesc = {};
  if (swapChain_.Get()) {
    swapChain_->GetDesc1(&currentDesc);
  }

  // Try to resize existing swapchain
  HRESULT hr = swapChain_->ResizeBuffers(
      kMaxFramesInFlight,
      width,
      height,
      currentDesc.Format ? currentDesc.Format : DXGI_FORMAT_B8G8R8A8_UNORM,
      currentDesc.Flags);

  if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12Context: ResizeBuffers failed (HRESULT=0x%08X), attempting to recreate swapchain\n",
                  static_cast<unsigned>(hr));

    // Graceful fallback: Recreate swapchain from scratch
    Result result = recreateSwapChain(width, height);
    if (!result.isOk()) {
      IGL_LOG_ERROR("D3D12Context: Failed to recreate swapchain: %s\n", result.message.c_str());
      return Result{Result::Code::RuntimeError,
                   "Failed to resize or recreate swapchain"};
    }

    IGL_LOG_INFO("D3D12Context: Swapchain recreated successfully\n");
  } else {
    IGL_LOG_INFO("D3D12Context: ResizeBuffers succeeded\n");
  }

  // Recreate back buffer views
  Result backBufferResult = createBackBuffers();
  if (!backBufferResult.isOk()) {
    IGL_LOG_ERROR("D3D12Context: Failed to recreate back buffers: %s\n", backBufferResult.message.c_str());
    return backBufferResult;
  }
  IGL_LOG_INFO("D3D12Context: Swapchain resize complete\n");

  return Result();
}

Result D3D12Context::recreateSwapChain(uint32_t width, uint32_t height) {
  IGL_LOG_INFO("D3D12Context: Recreating swapchain with dimensions %ux%u\n", width, height);

  // Get window handle from existing swapchain before releasing it
  DXGI_SWAP_CHAIN_DESC1 oldDesc = {};
  if (!swapChain_.Get()) {
    return Result{Result::Code::RuntimeError, "No existing swapchain to recreate"};
  }

  HRESULT hr = swapChain_->GetDesc1(&oldDesc);
  if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12Context: Failed to get swapchain description (HRESULT=0x%08X)\n",
                  static_cast<unsigned>(hr));
    return Result{Result::Code::RuntimeError, "Failed to get swapchain description"};
  }

  // Try to get HWND via GetHwnd (IDXGISwapChain3)
  HWND hwnd = nullptr;
  hr = swapChain_->GetHwnd(&hwnd);
  if (FAILED(hr) || !hwnd) {
    IGL_LOG_ERROR("D3D12Context: Failed to get HWND from swapchain (HRESULT=0x%08X)\n",
                  static_cast<unsigned>(hr));
    return Result{Result::Code::RuntimeError, "Failed to get HWND from swapchain"};
  }

  IGL_LOG_INFO("D3D12Context: Retrieved HWND=%p from existing swapchain\n", hwnd);

  // Release old swapchain completely
  swapChain_.Reset();
  IGL_LOG_INFO("D3D12Context: Old swapchain released\n");

  // Create new swapchain with updated dimensions
  DXGI_SWAP_CHAIN_DESC1 newDesc = {};
  newDesc.Width = width;
  newDesc.Height = height;
  newDesc.Format = oldDesc.Format ? oldDesc.Format : DXGI_FORMAT_B8G8R8A8_UNORM;
  newDesc.Stereo = FALSE;
  newDesc.SampleDesc.Count = 1;
  newDesc.SampleDesc.Quality = 0;
  newDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  newDesc.BufferCount = kMaxFramesInFlight;
  newDesc.Scaling = DXGI_SCALING_STRETCH;
  newDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  newDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
  newDesc.Flags = oldDesc.Flags;  // Preserve tearing support flag

  IGL_LOG_INFO("D3D12Context: Creating new swapchain (format=%u, flags=0x%X)\n",
               newDesc.Format, newDesc.Flags);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
  hr = dxgiFactory_->CreateSwapChainForHwnd(
      commandQueue_.Get(),
      hwnd,
      &newDesc,
      nullptr,
      nullptr,
      swapChain1.GetAddressOf());

  if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12Context: CreateSwapChainForHwnd failed (HRESULT=0x%08X)\n",
                  static_cast<unsigned>(hr));
    return Result{Result::Code::RuntimeError,
                 "Failed to recreate swapchain with CreateSwapChainForHwnd"};
  }

  // Query IDXGISwapChain3 interface
  hr = swapChain1->QueryInterface(IID_PPV_ARGS(swapChain_.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12Context: Failed to query IDXGISwapChain3 (HRESULT=0x%08X)\n",
                  static_cast<unsigned>(hr));
    return Result{Result::Code::RuntimeError,
                 "Failed to query IDXGISwapChain3 interface"};
  }

  IGL_LOG_INFO("D3D12Context: Swapchain recreated successfully\n");
  return Result{};
}

Result D3D12Context::createDevice() {
  // DO NOT enable experimental features in windowed mode - it breaks swapchain creation!
  // Experimental features are ONLY enabled in HeadlessD3D12Context for unit tests
  // Windowed render sessions use signed DXIL (via IDxcValidator) which doesn't need experimental mode

  // A-007: Read debug configuration from environment variables
  // Helper function to read boolean env var (returns defaultValue if not set)
  auto getEnvBool = [](const char* name, bool defaultValue) -> bool {
    const char* value = std::getenv(name);
    if (!value) return defaultValue;
    return (std::string(value) == "1") || (std::string(value) == "true");
  };

  // A-007: Debug configuration from environment variables
  bool enableDebugLayer = getEnvBool("IGL_D3D12_DEBUG",
#ifdef _DEBUG
    true  // Default ON in debug builds
#else
    false // Default OFF in release builds
#endif
  );
  bool enableGPUValidation = getEnvBool("IGL_D3D12_GPU_VALIDATION", false);
  bool enableDRED = getEnvBool("IGL_D3D12_DRED",
#ifdef _DEBUG
    true  // Default ON in debug builds
#else
    false // Default OFF in release builds
#endif
  );
  bool enableDXGIDebug = getEnvBool("IGL_DXGI_DEBUG",
#ifdef _DEBUG
    true  // Default ON in debug builds
#else
    false // Default OFF in release builds
#endif
  );
  bool breakOnError = getEnvBool("IGL_D3D12_BREAK_ON_ERROR", false);  // OFF by default to avoid hangs
  bool breakOnWarning = getEnvBool("IGL_D3D12_BREAK_ON_WARNING", false);

  IGL_LOG_INFO("=== D3D12 Debug Configuration ===\n");
  IGL_LOG_INFO("  Debug Layer:       %s\n", enableDebugLayer ? "ENABLED" : "DISABLED");
  IGL_LOG_INFO("  GPU Validation:    %s\n", enableGPUValidation ? "ENABLED" : "DISABLED");
  IGL_LOG_INFO("  DRED:              %s\n", enableDRED ? "ENABLED" : "DISABLED");
  IGL_LOG_INFO("  DXGI Debug:        %s\n", enableDXGIDebug ? "ENABLED" : "DISABLED");
  IGL_LOG_INFO("  Break on Error:    %s\n", breakOnError ? "ENABLED" : "DISABLED");
  IGL_LOG_INFO("  Break on Warning:  %s\n", breakOnWarning ? "ENABLED" : "DISABLED");
  IGL_LOG_INFO("=================================\n");

  // Initialize DXGI factory flags
  UINT dxgiFactoryFlags = 0;

  // A-007: Enable debug layer if configured
  if (enableDebugLayer) {
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())))) {
      debugController->EnableDebugLayer();
      IGL_LOG_INFO("D3D12Context: Debug layer ENABLED\n");

      // Enable DXGI debug layer if configured
      if (enableDXGIDebug) {
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        IGL_LOG_INFO("D3D12Context: DXGI debug layer ENABLED\n");
      }

      // A-007: Enable GPU-Based Validation if configured
      // WARNING: This significantly impacts performance (10-100x slower)
      if (enableGPUValidation) {
        Microsoft::WRL::ComPtr<ID3D12Debug1> debugController1;
        if (SUCCEEDED(debugController->QueryInterface(IID_PPV_ARGS(debugController1.GetAddressOf())))) {
          debugController1->SetEnableGPUBasedValidation(TRUE);
          IGL_LOG_INFO("D3D12Context: GPU-Based Validation ENABLED (may slow down rendering 10-100x)\n");
        } else {
          IGL_LOG_ERROR("D3D12Context: Failed to enable GPU-Based Validation (requires ID3D12Debug1)\n");
        }
      }
    } else {
      IGL_LOG_ERROR("D3D12Context: Failed to get D3D12 debug interface - Graphics Tools may not be installed\n");
    }
  } else {
    IGL_LOG_INFO("D3D12Context: Debug layer DISABLED\n");
  }

  // A-007: Enable DRED if configured (Device Removed Extended Data for better crash diagnostics)
  if (enableDRED) {
    Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> dredSettings1;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(dredSettings1.GetAddressOf())))) {
      dredSettings1->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
      dredSettings1->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
      dredSettings1->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
      IGL_LOG_INFO("D3D12Context: DRED 1.2 fully configured (breadcrumbs + page faults + context)\n");
    } else {
      IGL_LOG_ERROR("D3D12Context: Failed to configure DRED (requires Windows 10 19041+)\n");
    }
  }

  // Create DXGI factory with debug flag in debug builds (C-009)
  HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(dxgiFactory_.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12Context: Failed to create DXGI factory (HRESULT: 0x%08X)\n", static_cast<unsigned>(hr));
    IGL_DEBUG_ASSERT(false);
    return Result(Result::Code::RuntimeError, "Failed to create DXGI factory");
  }

  // A-011: Enumerate and select best adapter
  Result enumResult = enumerateAndSelectAdapter();
  if (!enumResult.isOk()) {
    return enumResult;
  }

  // A-012: Detect memory budget
  detectMemoryBudget();

  // Create D3D12 device on selected adapter
  hr = D3D12CreateDevice(
      adapter_.Get(),
      selectedFeatureLevel_,
      IID_PPV_ARGS(device_.GetAddressOf()));

  if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12CreateDevice failed on selected adapter: 0x%08X\n", static_cast<unsigned>(hr));
    IGL_DEBUG_ASSERT(false);
    return Result(Result::Code::RuntimeError, "Failed to create D3D12 device on selected adapter");
  }

  IGL_LOG_INFO("D3D12Context: Device created with Feature Level %s\n",
               featureLevelToString(selectedFeatureLevel_));

  // A-007: Setup info queue with configurable break-on-severity settings
  if (enableDebugLayer) {
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf())))) {
      // A-007: Configure break-on-severity based on environment variables
      infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);  // Always break on corruption
      infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, breakOnError ? TRUE : FALSE);
      infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, breakOnWarning ? TRUE : FALSE);

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

      IGL_LOG_INFO("D3D12Context: Info queue configured (Corruption=BREAK, Error=%s, Warning=%s)\n",
                   breakOnError ? "BREAK" : "LOG", breakOnWarning ? "BREAK" : "LOG");
    }
  }

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

  // Query shader model support with progressive fallback (A-005)
  // This is critical for FL11 hardware which only supports SM 5.1, not SM 6.0+
  IGL_LOG_INFO("D3D12Context: Querying shader model capabilities for Feature Level %d.%d...\n",
               (selectedFeatureLevel_ >> 12) & 0xF, (selectedFeatureLevel_ >> 8) & 0xF);

  // Helper to map feature level to expected minimum shader model
  auto getMinShaderModelForFeatureLevel = [](D3D_FEATURE_LEVEL fl) -> D3D_SHADER_MODEL {
    switch (fl) {
      case D3D_FEATURE_LEVEL_12_2:
        return D3D_SHADER_MODEL_6_6;  // FL 12.2 supports SM 6.6+
      case D3D_FEATURE_LEVEL_12_1:
        return D3D_SHADER_MODEL_6_1;  // FL 12.1 supports SM 6.1 (mesh shaders)
      case D3D_FEATURE_LEVEL_12_0:
        return D3D_SHADER_MODEL_6_0;  // FL 12.0 supports SM 6.0 (wave operations)
      case D3D_FEATURE_LEVEL_11_1:
      case D3D_FEATURE_LEVEL_11_0:
        return D3D_SHADER_MODEL_5_1;  // FL 11.x only supports SM 5.1
      default:
        return D3D_SHADER_MODEL_5_1;  // Conservative fallback
    }
  };

  auto shaderModelToString = [](D3D_SHADER_MODEL sm) -> const char* {
    switch (sm) {
      case D3D_SHADER_MODEL_6_6: return "6.6";
      case D3D_SHADER_MODEL_6_5: return "6.5";
      case D3D_SHADER_MODEL_6_4: return "6.4";
      case D3D_SHADER_MODEL_6_3: return "6.3";
      case D3D_SHADER_MODEL_6_2: return "6.2";
      case D3D_SHADER_MODEL_6_1: return "6.1";
      case D3D_SHADER_MODEL_6_0: return "6.0";
      case D3D_SHADER_MODEL_5_1: return "5.1";
      default: return "Unknown";
    }
  };

  // Shader models to attempt, from highest to lowest
  const D3D_SHADER_MODEL shaderModels[] = {
      D3D_SHADER_MODEL_6_6,
      D3D_SHADER_MODEL_6_5,
      D3D_SHADER_MODEL_6_4,
      D3D_SHADER_MODEL_6_3,
      D3D_SHADER_MODEL_6_2,
      D3D_SHADER_MODEL_6_1,
      D3D_SHADER_MODEL_6_0,
      D3D_SHADER_MODEL_5_1,
  };

  D3D_SHADER_MODEL detectedShaderModel = D3D_SHADER_MODEL_5_1;
  bool shaderModelDetected = false;

  // Try each shader model from highest to lowest
  for (D3D_SHADER_MODEL sm : shaderModels) {
    D3D12_FEATURE_DATA_SHADER_MODEL shaderModelData = { sm };
    hr = device_->CheckFeatureSupport(
        D3D12_FEATURE_SHADER_MODEL,
        &shaderModelData,
        sizeof(shaderModelData));

    if (SUCCEEDED(hr)) {
      detectedShaderModel = shaderModelData.HighestShaderModel;
      shaderModelDetected = true;
      IGL_LOG_INFO("  Detected Shader Model: %s\n", shaderModelToString(detectedShaderModel));
      break;  // Found highest supported, stop trying
    } else {
      IGL_LOG_INFO("  Shader Model %s not supported, trying lower version\n",
                   shaderModelToString(sm));
    }
  }

  if (!shaderModelDetected) {
    // Fallback based on feature level
    D3D_SHADER_MODEL minimumSM = getMinShaderModelForFeatureLevel(selectedFeatureLevel_);
    IGL_LOG_INFO("  WARNING: Shader model detection failed, using minimum for Feature Level: %s\n",
                    shaderModelToString(minimumSM));
    detectedShaderModel = minimumSM;
  }

  // Validate shader model is appropriate for feature level
  D3D_SHADER_MODEL minimumRequired = getMinShaderModelForFeatureLevel(selectedFeatureLevel_);
  if (detectedShaderModel < minimumRequired) {
    IGL_LOG_INFO("  WARNING: Detected Shader Model %s is below minimum for Feature Level: %s\n",
                    shaderModelToString(detectedShaderModel),
                    shaderModelToString(minimumRequired));
  }

  maxShaderModel_ = detectedShaderModel;
  IGL_LOG_INFO("D3D12Context: Final Shader Model selected: %s\n", shaderModelToString(maxShaderModel_));

  IGL_LOG_INFO("D3D12Context: Root signature capabilities detected successfully\n");

  return Result();
}

// A-011: Enumerate and select best adapter
Result D3D12Context::enumerateAndSelectAdapter() {
  enumeratedAdapters_.clear();

  IGL_LOG_INFO("D3D12Context: Enumerating DXGI adapters...\n");

  // Try IDXGIFactory6 first for high-performance GPU preference
  Microsoft::WRL::ComPtr<IDXGIFactory6> factory6;
  (void)dxgiFactory_->QueryInterface(IID_PPV_ARGS(factory6.GetAddressOf()));

  if (factory6.Get()) {
    for (UINT i = 0; ; ++i) {
      Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
      if (FAILED(factory6->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                      IID_PPV_ARGS(adapter.GetAddressOf())))) {
        break;
      }

      AdapterInfo info{};
      info.adapter = adapter;
      info.index = i;
      info.isWarp = false;

      adapter->GetDesc1(&info.desc);

      // Skip software adapters in main enumeration (we'll add WARP separately)
      if (info.desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        continue;
      }

      // Determine feature level
      info.featureLevel = getHighestFeatureLevel(adapter.Get());
      if (info.featureLevel == static_cast<D3D_FEATURE_LEVEL>(0)) {
        IGL_LOG_INFO("D3D12Context: Adapter %u does not support D3D12 (skipping)\n", i);
        continue;
      }

      enumeratedAdapters_.push_back(info);

      // Log adapter details
      IGL_LOG_INFO("D3D12Context: Adapter %u:\n", i);
      IGL_LOG_INFO("  Description: %ls\n", info.desc.Description);
      IGL_LOG_INFO("  Vendor ID: 0x%04X (%s)\n", info.desc.VendorId, info.getVendorName());
      IGL_LOG_INFO("  Device ID: 0x%04X\n", info.desc.DeviceId);
      IGL_LOG_INFO("  Dedicated VRAM: %llu MB\n", info.getDedicatedVideoMemoryMB());
      IGL_LOG_INFO("  Shared System Memory: %llu MB\n", info.desc.SharedSystemMemory / (1024 * 1024));
      IGL_LOG_INFO("  Feature Level: %s\n", featureLevelToString(info.featureLevel));
      IGL_LOG_INFO("  LUID: 0x%08X:0x%08X\n", info.desc.AdapterLuid.HighPart, info.desc.AdapterLuid.LowPart);
    }
  }

  // Fallback enumeration if Factory6 not available
  if (enumeratedAdapters_.empty()) {
    for (UINT i = 0; ; ++i) {
      Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
      if (dxgiFactory_->EnumAdapters1(i, adapter.GetAddressOf()) == DXGI_ERROR_NOT_FOUND) {
        break;
      }

      AdapterInfo info{};
      info.adapter = adapter;
      info.index = i;
      info.isWarp = false;

      adapter->GetDesc1(&info.desc);

      // Skip software adapters
      if (info.desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        continue;
      }

      // Determine feature level
      info.featureLevel = getHighestFeatureLevel(adapter.Get());
      if (info.featureLevel == static_cast<D3D_FEATURE_LEVEL>(0)) {
        continue;
      }

      enumeratedAdapters_.push_back(info);

      // Log adapter details
      IGL_LOG_INFO("D3D12Context: Adapter %u:\n", i);
      IGL_LOG_INFO("  Description: %ls\n", info.desc.Description);
      IGL_LOG_INFO("  Vendor ID: 0x%04X (%s)\n", info.desc.VendorId, info.getVendorName());
      IGL_LOG_INFO("  Device ID: 0x%04X\n", info.desc.DeviceId);
      IGL_LOG_INFO("  Dedicated VRAM: %llu MB\n", info.getDedicatedVideoMemoryMB());
      IGL_LOG_INFO("  Shared System Memory: %llu MB\n", info.desc.SharedSystemMemory / (1024 * 1024));
      IGL_LOG_INFO("  Feature Level: %s\n", featureLevelToString(info.featureLevel));
    }
  }

  // Add WARP adapter as fallback option (software rasterizer)
  Microsoft::WRL::ComPtr<IDXGIAdapter> warpAdapter;
  if (SUCCEEDED(dxgiFactory_->EnumWarpAdapter(IID_PPV_ARGS(warpAdapter.GetAddressOf())))) {
    Microsoft::WRL::ComPtr<IDXGIAdapter1> warpAdapter1;
    if (SUCCEEDED(warpAdapter->QueryInterface(IID_PPV_ARGS(warpAdapter1.GetAddressOf())))) {
      AdapterInfo warpInfo{};
      warpInfo.adapter = warpAdapter1;
      warpInfo.index = static_cast<uint32_t>(enumeratedAdapters_.size());
      warpInfo.isWarp = true;

      warpAdapter1->GetDesc1(&warpInfo.desc);
      warpInfo.featureLevel = getHighestFeatureLevel(warpAdapter1.Get());

      enumeratedAdapters_.push_back(warpInfo);

      IGL_LOG_INFO("D3D12Context: WARP Adapter (Software):\n");
      IGL_LOG_INFO("  Description: %ls\n", warpInfo.desc.Description);
      IGL_LOG_INFO("  Feature Level: %s\n", featureLevelToString(warpInfo.featureLevel));
    }
  }

  if (enumeratedAdapters_.empty()) {
    IGL_LOG_ERROR("D3D12Context: No compatible D3D12 adapters found!\n");
    IGL_DEBUG_ASSERT(false);
    return Result(Result::Code::RuntimeError, "No D3D12-compatible adapters available");
  }

  // Select adapter based on environment variable or heuristic
  selectedAdapterIndex_ = 0;  // Default to first adapter (discrete GPU on laptops)

  char adapterEnv[64] = {};
  DWORD envResult = GetEnvironmentVariableA("IGL_D3D12_ADAPTER", adapterEnv, sizeof(adapterEnv));
  if (envResult > 0 && envResult < sizeof(adapterEnv)) {
    if (strcmp(adapterEnv, "WARP") == 0) {
      // Find WARP adapter
      for (size_t i = 0; i < enumeratedAdapters_.size(); ++i) {
        if (enumeratedAdapters_[i].isWarp) {
          selectedAdapterIndex_ = static_cast<uint32_t>(i);
          IGL_LOG_INFO("D3D12Context: Environment override - using WARP adapter\n");
          break;
        }
      }
    } else {
      // Parse adapter index
      int requestedIndex = atoi(adapterEnv);
      if (requestedIndex >= 0 && requestedIndex < static_cast<int>(enumeratedAdapters_.size())) {
        selectedAdapterIndex_ = static_cast<uint32_t>(requestedIndex);
        IGL_LOG_INFO("D3D12Context: Environment override - using adapter %d\n", requestedIndex);
      } else {
        IGL_LOG_ERROR("D3D12Context: Invalid adapter index %d (available: 0-%zu)\n",
                      requestedIndex, enumeratedAdapters_.size() - 1);
      }
    }
  } else {
    // Heuristic: Choose adapter with highest feature level and most VRAM
    D3D_FEATURE_LEVEL highestFL = enumeratedAdapters_[0].featureLevel;
    uint64_t largestVRAM = enumeratedAdapters_[0].getDedicatedVideoMemoryMB();

    for (size_t i = 1; i < enumeratedAdapters_.size(); ++i) {
      if (enumeratedAdapters_[i].isWarp) {
        continue;  // Skip WARP for automatic selection
      }

      uint64_t vram = enumeratedAdapters_[i].getDedicatedVideoMemoryMB();
      D3D_FEATURE_LEVEL fl = enumeratedAdapters_[i].featureLevel;

      // Prefer higher feature level, or same feature level with more VRAM
      if (fl > highestFL || (fl == highestFL && vram > largestVRAM)) {
        selectedAdapterIndex_ = static_cast<uint32_t>(i);
        highestFL = fl;
        largestVRAM = vram;
      }
    }
  }

  adapter_ = enumeratedAdapters_[selectedAdapterIndex_].adapter;
  selectedFeatureLevel_ = enumeratedAdapters_[selectedAdapterIndex_].featureLevel;

  IGL_LOG_INFO("D3D12Context: Selected adapter %u: %ls (FL %s)\n",
               selectedAdapterIndex_,
               enumeratedAdapters_[selectedAdapterIndex_].desc.Description,
               featureLevelToString(selectedFeatureLevel_));

  return Result();
}

// A-012: Detect memory budget from selected adapter
void D3D12Context::detectMemoryBudget() {
  if (selectedAdapterIndex_ >= enumeratedAdapters_.size()) {
    IGL_LOG_ERROR("D3D12Context: No adapter selected for memory budget detection\n");
    return;
  }

  const auto& selectedAdapter = enumeratedAdapters_[selectedAdapterIndex_];

  memoryBudget_.dedicatedVideoMemory = selectedAdapter.desc.DedicatedVideoMemory;
  memoryBudget_.sharedSystemMemory = selectedAdapter.desc.SharedSystemMemory;

  IGL_LOG_INFO("D3D12Context: GPU Memory Budget:\n");
  IGL_LOG_INFO("  Dedicated Video Memory: %.2f MB\n",
               memoryBudget_.dedicatedVideoMemory / (1024.0 * 1024.0));
  IGL_LOG_INFO("  Shared System Memory: %.2f MB\n",
               memoryBudget_.sharedSystemMemory / (1024.0 * 1024.0));
  IGL_LOG_INFO("  Total Available: %.2f MB\n",
               memoryBudget_.totalAvailableMemory() / (1024.0 * 1024.0));

  // Recommend conservative budget (80% of available)
  uint64_t recommendedBudget = static_cast<uint64_t>(memoryBudget_.totalAvailableMemory() * 0.8);
  IGL_LOG_INFO("  Recommended Budget (80%%): %.2f MB\n",
               recommendedBudget / (1024.0 * 1024.0));
}

// A-010: Detect HDR output capabilities
void D3D12Context::detectHDRCapabilities() {
  IGL_LOG_INFO("D3D12Context: Detecting HDR output capabilities...\n");

  // Reset to defaults
  hdrCapabilities_ = HDRCapabilities{};

  // Need a valid swapchain to query output
  if (!swapChain_.Get()) {
    IGL_LOG_INFO("  No swapchain available, HDR detection skipped\n");
    return;
  }

  // Get the output (monitor) containing the swapchain
  Microsoft::WRL::ComPtr<IDXGIOutput> output;
  HRESULT hr = swapChain_->GetContainingOutput(output.GetAddressOf());
  if (FAILED(hr)) {
    IGL_LOG_INFO("  Failed to get containing output (0x%08X), HDR not available\n", static_cast<unsigned>(hr));
    return;
  }

  // Query for IDXGIOutput6 (required for HDR queries)
  Microsoft::WRL::ComPtr<IDXGIOutput6> output6;
  hr = output->QueryInterface(IID_PPV_ARGS(output6.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_INFO("  IDXGIOutput6 not available (needs Windows 10 1703+), HDR not supported\n");
    return;
  }

  // Get output description with color space info
  DXGI_OUTPUT_DESC1 outputDesc = {};
  hr = output6->GetDesc1(&outputDesc);
  if (FAILED(hr)) {
    IGL_LOG_INFO("  Failed to get output description (0x%08X)\n", static_cast<unsigned>(hr));
    return;
  }

  // Store native color space
  hdrCapabilities_.nativeColorSpace = outputDesc.ColorSpace;

  // Store luminance information
  hdrCapabilities_.maxLuminance = outputDesc.MaxLuminance;
  hdrCapabilities_.minLuminance = outputDesc.MinLuminance;
  hdrCapabilities_.maxFullFrameLuminance = outputDesc.MaxFullFrameLuminance;

  IGL_LOG_INFO("  Native Color Space: %u\n", outputDesc.ColorSpace);
  IGL_LOG_INFO("  Max Luminance: %.2f nits\n", outputDesc.MaxLuminance);
  IGL_LOG_INFO("  Min Luminance: %.4f nits\n", outputDesc.MinLuminance);
  IGL_LOG_INFO("  Max Full Frame Luminance: %.2f nits\n", outputDesc.MaxFullFrameLuminance);

  // Check for HDR10 support (BT.2020 ST2084 - PQ curve) via swapchain
  UINT colorSpaceSupport = 0;
  hr = swapChain_->CheckColorSpaceSupport(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, &colorSpaceSupport);
  if (SUCCEEDED(hr) && (colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)) {
    hdrCapabilities_.hdrSupported = true;
    IGL_LOG_INFO("  HDR10 (BT.2020 PQ): SUPPORTED\n");
  } else {
    IGL_LOG_INFO("  HDR10 (BT.2020 PQ): NOT SUPPORTED\n");
  }

  // Check for scRGB support (linear floating-point HDR)
  hr = swapChain_->CheckColorSpaceSupport(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709, &colorSpaceSupport);
  if (SUCCEEDED(hr) && (colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)) {
    hdrCapabilities_.scRGBSupported = true;
    IGL_LOG_INFO("  scRGB (Linear FP16): SUPPORTED\n");
  } else {
    IGL_LOG_INFO("  scRGB (Linear FP16): NOT SUPPORTED\n");
  }

  // Summary
  if (hdrCapabilities_.hdrSupported || hdrCapabilities_.scRGBSupported) {
    IGL_LOG_INFO("D3D12Context: HDR output AVAILABLE (max %.0f nits)\n", outputDesc.MaxLuminance);
  } else {
    IGL_LOG_INFO("D3D12Context: HDR output NOT AVAILABLE (SDR display)\n");
  }
}

Result D3D12Context::createCommandQueue() {
  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

  HRESULT hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(commandQueue_.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12Context: Failed to create command queue (HRESULT: 0x%08X)\n", static_cast<unsigned>(hr));
    IGL_DEBUG_ASSERT(false);
    return Result(Result::Code::RuntimeError, "Failed to create command queue");
  }

  return Result();
}

Result D3D12Context::createSwapChain(HWND hwnd, uint32_t width, uint32_t height) {
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
      IGL_LOG_ERROR("D3D12Context: Failed to create swapchain (hr=0x%08X / 0x%08X)\n", (unsigned)hr, (unsigned)hr2);
      IGL_DEBUG_ASSERT(false);
      return Result(Result::Code::RuntimeError, "Failed to create swapchain");
    }
    // Try to QI to IDXGISwapChain3
    hr2 = legacySwap->QueryInterface(IID_PPV_ARGS(swapChain_.GetAddressOf()));
    if (FAILED(hr2)) {
      IGL_LOG_ERROR("D3D12Context: Failed to query IDXGISwapChain3 (hr=0x%08X)\n", (unsigned)hr2);
      IGL_DEBUG_ASSERT(false);
      return Result(Result::Code::RuntimeError, "Failed to query IDXGISwapChain3");
    }
    return Result();
  }

  // Cast to IDXGISwapChain3
  hr = tempSwapChain->QueryInterface(IID_PPV_ARGS(swapChain_.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12Context: Failed to query IDXGISwapChain3 interface (HRESULT: 0x%08X)\n", static_cast<unsigned>(hr));
    IGL_DEBUG_ASSERT(false);
    return Result(Result::Code::RuntimeError, "Failed to query IDXGISwapChain3 interface");
  }

  // A-009: Verify swapchain actually supports tearing after creation
  if (tearingSupported_) {
    DXGI_SWAP_CHAIN_DESC1 actualDesc = {};
    hr = swapChain_->GetDesc1(&actualDesc);
    if (SUCCEEDED(hr)) {
      const bool actualTearingFlag = (actualDesc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) != 0;
      const bool actualWindowedMode = (actualDesc.SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD ||
                                        actualDesc.SwapEffect == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL);

      if (!actualTearingFlag) {
        IGL_LOG_INFO("D3D12Context: Tearing flag was NOT set on swapchain (downgraded by driver)\n");
        tearingSupported_ = false;
      } else if (!actualWindowedMode) {
        IGL_LOG_INFO("D3D12Context: Swapchain not in flip mode (tearing requires flip model)\n");
        tearingSupported_ = false;
      } else {
        IGL_LOG_INFO("D3D12Context: Tearing verified on swapchain (windowed flip model + tearing flag)\n");
      }
    } else {
      IGL_LOG_INFO("D3D12Context: Failed to verify swapchain desc, assuming tearing unavailable\n");
      tearingSupported_ = false;
    }
  }

  // A-010: Detect HDR capabilities now that swapchain is created
  detectHDRCapabilities();

  return Result();
}

Result D3D12Context::createRTVHeap() {
  D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
  heapDesc.NumDescriptors = kMaxFramesInFlight;
  heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

  HRESULT hr = device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(rtvHeap_.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12Context: Failed to create RTV heap (HRESULT: 0x%08X)\n", static_cast<unsigned>(hr));
    IGL_DEBUG_ASSERT(false);
    return Result(Result::Code::RuntimeError, "Failed to create RTV heap");
  }

  rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  return Result();
}

Result D3D12Context::createBackBuffers() {
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();

  for (UINT i = 0; i < kMaxFramesInFlight; i++) {
    HRESULT hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(renderTargets_[i].GetAddressOf()));
    if (FAILED(hr)) {
      IGL_LOG_ERROR("D3D12Context: Failed to get swapchain buffer %u (HRESULT: 0x%08X)\n", i, static_cast<unsigned>(hr));
      IGL_DEBUG_ASSERT(false);
      return Result(Result::Code::RuntimeError, "Failed to get swapchain buffer");
    }

    // Pre-creation validation (TASK_P0_DX12-004)
    IGL_DEBUG_ASSERT(device_.Get() != nullptr, "Device is null before CreateRenderTargetView");
    IGL_DEBUG_ASSERT(renderTargets_[i].Get() != nullptr, "Swapchain buffer is null");
    IGL_DEBUG_ASSERT(rtvHandle.ptr != 0, "RTV descriptor handle is invalid");

    device_->CreateRenderTargetView(renderTargets_[i].Get(), nullptr, rtvHandle);
    rtvHandle.ptr += rtvDescriptorSize_;
  }

  return Result();
}

Result D3D12Context::createDescriptorHeaps() {
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
        IGL_LOG_ERROR("D3D12Context: Failed to create initial CBV/SRV/UAV heap page for frame %u\n", i);
        IGL_DEBUG_ASSERT(false);
        return result;
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
        IGL_LOG_ERROR("D3D12Context: Failed to create per-frame Sampler heap for frame %u (HRESULT: 0x%08X)\n", i, static_cast<unsigned>(hr));
        IGL_DEBUG_ASSERT(false);
        return Result(Result::Code::RuntimeError, "Failed to create per-frame Sampler heap for frame " + std::to_string(i));
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

  return Result();
}

Result D3D12Context::createCommandSignatures() {
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
      IGL_LOG_ERROR("D3D12Context: Failed to create draw indirect command signature (HRESULT: 0x%08X)\n", static_cast<unsigned>(hr));
      IGL_DEBUG_ASSERT(false);
      return Result(Result::Code::RuntimeError, "Failed to create draw indirect command signature");
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
      IGL_LOG_ERROR("D3D12Context: Failed to create draw indexed indirect command signature (HRESULT: 0x%08X)\n", static_cast<unsigned>(hr));
      IGL_DEBUG_ASSERT(false);
      return Result(Result::Code::RuntimeError, "Failed to create draw indexed indirect command signature");
    }
    IGL_LOG_INFO("D3D12Context: Created draw indexed indirect command signature (stride: %u bytes)\n",
                 drawIndexedSigDesc.ByteStride);
  }

  return Result();
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
