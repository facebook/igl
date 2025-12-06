/*
 * Minimal headless D3D12 context for unit tests (no swapchain / no HWND).
 */

#include <igl/d3d12/HeadlessContext.h>
#include <igl/d3d12/DescriptorHeapManager.h>
#include <string>

namespace igl::d3d12 {

namespace {
// Import ComPtr for readability
template<typename T>
using ComPtr = igl::d3d12::ComPtr<T>;
} // namespace

HeadlessD3D12Context::~HeadlessD3D12Context() = default;

Result HeadlessD3D12Context::initializeHeadless(uint32_t width, uint32_t height,
                                                const D3D12ContextConfig& config) {
  width_ = width;
  height_ = height;

  // Store and validate configuration.
  config_ = config;
  config_.validate();

  // Headless mode: No swapchain, so use kMaxFramesInFlight as buffer count (T43)
  swapchainBufferCount_ = kMaxFramesInFlight;
  renderTargets_.resize(swapchainBufferCount_);
  frameContexts_.resize(swapchainBufferCount_);
  IGL_D3D12_LOG_VERBOSE("HeadlessD3D12Context: Initialized with %u frame buffers (no swapchain)\n",
                        swapchainBufferCount_);

  // Initialize DXGI factory flags and debug configuration (mirrors windowed D3D12Context).
  auto getEnvBool = [](const char* name, bool defaultValue) -> bool {
    const char* value = std::getenv(name);
    if (!value) {
      return defaultValue;
    }
    return (std::string(value) == "1") || (std::string(value) == "true");
  };

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

  IGL_D3D12_LOG_VERBOSE("=== Headless D3D12 Debug Configuration ===\n");
  IGL_D3D12_LOG_VERBOSE("  Debug Layer:       %s\n", enableDebugLayer ? "ENABLED" : "DISABLED");
  IGL_D3D12_LOG_VERBOSE("  GPU Validation:    %s\n", enableGPUValidation ? "ENABLED" : "DISABLED");
  IGL_D3D12_LOG_VERBOSE("  DRED:              %s\n", enableDRED ? "ENABLED" : "DISABLED");
  IGL_D3D12_LOG_VERBOSE("  DXGI Debug:        %s\n", enableDXGIDebug ? "ENABLED" : "DISABLED");
  IGL_D3D12_LOG_VERBOSE("=========================================\n");

  UINT dxgiFactoryFlags = 0;

  // Enable debug layer (and GPU-based validation) if configured.
  if (enableDebugLayer) {
    igl::d3d12::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())))) {
      debugController->EnableDebugLayer();
      IGL_D3D12_LOG_VERBOSE("HeadlessD3D12Context: Debug layer ENABLED\n");

      if (enableDXGIDebug) {
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        IGL_D3D12_LOG_VERBOSE("HeadlessD3D12Context: DXGI debug layer ENABLED\n");
      }

      if (enableGPUValidation) {
        igl::d3d12::ComPtr<ID3D12Debug1> debugController1;
        if (SUCCEEDED(debugController->QueryInterface(IID_PPV_ARGS(debugController1.GetAddressOf())))) {
          debugController1->SetEnableGPUBasedValidation(TRUE);
          IGL_D3D12_LOG_VERBOSE("HeadlessD3D12Context: GPU-Based Validation ENABLED\n");
        } else {
          IGL_LOG_ERROR("HeadlessD3D12Context: Failed to enable GPU-Based Validation (requires ID3D12Debug1)\n");
        }
      }
    } else {
      IGL_LOG_ERROR("HeadlessD3D12Context: Failed to get D3D12 debug interface - Graphics Tools may not be installed\n");
    }
  } else {
    IGL_D3D12_LOG_VERBOSE("HeadlessD3D12Context: Debug layer DISABLED\n");
  }

  // Enable DRED if configured (Device Removed Extended Data for better crash diagnostics).
  if (enableDRED) {
    igl::d3d12::ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> dredSettings1;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(dredSettings1.GetAddressOf())))) {
      dredSettings1->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
      dredSettings1->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
      dredSettings1->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
      IGL_D3D12_LOG_VERBOSE("HeadlessD3D12Context: DRED 1.2 fully configured\n");
    } else {
      IGL_LOG_ERROR("HeadlessD3D12Context: Failed to configure DRED (requires Windows 10 19041+)\n");
    }
  }

  // Enable experimental features for headless contexts (unit tests)
  // This allows unsigned DXIL shaders to run
  // NOTE: This is ONLY called in headless mode (unit tests), NOT in windowed render sessions
  {
    UUID experimentalFeatures[] = {D3D12ExperimentalShaderModels};
    HRESULT hr = D3D12EnableExperimentalFeatures(1, experimentalFeatures, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
      IGL_D3D12_LOG_VERBOSE("HeadlessD3D12Context: Experimental shader models enabled (allows unsigned DXIL)\n");
    } else {
      IGL_D3D12_LOG_VERBOSE("HeadlessD3D12Context: Failed to enable experimental features (0x%08X) - signed DXIL required\n", static_cast<unsigned>(hr));
    }
  }

  // Create DXGI factory with debug flag in debug builds.
  HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(dxgiFactory_.GetAddressOf()));
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create DXGI factory");
  }

  // Helper function to try creating device with progressive feature level fallback (A-004)
  auto tryCreateDeviceWithFallback =
      [](IDXGIAdapter1* adapter, D3D_FEATURE_LEVEL& outFeatureLevel) -> igl::d3d12::ComPtr<ID3D12Device> {
    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    igl::d3d12::ComPtr<ID3D12Device> device;
    for (D3D_FEATURE_LEVEL fl : featureLevels) {
      HRESULT hr = D3D12CreateDevice(adapter, fl, IID_PPV_ARGS(device.GetAddressOf()));
      if (SUCCEEDED(hr)) {
        outFeatureLevel = fl;
        IGL_D3D12_LOG_VERBOSE("HeadlessD3D12Context: Device created with Feature Level %d.%d\n",
                     (fl >> 12) & 0xF, (fl >> 8) & 0xF);
        return device;
      }
    }
    outFeatureLevel = static_cast<D3D_FEATURE_LEVEL>(0);
    return nullptr;
  };

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

  igl::d3d12::ComPtr<IDXGIFactory6> factory6;
  (void)dxgiFactory_->QueryInterface(IID_PPV_ARGS(factory6.GetAddressOf()));

  bool created = false;
  D3D_FEATURE_LEVEL selectedFeatureLevel = D3D_FEATURE_LEVEL_11_0;

  if (factory6.Get()) {
    for (UINT i = 0;; ++i) {
      igl::d3d12::ComPtr<IDXGIAdapter1> adapter;
      if (FAILED(factory6->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                      IID_PPV_ARGS(adapter.GetAddressOf())))) {
        break;
      }
      DXGI_ADAPTER_DESC1 desc{};
      adapter->GetDesc1(&desc);
      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        continue;
      }

      D3D_FEATURE_LEVEL featureLevel = static_cast<D3D_FEATURE_LEVEL>(0);
      auto device = tryCreateDeviceWithFallback(adapter.Get(), featureLevel);
      if (device.Get() != nullptr) {
        device_ = device;
        created = true;
        selectedFeatureLevel = featureLevel;
        IGL_D3D12_LOG_VERBOSE("HeadlessD3D12Context: Selected HW adapter (FL %s)\n",
                     featureLevelToString(featureLevel));
        break;
      }
    }
  }
  if (!created) {
    for (UINT i = 0;; ++i) {
      igl::d3d12::ComPtr<IDXGIAdapter1> adapter;
      if (dxgiFactory_->EnumAdapters1(i, adapter.GetAddressOf()) == DXGI_ERROR_NOT_FOUND) {
        break;
      }
      DXGI_ADAPTER_DESC1 desc{};
      adapter->GetDesc1(&desc);
      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        continue;
      }

      D3D_FEATURE_LEVEL featureLevel = static_cast<D3D_FEATURE_LEVEL>(0);
      auto device = tryCreateDeviceWithFallback(adapter.Get(), featureLevel);
      if (device.Get() != nullptr) {
        device_ = device;
        created = true;
        selectedFeatureLevel = featureLevel;
        IGL_D3D12_LOG_VERBOSE("HeadlessD3D12Context: Selected HW adapter via EnumAdapters1 (FL %s)\n",
                     featureLevelToString(featureLevel));
        break;
      }
    }
  }
  if (!created) {
    igl::d3d12::ComPtr<IDXGIAdapter> warp;
    if (SUCCEEDED(dxgiFactory_->EnumWarpAdapter(IID_PPV_ARGS(warp.GetAddressOf())))) {
      igl::d3d12::ComPtr<IDXGIAdapter1> warp1;
      warp->QueryInterface(IID_PPV_ARGS(warp1.GetAddressOf()));
      if (warp1.Get()) {
        D3D_FEATURE_LEVEL featureLevel = static_cast<D3D_FEATURE_LEVEL>(0);
        auto device = tryCreateDeviceWithFallback(warp1.Get(), featureLevel);
        if (device.Get() != nullptr) {
          device_ = device;
          created = true;
          selectedFeatureLevel = featureLevel;
          IGL_D3D12_LOG_VERBOSE("HeadlessD3D12Context: Using WARP adapter (FL %s)\n",
                       featureLevelToString(featureLevel));
        }
      }
    }
  }
  if (!created) {
    return Result(Result::Code::RuntimeError, "Failed to create any D3D12 device");
  }

  // Store selected feature level (A-004)
  selectedFeatureLevel_ = selectedFeatureLevel;

#ifdef _DEBUG
  {
    igl::d3d12::ComPtr<ID3D12InfoQueue> infoQueue;
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

  // Create per-frame descriptor heaps (consistent with windowed D3D12Context)
  // Allow override via env vars for headless tests
  UINT cbvSrvUavHeapSize = 1024; // default matching Microsoft MiniEngine
  {
    char buf[32] = {};
    const DWORD n = GetEnvironmentVariableA("IGL_D3D12_CBV_SRV_UAV_HEAP_SIZE", buf, sizeof(buf));
    if (n > 0) {
      cbvSrvUavHeapSize = std::max<UINT>(256, static_cast<UINT>(strtoul(buf, nullptr, 10)));
    }
  }

  UINT samplerHeapSize = kMaxSamplers; // Match D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE (2048)
  {
    char buf[32] = {};
    const DWORD n = GetEnvironmentVariableA("IGL_D3D12_SAMPLER_HEAP_SIZE", buf, sizeof(buf));
    if (n > 0) {
      samplerHeapSize = std::max<UINT>(16, static_cast<UINT>(strtoul(buf, nullptr, 10)));
    }
  }

  // Cache descriptor sizes
  cbvSrvUavDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  samplerDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

  IGL_D3D12_LOG_VERBOSE("HeadlessContext: Creating per-frame descriptor heaps (CBV/SRV/UAV=%u, Samplers=%u)...\n",
               cbvSrvUavHeapSize, samplerHeapSize);

  // Create per-frame shader-visible descriptor heaps and an initial page for each frame.
  for (UINT i = 0; i < swapchainBufferCount_; i++) {
    // CBV/SRV/UAV heap per frame - create initial page
    igl::d3d12::ComPtr<ID3D12DescriptorHeap> initialHeap;
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = cbvSrvUavHeapSize;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    desc.NodeMask = 0;
    hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(initialHeap.GetAddressOf()));
    if (FAILED(hr)) {
      return Result(Result::Code::RuntimeError, "Failed to create per-frame CBV/SRV/UAV heap for frame " + std::to_string(i));
    }

    // Initialize page vector with first page
    frameContexts_[i].cbvSrvUavHeapPages.clear();
    frameContexts_[i].cbvSrvUavHeapPages.emplace_back(initialHeap, cbvSrvUavHeapSize);
    frameContexts_[i].currentCbvSrvUavPageIndex = 0;

    IGL_D3D12_LOG_VERBOSE("  Frame %u: Created CBV/SRV/UAV heap page (%u descriptors)\n", i, cbvSrvUavHeapSize);

    // Sampler heap per frame
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    desc.NumDescriptors = samplerHeapSize;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    desc.NodeMask = 0;
    hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(frameContexts_[i].samplerHeap.GetAddressOf()));
    if (FAILED(hr)) {
      return Result(Result::Code::RuntimeError, "Failed to create per-frame Sampler heap for frame " + std::to_string(i));
    }
    IGL_D3D12_LOG_VERBOSE("  Frame %u: Created Sampler heap (%u descriptors)\n", i, samplerHeapSize);
  }

  IGL_D3D12_LOG_VERBOSE("HeadlessContext: Per-frame descriptor heaps created successfully\n");

  // Create per-frame command allocators (following Microsoft's D3D12HelloFrameBuffering pattern)
  IGL_D3D12_LOG_VERBOSE("HeadlessContext: Creating per-frame command allocators...\n");
  for (UINT i = 0; i < swapchainBufferCount_; i++) {
    hr = device_->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(frameContexts_[i].allocator.GetAddressOf()));
    if (FAILED(hr)) {
      return Result(Result::Code::RuntimeError, "Failed to create command allocator for frame " + std::to_string(i));
    }
    IGL_D3D12_LOG_VERBOSE("  Frame %u: Created command allocator\n", i);
  }
  IGL_D3D12_LOG_VERBOSE("HeadlessContext: Per-frame command allocators created successfully\n");

  // Fence for GPU synchronization
  hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf()));
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create fence");
  }

  // Create descriptor heap manager with the same sizes for consistency.
  {
    DescriptorHeapManager::Sizes sz{};
    sz.cbvSrvUav = cbvSrvUavHeapSize;
    sz.samplers = samplerHeapSize;
    sz.rtvs = 64;
    sz.dsvs = 32;
    descriptorHeaps_ = std::make_unique<DescriptorHeapManager>();
    const Result r = descriptorHeaps_->initialize(device_.Get(), sz);
    if (!r.isOk()) {
      IGL_LOG_ERROR("HeadlessD3D12Context: Failed to initialize descriptor heap manager: %s\n",
                    r.message.c_str());
      // Non-fatal: continue without a dedicated manager.
      descriptorHeaps_.reset();
    }
    // Expose manager to base context for consumers that only see D3D12Context
    heapMgr_ = descriptorHeaps_.get();
  }

  // Create command signatures for indirect drawing.
  IGL_D3D12_LOG_VERBOSE("HeadlessD3D12Context: Creating command signatures...\n");
  Result commandSigResult = createCommandSignatures();
  if (!commandSigResult.isOk()) {
    IGL_LOG_ERROR("HeadlessD3D12Context: Failed to create command signatures: %s\n",
                  commandSigResult.message.c_str());
    return commandSigResult;
  }
  IGL_D3D12_LOG_VERBOSE("HeadlessD3D12Context: Command signatures created successfully\n");

  IGL_D3D12_LOG_VERBOSE("HeadlessD3D12Context: Initialization complete\n");
  return Result();
}

} // namespace igl::d3d12

