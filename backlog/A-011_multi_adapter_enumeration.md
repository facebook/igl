# A-011: Multi-Adapter Enumeration Incomplete

## 1. Problem Statement

**Severity:** MEDIUM
**Category:** A - Architecture
**Status:** Open
**Related Issues:** A-001

### Detailed Explanation

The D3D12 backend's adapter enumeration logic is incomplete and lacks support for multi-GPU systems. The current implementation has several limitations:

1. **No Explicit Adapter Selection:** Users cannot specify which GPU to use on multi-GPU systems
2. **Incomplete Enumeration:** Code iterates through adapters but doesn't expose enumeration results to applications
3. **Missing Adapter Information:** No mechanism to query adapter capabilities (VRAM size, architecture, vendor)
4. **No LUID Tracking:** Adapter LUIDs are not stored for cross-API correlation (D3D12/DXGI/Vulkan)
5. **Limited Fallback Logic:** When preferred adapter fails, fallback to next adapter is not implemented
6. **No Software Adapter Option:** No explicit support for WARP (software rasterizer) adapter selection

### The Danger

- **Wrong GPU Selection:** On hybrid GPU laptops (integrated + discrete), IGL may select the wrong GPU
- **Performance Loss:** Applications run on integrated GPU instead of high-performance discrete GPU
- **Power Inefficiency:** Rendering on discrete GPU when integrated GPU would suffice (battery drain)
- **No User Control:** Users cannot override GPU selection for testing or power management
- **Cross-API Confusion:** No way to ensure D3D12 and Vulkan use the same physical device
- **Testing Limitations:** Cannot test on specific adapters or software rasterizer

## 2. Root Cause Analysis

### Current Implementation

From `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\D3D12Context.cpp` (around line 311):

```cpp
auto getHighestFeatureLevel = [](IDXGIAdapter1* adapter) -> D3D_FEATURE_LEVEL {
  // ... feature level detection ...
};

// Choose the adapter with the highest D3D12 feature level
Microsoft::WRL::ComPtr<IDXGIAdapter1> bestAdapter;
D3D_FEATURE_LEVEL highestFeatureLevel = D3D_FEATURE_LEVEL_11_0;

for (UINT i = 0; ; ++i) {
  Microsoft::WRL::ComPtr<IDXGIAdapter1> cand;
  if (dxgiFactory_->EnumAdapters1(i, cand.GetAddressOf()) == DXGI_ERROR_NOT_FOUND) {
    break;
  }

  DXGI_ADAPTER_DESC1 desc{};
  cand->GetAdapterDesc1(&desc);

  // SKIP software adapters without logging or user control
  if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
    continue;
  }

  D3D_FEATURE_LEVEL fl = getHighestFeatureLevel(cand.Get());
  if (fl > highestFeatureLevel) {
    highestFeatureLevel = fl;
    bestAdapter = cand;
  }
}

// NO ADAPTER INFORMATION EXPOSED:
// - Adapter list not saved
// - LUID not tracked
// - No way to query adapter capabilities
// - No user override mechanism
```

From `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\HeadlessContext.cpp` (around line 58):

```cpp
// Headless context has different adapter selection logic (inconsistent)
for (UINT i = 0; ; ++i) {
  Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
  if (dxgiFactory_->EnumAdapters1(i, adapter.GetAddressOf()) == DXGI_ERROR_NOT_FOUND) {
    break;
  }

  DXGI_ADAPTER_DESC1 desc{};
  adapter->GetAdapterDesc1(&desc);

  // Skip software adapters differently than windowed context
  if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
    continue;
  }

  // First adapter that supports D3D12 is selected (not highest feature level)
  if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr))) {
    adapter_ = adapter;
    break;
  }
}
```

### Technical Explanation

The root causes are:

1. **Inconsistent Selection Logic:** Windowed context picks highest feature level, headless context picks first compatible adapter
2. **No Adapter Registry:** Enumerated adapters are discarded after selection
3. **Missing LUID Storage:** `DXGI_ADAPTER_DESC1::AdapterLuid` is not stored for device tracking
4. **No Explicit Override:** No environment variable or API to force specific adapter
5. **Silent Software Adapter Skip:** WARP adapter is skipped without logging
6. **No Capability Query:** Applications cannot query adapter VRAM, frequency, or vendor ID

## 3. Official Documentation References

1. **Enumerating Adapters**
   https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/d3d10-graphics-programming-guide-dxgi
   *Key Guidance:* Enumerate all adapters and expose selection to application; track adapter LUID for device correlation

2. **IDXGIFactory1::EnumAdapters1**
   https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgifactory1-enumadapters1
   *Key Guidance:* Returns adapters in preference order (discrete GPU first, then integrated, then software)

3. **DXGI_ADAPTER_DESC1 Structure**
   https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ns-dxgi-dxgi_adapter_desc1
   *Key Guidance:* Contains VRAM size, adapter LUID, vendor ID, device ID, and software flag

4. **WARP (Software Rasterizer)**
   https://learn.microsoft.com/en-us/windows/win32/direct3darticles/directx-warp
   *Key Guidance:* Useful for testing and systems without hardware GPU; enumerate via IDXGIFactory4::EnumWarpAdapter

5. **Multi-GPU Programming**
   https://learn.microsoft.com/en-us/windows/win32/direct3d12/multi-engine
   *Key Guidance:* Applications should enumerate adapters, display list to user, and allow explicit selection

## 4. Code Location Strategy

### Files to Modify

**File:** `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\D3D12Context.h`
- **Search Pattern:** `Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter_`
- **Context:** Adapter storage around line 196
- **Action:** Add adapter enumeration results and LUID tracking

**File:** `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\D3D12Context.cpp`
- **Search Pattern:** `EnumAdapters1` and adapter selection loop
- **Context:** Adapter enumeration in createDevice() around line 359
- **Action:** Store all enumerated adapters, add explicit selection mechanism

**File:** `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\PlatformDevice.h` (if exists)
- **Search Pattern:** `class PlatformDevice`
- **Context:** Platform-specific device information
- **Action:** Expose adapter list and selection API

**File:** `C:\Users\rudyb\source\repos\igl\igl\src\igl\d3d12\HeadlessContext.cpp`
- **Search Pattern:** `EnumAdapters1`
- **Context:** Headless adapter selection around line 78
- **Action:** Align with windowed context adapter selection logic

## 5. Detection Strategy

### How to Reproduce

**Test Scenario 1: Multi-GPU System**

```cpp
// On laptop with integrated + discrete GPU
auto device = createD3D12Device();

// Current behavior: No way to know which GPU was selected
// Expected: Should enumerate both GPUs and allow selection

// Try to query adapter (not implemented):
// std::vector<AdapterInfo> adapters = device->enumerateAdapters();
// device->selectAdapter(1);  // Select discrete GPU
```

**Test Scenario 2: WARP Testing**

```cpp
// Try to force WARP adapter for testing
// Current: No mechanism to select WARP
// Expected: Environment variable like IGL_D3D12_ADAPTER=WARP
```

**Manual Testing:**

1. Run IGL application on laptop with hybrid GPU (integrated + discrete)
2. Check Task Manager > Performance > GPU to see which GPU is active
3. Observe: No control over GPU selection
4. Note: Application may use wrong GPU

### Verification Steps

After implementing adapter enumeration:

```bash
# Build with adapter enumeration enabled
cd C:\Users\rudyb\source\repos\igl\igl\build
cmake --build . --config Debug

# Run with adapter logging
cd C:\Users\rudyb\source\repos\igl\igl\build\shell\renderSessions\Debug
.\igl_shell_RenderSessions.exe --session=HelloWorld

# Expected log output:
# "D3D12Context: Enumerating adapters..."
# "D3D12Context: Adapter 0:"
# "  Description: NVIDIA GeForce RTX 3060"
# "  Vendor ID: 0x10DE (NVIDIA)"
# "  Device ID: 0x2504"
# "  Dedicated VRAM: 6144 MB"
# "  Feature Level: D3D_FEATURE_LEVEL_12_1"
# "  LUID: 0x00000000:0x0000D3C8"
# "D3D12Context: Adapter 1:"
# "  Description: Intel(R) UHD Graphics 630"
# "  Vendor ID: 0x8086 (Intel)"
# "  Device ID: 0x3E9B"
# "  Dedicated VRAM: 128 MB"
# "  Feature Level: D3D_FEATURE_LEVEL_12_1"
# "  LUID: 0x00000000:0x0000D1A0"
# "D3D12Context: Selected adapter 0 (NVIDIA GeForce RTX 3060)"

# Test explicit adapter selection
set IGL_D3D12_ADAPTER=1
.\igl_shell_RenderSessions.exe --session=HelloWorld
# Expected: "D3D12Context: Environment override - using adapter 1"

# Test WARP adapter
set IGL_D3D12_ADAPTER=WARP
.\igl_shell_RenderSessions.exe --session=HelloWorld
# Expected: "D3D12Context: Using WARP software adapter"
```

## 6. Fix Guidance

### Implementation Steps

**Step 1: Add Adapter Information Storage to D3D12Context.h**

Add after line 196 (`Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter_`):

```cpp
// A-011: Multi-adapter enumeration and tracking
struct AdapterInfo {
  Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
  DXGI_ADAPTER_DESC1 desc;
  D3D_FEATURE_LEVEL featureLevel;
  bool isWarp;  // Software rasterizer
  uint32_t index;  // Original enumeration index

  // Helper methods
  uint64_t getDedicatedVideoMemoryMB() const {
    return desc.DedicatedVideoMemory / (1024 * 1024);
  }

  const char* getVendorName() const {
    switch (desc.VendorId) {
      case 0x10DE: return "NVIDIA";
      case 0x1002: return "AMD";
      case 0x8086: return "Intel";
      case 0x1414: return "Microsoft (WARP)";
      default: return "Unknown";
    }
  }
};

std::vector<AdapterInfo> enumeratedAdapters_;
uint32_t selectedAdapterIndex_ = 0;

// Query all available adapters
const std::vector<AdapterInfo>& getEnumeratedAdapters() const {
  return enumeratedAdapters_;
}

// Get currently selected adapter
const AdapterInfo* getSelectedAdapter() const {
  if (selectedAdapterIndex_ < enumeratedAdapters_.size()) {
    return &enumeratedAdapters_[selectedAdapterIndex_];
  }
  return nullptr;
}

// Enumerate and select adapter
void enumerateAndSelectAdapter();
```

**Step 2: Implement enumerateAndSelectAdapter() in D3D12Context.cpp**

Replace adapter selection logic in `createDevice()` (around line 359):

```cpp
void D3D12Context::enumerateAndSelectAdapter() {
  enumeratedAdapters_.clear();

  IGL_LOG_INFO("D3D12Context: Enumerating DXGI adapters...\n");

  // Helper to get highest feature level for adapter
  auto getHighestFeatureLevel = [](IDXGIAdapter1* adapter) -> D3D_FEATURE_LEVEL {
    const D3D_FEATURE_LEVEL levels[] = {
      D3D_FEATURE_LEVEL_12_1,
      D3D_FEATURE_LEVEL_12_0,
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
    };

    for (D3D_FEATURE_LEVEL level : levels) {
      if (SUCCEEDED(D3D12CreateDevice(adapter, level, _uuidof(ID3D12Device), nullptr))) {
        return level;
      }
    }
    return D3D_FEATURE_LEVEL_11_0;
  };

  // Enumerate hardware adapters
  for (UINT i = 0; ; ++i) {
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    if (dxgiFactory_->EnumAdapters1(i, adapter.GetAddressOf()) == DXGI_ERROR_NOT_FOUND) {
      break;
    }

    AdapterInfo info{};
    info.adapter = adapter;
    info.index = i;
    info.isWarp = false;

    adapter->GetAdapterDesc1(&info.desc);

    // Check if software adapter
    if (info.desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
      IGL_LOG_INFO("D3D12Context: Adapter %u is software adapter (skipping from main enum)\n", i);
      continue;
    }

    // Determine feature level
    info.featureLevel = getHighestFeatureLevel(adapter.Get());
    if (info.featureLevel < D3D_FEATURE_LEVEL_11_0) {
      IGL_LOG_INFO("D3D12Context: Adapter %u does not support D3D12 (skipping)\n", i);
      continue;
    }

    enumeratedAdapters_.push_back(info);

    // Log adapter details
    IGL_LOG_INFO("D3D12Context: Adapter %u:\n", i);
    IGL_LOG_INFO("  Description: %ls\n", info.desc.Description);
    IGL_LOG_INFO("  Vendor ID: 0x%04X (%s)\n", info.desc.VendorId, info.getVendorName());
    IGL_LOG_INFO("  Device ID: 0x%04X\n", info.desc.DeviceId);
    IGL_LOG_INFO("  Subsystem ID: 0x%08X\n", info.desc.SubSysId);
    IGL_LOG_INFO("  Revision: %u\n", info.desc.Revision);
    IGL_LOG_INFO("  Dedicated VRAM: %llu MB\n", info.getDedicatedVideoMemoryMB());
    IGL_LOG_INFO("  Shared VRAM: %llu MB\n", info.desc.SharedSystemMemory / (1024 * 1024));
    IGL_LOG_INFO("  Feature Level: %s\n", featureLevelToString(info.featureLevel));
    IGL_LOG_INFO("  LUID: 0x%08X:0x%08X\n", info.desc.AdapterLuid.HighPart, info.desc.AdapterLuid.LowPart);
  }

  // Add WARP adapter as fallback option (software rasterizer)
  Microsoft::WRL::ComPtr<IDXGIFactory4> factory4;
  if (SUCCEEDED(dxgiFactory_.As(&factory4))) {
    Microsoft::WRL::ComPtr<IDXGIAdapter1> warpAdapter;
    if (SUCCEEDED(factory4->EnumWarpAdapter(IID_PPV_ARGS(warpAdapter.GetAddressOf())))) {
      AdapterInfo warpInfo{};
      warpInfo.adapter = warpAdapter;
      warpInfo.index = static_cast<uint32_t>(enumeratedAdapters_.size());
      warpInfo.isWarp = true;

      warpAdapter->GetAdapterDesc1(&warpInfo.desc);
      warpInfo.featureLevel = getHighestFeatureLevel(warpAdapter.Get());

      enumeratedAdapters_.push_back(warpInfo);

      IGL_LOG_INFO("D3D12Context: WARP Adapter (Software):\n");
      IGL_LOG_INFO("  Description: %ls\n", warpInfo.desc.Description);
      IGL_LOG_INFO("  Feature Level: %s\n", featureLevelToString(warpInfo.featureLevel));
    }
  }

  if (enumeratedAdapters_.empty()) {
    IGL_LOG_ERROR("D3D12Context: No compatible D3D12 adapters found!\n");
    throw std::runtime_error("No D3D12-compatible adapters available");
  }

  // Select adapter based on environment variable or heuristic
  selectedAdapterIndex_ = 0;  // Default to first adapter (discrete GPU on laptops)

  char adapterEnv[64] = {};
  if (GetEnvironmentVariableA("IGL_D3D12_ADAPTER", adapterEnv, sizeof(adapterEnv)) > 0) {
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
    // (typically discrete GPU on laptops)
    D3D_FEATURE_LEVEL highestFL = enumeratedAdapters_[0].featureLevel;
    size_t largestVRAM = enumeratedAdapters_[0].getDedicatedVideoMemoryMB();

    for (size_t i = 1; i < enumeratedAdapters_.size(); ++i) {
      if (enumeratedAdapters_[i].isWarp) {
        continue;  // Skip WARP for automatic selection
      }

      size_t vram = enumeratedAdapters_[i].getDedicatedVideoMemoryMB();
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

  IGL_LOG_INFO("D3D12Context: Selected adapter %u: %ls\n",
               selectedAdapterIndex_,
               enumeratedAdapters_[selectedAdapterIndex_].desc.Description);
}
```

**Step 3: Call enumerateAndSelectAdapter() in createDevice()**

Replace existing adapter selection code:

```cpp
void D3D12Context::createDevice() {
  // A-011: Enumerate and select adapter
  enumerateAndSelectAdapter();

  // Create device on selected adapter
  const auto& selectedAdapter = enumeratedAdapters_[selectedAdapterIndex_];

  HRESULT hr = D3D12CreateDevice(
      adapter_.Get(),
      selectedAdapter.featureLevel,
      IID_PPV_ARGS(device_.GetAddressOf()));

  if (FAILED(hr)) {
    IGL_LOG_ERROR("D3D12CreateDevice failed on adapter %u: 0x%08X\n",
                  selectedAdapterIndex_, static_cast<unsigned>(hr));
    throw std::runtime_error("Failed to create D3D12 device");
  }

  IGL_LOG_INFO("D3D12: Device created on %ls (FL %s)\n",
               selectedAdapter.desc.Description,
               featureLevelToString(selectedAdapter.featureLevel));

  // ... rest of device initialization ...
}
```

**Step 4: Update HeadlessContext to Use Same Logic**

In `HeadlessContext.cpp`, replace adapter selection with:

```cpp
void HeadlessD3D12Context::createDevice() {
  // A-011: Use consistent adapter enumeration logic
  enumerateAndSelectAdapter();

  // Create device (rest of code same as windowed context)
  // ...
}
```

**Step 5: Add Adapter Information to PlatformDevice (Optional)**

Expose adapter list through IGL's cross-platform API:

```cpp
// In PlatformDevice.h
class PlatformDevice {
public:
  // ... existing methods ...

  // A-011: Query adapter information
  virtual size_t getAdapterCount() const = 0;
  virtual std::string getAdapterName(size_t index) const = 0;
  virtual size_t getAdapterMemoryMB(size_t index) const = 0;
  virtual size_t getSelectedAdapterIndex() const = 0;
};
```

**Rationale:**
- Complete enumeration provides visibility into all available GPUs
- Adapter LUID tracking enables cross-API device correlation
- Environment variable override simplifies testing and debugging
- WARP adapter support enables testing without hardware GPU
- Consistent selection logic between windowed and headless contexts
- Heuristic selection (highest FL + most VRAM) works well for laptops

### Before/After Code Comparison

**Before (incomplete enumeration):**
```cpp
// Iterate through adapters but don't save list
for (UINT i = 0; ; ++i) {
  Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
  if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) break;

  // Skip software adapters silently
  if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

  // Pick first compatible adapter (inconsistent between contexts)
  // No logging, no user control, no LUID tracking
}
```

**After (complete enumeration with control):**
```cpp
// Enumerate ALL adapters (hardware + WARP)
for (UINT i = 0; ; ++i) {
  // ... create AdapterInfo with full details ...
  enumeratedAdapters_.push_back(info);

  IGL_LOG_INFO("Adapter %u: %ls, VRAM=%lluMB, FL=%s, LUID=%08X:%08X\n", ...);
}

// Check environment variable for override
if (getenv("IGL_D3D12_ADAPTER") == "WARP") {
  selectedAdapterIndex_ = warpAdapterIndex;
}

// Select best adapter using consistent heuristic
adapter_ = enumeratedAdapters_[selectedAdapterIndex_].adapter;
IGL_LOG_INFO("Selected adapter: %ls\n", ...);
```

## 7. Testing Requirements

### Unit Tests

**Command:**
```bash
cd C:\Users\rudyb\source\repos\igl\igl\build
ctest -C Debug -R "d3d12.*adapter" --verbose
```

**Allowed Test Modifications:**
- Add new test `AdapterEnumeration` to verify all adapters are enumerated
- Add test `AdapterSelection` to verify environment variable override works
- Add test `WarpAdapterAvailable` to verify WARP adapter is included

**New Test Cases to Add:**

```cpp
// In D3D12DeviceTest.cpp
TEST(D3D12AdapterTest, EnumerationComplete) {
  auto ctx = createD3D12Context();

  auto adapters = ctx.getEnumeratedAdapters();

  // Should enumerate at least one adapter
  EXPECT_GT(adapters.size(), 0);

  // Log all adapters
  for (const auto& adapter : adapters) {
    std::cout << "Adapter " << adapter.index << ": "
              << wstringToString(adapter.desc.Description) << "\n";
    std::cout << "  VRAM: " << adapter.getDedicatedVideoMemoryMB() << " MB\n";
    std::cout << "  Vendor: " << adapter.getVendorName() << "\n";
    std::cout << "  WARP: " << (adapter.isWarp ? "YES" : "NO") << "\n";
  }
}

TEST(D3D12AdapterTest, SelectionOverride) {
  // Test environment variable override (if multiple adapters available)
  SetEnvironmentVariableA("IGL_D3D12_ADAPTER", "0");

  auto ctx = createD3D12Context();
  EXPECT_EQ(ctx.getSelectedAdapter()->index, 0);

  // Cleanup
  SetEnvironmentVariableA("IGL_D3D12_ADAPTER", nullptr);
}

TEST(D3D12AdapterTest, WarpAvailable) {
  auto ctx = createD3D12Context();
  auto adapters = ctx.getEnumeratedAdapters();

  // Find WARP adapter
  bool foundWarp = false;
  for (const auto& adapter : adapters) {
    if (adapter.isWarp) {
      foundWarp = true;
      EXPECT_NE(adapter.adapter.Get(), nullptr);
      break;
    }
  }

  EXPECT_TRUE(foundWarp) << "WARP adapter should always be available";
}
```

### Render Session Tests

**Commands:**
```bash
cd C:\Users\rudyb\source\repos\igl\igl\build\shell\renderSessions\Debug

# Test adapter enumeration logging
.\igl_shell_RenderSessions.exe --session=HelloWorld --frames=10

# Expected output:
# "D3D12Context: Enumerating DXGI adapters..."
# "D3D12Context: Adapter 0: [GPU name]"
# "  Vendor ID: [vendor]"
# "  Dedicated VRAM: [size] MB"
# "  Feature Level: [level]"
# "D3D12Context: Selected adapter 0: [GPU name]"

# Test explicit adapter selection (if multiple GPUs)
set IGL_D3D12_ADAPTER=1
.\igl_shell_RenderSessions.exe --session=HelloWorld --frames=10

# Expected: "D3D12Context: Environment override - using adapter 1"

# Test WARP adapter
set IGL_D3D12_ADAPTER=WARP
.\igl_shell_RenderSessions.exe --session=HelloWorld --frames=10

# Expected: "D3D12Context: Environment override - using WARP adapter"
# Should render correctly (slower) using software rasterizer
```

## 8. Definition of Done

### Completion Criteria

- [ ] `AdapterInfo` struct defined with complete adapter details (description, VRAM, LUID, vendor)
- [ ] `enumeratedAdapters_` vector stores all compatible adapters
- [ ] `enumerateAndSelectAdapter()` enumerates hardware adapters via EnumAdapters1()
- [ ] WARP adapter enumerated via EnumWarpAdapter() and included in list
- [ ] Adapter selection uses consistent heuristic (highest feature level + most VRAM)
- [ ] Environment variable `IGL_D3D12_ADAPTER` allows explicit selection by index or "WARP"
- [ ] Comprehensive logging of all enumerated adapters with details
- [ ] Selected adapter logged with name and index
- [ ] HeadlessContext uses same enumeration logic as windowed context
- [ ] Unit tests verify adapter enumeration and selection override
- [ ] Render sessions work with explicit adapter selection

### User Confirmation Required

Before proceeding to next task, confirm:

```
Task A-011 (Multi-Adapter Enumeration) has been completed. Please verify:

1. Does the log show all available GPUs with details (VRAM, vendor, feature level)?
2. Can you override adapter selection with IGL_D3D12_ADAPTER environment variable?
3. Does WARP adapter work correctly when explicitly selected?
4. On multi-GPU systems, is the correct GPU (discrete) selected by default?

Reply with:
- "Confirmed - proceed to next task" if all checks pass
- "Issues found: [description]" if problems are discovered
- "Needs more testing" if additional verification is required
```

## 9. Related Issues

### Blocks
None - Capability detection and configuration only

### Related
- **A-001:** Hardcoded feature level (adapter enumeration improves feature detection)
- **A-010:** HDR output detection (HDR capabilities vary by adapter)

## 10. Implementation Priority

**Priority:** P1-Medium
**Estimated Effort:** 5-6 hours
- 1 hour: Define AdapterInfo struct and storage
- 2 hours: Implement enumerateAndSelectAdapter() with logging
- 1 hour: Add environment variable override mechanism
- 1 hour: Update HeadlessContext to use consistent logic
- 1 hour: Testing and validation on multi-GPU system

**Risk Assessment:** Low
- Changes are additive (enumeration + logging)
- Default selection heuristic works well for common cases
- Environment variable provides escape hatch for testing
- No impact on single-GPU systems

**Impact:** Medium
- Enables proper GPU selection on multi-GPU systems
- Improves testing capability (WARP adapter, specific GPU selection)
- Provides visibility into available hardware
- Foundation for future multi-GPU rendering support

## 11. References

- https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/d3d10-graphics-programming-guide-dxgi
- https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgifactory1-enumadapters1
- https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ns-dxgi-dxgi_adapter_desc1
- https://learn.microsoft.com/en-us/windows/win32/direct3darticles/directx-warp
- https://learn.microsoft.com/en-us/windows/win32/direct3d12/multi-engine
- https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12Multithreading
