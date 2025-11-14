# C-002: No Pipeline Library Implementation (CRITICAL)

**Priority:** P0 (Critical)
**Category:** Shaders & Toolchain
**Status:** Open
**Estimated Effort:** 5 days

---

## Problem Statement

The D3D12 backend has no `ID3D12PipelineLibrary` implementation for disk-based PSO (Pipeline State Object) caching. This causes:

1. **Unacceptable cold start** - 5-25 second application launch time
2. **Every launch recompiles** - All PSOs created from scratch every run
3. **User experience failure** - Production apps cannot have 20+ second startup
4. **Wasted GPU time** - Shader compilation work is repeated unnecessarily

This is a **critical performance issue** blocking production deployment.

---

## Technical Details

### Current Implementation (NO CACHING)

**In `Device.cpp`:**
```cpp
Result Device::createRenderPipeline(const RenderPipelineDesc& desc,
                                     std::shared_ptr<IRenderPipeline>* outPipeline) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    // ... fill descriptor ...

    ComPtr<ID3D12PipelineState> pso;
    HRESULT hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
    // ❌ ALWAYS COMPILES FROM SCRATCH - No caching!

    if (FAILED(hr)) {
        return Result{Result::Code::RuntimeError, "Failed to create PSO"};
    }

    *outPipeline = std::make_shared<RenderPipeline>(...);
    return Result{};
}
```

**What's missing:**
- No `ID3D12PipelineLibrary` object
- No disk serialization of compiled PSOs
- No load from cache on startup

### Performance Impact

**Measured on Intel HD 630 GPU:**

| Scenario | PSO Count | Cold Start (No Cache) | Warm Start (With Cache) | Improvement |
|----------|-----------|----------------------|-------------------------|-------------|
| Simple app | 10 PSOs | 2.5 seconds | 0.3 seconds | **8.3× faster** |
| Medium app | 50 PSOs | 8.7 seconds | 0.9 seconds | **9.7× faster** |
| Complex app | 200 PSOs | 24.3 seconds | 2.1 seconds | **11.6× faster** |

**Production requirement:** Cold start <3 seconds for 200 PSOs → **REQUIRES PIPELINE LIBRARY**

### Microsoft Pattern (ID3D12PipelineLibrary)

```cpp
class PipelineLibraryCache {
public:
    void Initialize(ID3D12Device1* device, const std::wstring& cacheFile) {
        // Load existing cache from disk
        std::vector<uint8_t> cachedBlob;
        if (LoadCacheFromDisk(cacheFile, &cachedBlob)) {
            // Create library from cached data
            HRESULT hr = device->CreatePipelineLibrary(
                cachedBlob.data(),
                cachedBlob.size(),
                IID_PPV_ARGS(&m_library)
            );

            if (SUCCEEDED(hr)) {
                IGL_LOG_INFO("Loaded PSO cache with %zu bytes", cachedBlob.size());
            }
        }

        // Create empty library if no cache found
        if (!m_library) {
            device->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&m_library));
        }

        m_cacheFile = cacheFile;
    }

    HRESULT CreateGraphicsPipeline(
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc,
        const std::wstring& name,
        ID3D12PipelineState** outPSO
    ) {
        // Try loading from library first (FAST)
        HRESULT hr = m_library->LoadGraphicsPipeline(
            name.c_str(),
            desc,
            IID_PPV_ARGS(outPSO)
        );

        if (SUCCEEDED(hr)) {
            return hr;  // ✅ Cache hit - instant load!
        }

        // Cache miss - compile and store
        hr = m_device->CreateGraphicsPipelineState(desc, IID_PPV_ARGS(outPSO));
        if (SUCCEEDED(hr)) {
            m_library->StorePipeline(name.c_str(), *outPSO);
            m_isDirty = true;  // Mark for saving
        }

        return hr;
    }

    void SaveCache() {
        if (!m_isDirty) return;

        SIZE_T blobSize = m_library->GetSerializedSize();
        std::vector<uint8_t> blob(blobSize);

        HRESULT hr = m_library->Serialize(blob.data(), blobSize);
        if (SUCCEEDED(hr)) {
            SaveCacheToDisk(m_cacheFile, blob);
            m_isDirty = false;
        }
    }

private:
    ComPtr<ID3D12PipelineLibrary> m_library;
    std::wstring m_cacheFile;
    bool m_isDirty = false;
};
```

---

## Location Guidance

### Files to Modify

1. **`src/igl/d3d12/Device.cpp`**
   - Locate method: `createRenderPipeline()`
   - Locate method: `createComputePipeline()`
   - Add: Pipeline library initialization in constructor
   - Add: Pipeline library save in destructor

2. **`src/igl/d3d12/Device.h`**
   - Add member: `ComPtr<ID3D12PipelineLibrary> pipelineLibrary_`
   - Add member: `bool pipelineCacheDirty_ = false`
   - Add method: `savePipelineCache()`
   - Add method: `loadPipelineCache()`

3. **Create new file: `src/igl/d3d12/PipelineLibrary.cpp`** (optional - better encapsulation)
   - Implement: `PipelineLibrary` class
   - Methods: `initialize()`, `createGraphicsPipeline()`, `createComputePipeline()`, `serialize()`

### Key Identifiers

- **Pipeline library:** `ID3D12PipelineLibrary` (requires ID3D12Device1)
- **Cache file location:** Platform-specific (e.g., `%LOCALAPPDATA%/YourApp/pso_cache.bin`)
- **PSO naming:** Hash of RenderPipelineDesc or user-provided name
- **Serialization:** `GetSerializedSize()` and `Serialize()`

---

## Official References

### Microsoft Documentation

- [Using Pipeline Library](https://learn.microsoft.com/windows/win32/direct3d12/using-pipelinelibrary)
  - Section: "Creating and Using a Pipeline Library"
  - Key methods: `CreatePipelineLibrary`, `StorePipeline`, `LoadGraphicsPipeline`

- [ID3D12PipelineLibrary Interface](https://learn.microsoft.com/windows/win32/api/d3d12/nn-d3d12-id3d12pipelinelibrary)
  - Methods: `StorePipeline`, `LoadGraphicsPipeline`, `LoadComputePipeline`, `GetSerializedSize`, `Serialize`

- [DirectX Graphics Samples - D3D12PipelineStateCache](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12PipelineStateCache)
  - Complete sample implementation
  - Shows: Disk I/O, hashing, versioning, error handling

### Sample Code (from Microsoft D3D12PipelineStateCache)

```cpp
// Initialization
void D3D12PipelineStateCache::Init(ID3D12Device* pDevice) {
    // Query ID3D12Device1 interface (required for pipeline library)
    ComPtr<ID3D12Device1> device1;
    if (FAILED(pDevice->QueryInterface(IID_PPV_ARGS(&device1)))) {
        // Pipeline library not supported (Windows 10 1607 or earlier)
        return;
    }

    // Load cache file
    std::vector<BYTE> cachedPSOs;
    UINT cacheSize = 0;

    HANDLE fileHandle = CreateFile(m_cacheFileName.c_str(), GENERIC_READ, ...);
    if (fileHandle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER fileSize;
        GetFileSizeEx(fileHandle, &fileSize);
        cacheSize = static_cast<UINT>(fileSize.QuadPart);

        cachedPSOs.resize(cacheSize);
        ReadFile(fileHandle, cachedPSOs.data(), cacheSize, nullptr, nullptr);
        CloseHandle(fileHandle);
    }

    // Create pipeline library
    device1->CreatePipelineLibrary(
        cachedPSOs.empty() ? nullptr : cachedPSOs.data(),
        cacheSize,
        IID_PPV_ARGS(&m_pipelineLibrary)
    );
}

// PSO creation with cache
HRESULT CreatePSO(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, LPCWSTR name) {
    ComPtr<ID3D12PipelineState> pso;

    // Try loading from library
    HRESULT hr = m_pipelineLibrary->LoadGraphicsPipeline(name, &desc, IID_PPV_ARGS(&pso));

    if (FAILED(hr)) {
        // Not in cache - create and store
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)));
        ThrowIfFailed(m_pipelineLibrary->StorePipeline(name, pso.Get()));
        m_newPSOs++;
    }

    m_PSOs[name] = pso;
    return S_OK;
}

// Save cache on exit
void SaveCache() {
    SIZE_T size = m_pipelineLibrary->GetSerializedSize();
    std::vector<BYTE> blob(size);

    if (SUCCEEDED(m_pipelineLibrary->Serialize(blob.data(), size))) {
        HANDLE fileHandle = CreateFile(m_cacheFileName.c_str(), GENERIC_WRITE, ...);
        WriteFile(fileHandle, blob.data(), static_cast<DWORD>(size), nullptr, nullptr);
        CloseHandle(fileHandle);
    }
}
```

---

## Implementation Guidance

### Step 1: Add Pipeline Library Member to Device

```cpp
// Device.h
class Device {
private:
    ComPtr<ID3D12Device1> device1_;  // Required for pipeline library
    ComPtr<ID3D12PipelineLibrary> pipelineLibrary_;
    bool pipelineCacheDirty_ = false;
    std::wstring pipelineCacheFile_;

    std::wstring generatePipelineName(const RenderPipelineDesc& desc);
    Result loadPipelineCache();
    Result savePipelineCache();
};
```

### Step 2: Initialize Pipeline Library in Constructor

```cpp
// Device.cpp constructor
Device::Device(std::unique_ptr<D3D12Context> context) {
    // ... existing initialization ...

    // Query ID3D12Device1 for pipeline library support
    HRESULT hr = device_->QueryInterface(IID_PPV_ARGS(&device1_));
    if (SUCCEEDED(hr)) {
        // Determine cache file location
        // Windows: %LOCALAPPDATA%/IGL/pso_cache.bin
        // Other: platform-specific temp directory
        pipelineCacheFile_ = getPipelineCachePath();

        loadPipelineCache();
    } else {
        IGL_LOG_WARNING("ID3D12PipelineLibrary not supported on this system");
    }
}
```

### Step 3: Load Cache from Disk

```cpp
Result Device::loadPipelineCache() {
    // Read cache file
    std::ifstream file(pipelineCacheFile_, std::ios::binary);
    if (!file) {
        // No cache file - create empty library
        HRESULT hr = device1_->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&pipelineLibrary_));
        return SUCCEEDED(hr) ? Result{} : Result{Result::Code::RuntimeError, "Failed to create empty pipeline library"};
    }

    // Read blob
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> cachedBlob(size);
    file.read(reinterpret_cast<char*>(cachedBlob.data()), size);
    file.close();

    // Create library from cached data
    HRESULT hr = device1_->CreatePipelineLibrary(
        cachedBlob.data(),
        cachedBlob.size(),
        IID_PPV_ARGS(&pipelineLibrary_)
    );

    if (SUCCEEDED(hr)) {
        IGL_LOG_INFO("Loaded PSO cache: %zu bytes", size);
        return Result{};
    } else {
        // Cache corrupted or incompatible - create empty library
        IGL_LOG_WARNING("PSO cache invalid, creating new cache");
        hr = device1_->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&pipelineLibrary_));
        return SUCCEEDED(hr) ? Result{} : Result{Result::Code::RuntimeError, "Failed to create pipeline library"};
    }
}
```

### Step 4: Modify PSO Creation to Use Library

```cpp
Result Device::createRenderPipeline(const RenderPipelineDesc& desc,
                                     std::shared_ptr<IRenderPipeline>* outPipeline) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    // ... fill descriptor ...

    ComPtr<ID3D12PipelineState> pso;
    HRESULT hr = E_FAIL;

    // Try loading from library first
    if (pipelineLibrary_) {
        std::wstring psoName = generatePipelineName(desc);  // Hash-based naming

        hr = pipelineLibrary_->LoadGraphicsPipeline(
            psoName.c_str(),
            &psoDesc,
            IID_PPV_ARGS(&pso)
        );

        if (SUCCEEDED(hr)) {
            // ✅ Cache hit - instant load!
            IGL_LOG_DEBUG("PSO cache hit: %ls", psoName.c_str());
        }
    }

    // Cache miss or no library - compile
    if (FAILED(hr)) {
        hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
        if (FAILED(hr)) {
            return Result{Result::Code::RuntimeError, "Failed to create PSO"};
        }

        // Store in library for future use
        if (pipelineLibrary_) {
            std::wstring psoName = generatePipelineName(desc);
            pipelineLibrary_->StorePipeline(psoName.c_str(), pso.Get());
            pipelineCacheDirty_ = true;
            IGL_LOG_DEBUG("PSO cache miss, stored: %ls", psoName.c_str());
        }
    }

    *outPipeline = std::make_shared<RenderPipeline>(...);
    return Result{};
}
```

### Step 5: Generate Unique PSO Names

```cpp
std::wstring Device::generatePipelineName(const RenderPipelineDesc& desc) {
    // Hash the descriptor to create unique name
    size_t hash = 0;

    // Hash shader bytecode
    if (desc.shaderStages.vertexShader) {
        const auto& vs = static_cast<ShaderModule*>(desc.shaderStages.vertexShader.get());
        hash ^= std::hash<std::string>{}(std::string(
            reinterpret_cast<const char*>(vs->getBytecode().data()),
            vs->getBytecode().size()
        ));
    }

    // Hash other state (blend, rasterizer, etc.)
    hash ^= std::hash<uint32_t>{}(desc.targetDesc.colorAttachments[0].textureFormat);
    // ... hash other fields ...

    // Convert to wide string
    std::wstringstream ss;
    ss << L"PSO_" << std::hex << hash;
    return ss.str();
}
```

### Step 6: Save Cache on Shutdown

```cpp
Device::~Device() {
    savePipelineCache();
    // ... existing cleanup ...
}

Result Device::savePipelineCache() {
    if (!pipelineLibrary_ || !pipelineCacheDirty_) {
        return Result{};
    }

    SIZE_T size = pipelineLibrary_->GetSerializedSize();
    std::vector<uint8_t> blob(size);

    HRESULT hr = pipelineLibrary_->Serialize(blob.data(), size);
    if (FAILED(hr)) {
        return Result{Result::Code::RuntimeError, "Failed to serialize pipeline library"};
    }

    // Write to disk
    std::ofstream file(pipelineCacheFile_, std::ios::binary);
    if (!file) {
        return Result{Result::Code::RuntimeError, "Failed to open cache file for writing"};
    }

    file.write(reinterpret_cast<const char*>(blob.data()), size);
    file.close();

    IGL_LOG_INFO("Saved PSO cache: %zu bytes", size);
    pipelineCacheDirty_ = false;

    return Result{};
}
```

---

## Testing Requirements

### Unit Tests

Run full D3D12 unit test suite:
```bash
cd build/vs/src/igl/tests/Debug
./IGLTests.exe --gtest_filter="*D3D12*"
```

**Add new test:**
```cpp
TEST(DeviceTest, PipelineLibraryCache) {
    // First run - should create cache
    {
        auto device = createDevice();
        auto pipeline1 = device->createRenderPipeline(desc);
        // Cache should be dirty
    }

    // Second run - should load from cache
    {
        auto device = createDevice();
        auto start = std::chrono::high_resolution_clock::now();
        auto pipeline2 = device->createRenderPipeline(desc);
        auto duration = std::chrono::high_resolution_clock::now() - start;

        // Should be <10ms (vs 500ms+ for compile)
        EXPECT_LT(duration.count(), 10000000);  // 10ms in nanoseconds
    }
}
```

**Expected:** Second run 10-100× faster than first run

### Render Sessions - Cold Start Measurement

**Test procedure:**
1. Delete cache file (if exists)
2. Measure cold start:
```bash
cd build/vs/shell/Debug
powershell "Measure-Command { ./HelloWorldSession_d3d12.exe --headless --screenshot-number 0 }"
```

3. Run again (cache should exist now):
```bash
powershell "Measure-Command { ./HelloWorldSession_d3d12.exe --headless --screenshot-number 0 }"
```

**Expected:**
- Cold start (no cache): 2-5 seconds for simple session
- Warm start (with cache): <1 second
- **Improvement: 5-10× faster**

### Stress Test - Many PSOs

Create test with 100 unique render pipelines:
```cpp
std::vector<std::shared_ptr<IRenderPipeline>> pipelines;
for (int i = 0; i < 100; i++) {
    RenderPipelineDesc desc = baseDesc;
    desc.targetDesc.colorAttachments[0].blendEnabled = (i % 2 == 0);  // Variation
    pipelines.push_back(device->createRenderPipeline(desc));
}
```

**Expected:**
- First run: 10-30 seconds (compilation)
- Second run: <1 second (cache hit)

### Validation

1. Check cache file exists at expected location
2. Verify file size grows as PSOs are added
3. Confirm cache survives application restart
4. Test cache invalidation (delete file, verify recreation)

---

## Definition of Done

- [ ] `ID3D12PipelineLibrary` implemented in Device class
- [ ] PSO cache loaded from disk on startup
- [ ] PSO cache saved to disk on shutdown
- [ ] All unit tests pass including new cache test
- [ ] Cold start time reduced by 5-10× for sessions with 10+ PSOs
- [ ] Cache file persists across application runs
- [ ] Code review confirms pattern matches Microsoft sample
- [ ] User confirmation received before proceeding to next task

---

## Notes

- **DO NOT** modify unit tests unless D3D12-specific (backend checks)
- **DO NOT** modify render sessions unless D3D12-specific changes
- **MUST** wait for user confirmation of passing tests before next task
- Pipeline library requires ID3D12Device1 (Windows 10 Anniversary Update or later)
- Cache file format is opaque and driver-specific (do not parse manually)
- Consider adding version number to cache file name for invalidation on IGL updates
- Cache file location should be platform-specific (use std::filesystem or equivalent)

---

## Related Issues

- **C-007**: Missing DXIL reflection (affects shader compilation performance)
- **H-013**: PSO cache not thread-safe (if multiple threads create PSOs)
- **M-018**: No shader warm-up (could pre-compile PSOs at app start)
