# I-003: MSAA Sample Count Limits Not Consistent with Vulkan

**Severity:** Medium
**Category:** Cross-Platform Portability & API Inconsistency
**Status:** Open
**Related Issues:** I-001 (Vertex Attribute Limits), I-004 (Texture Format Support Query)

---

## Problem Statement

D3D12 backend validates MSAA sample counts at texture creation time but does not expose **queryable MSAA support limits** through `getFeatureLimits()` API, creating **inconsistency with Vulkan** which provides `VkPhysicalDeviceLimits::framebufferColorSampleCounts` and `framebufferDepthSampleCounts`. This makes it **impossible for applications to query supported MSAA sample counts** before attempting to create textures, forcing a **trial-and-error approach** that results in runtime errors.

**The Impact:**
- Applications cannot query which MSAA sample counts are supported (2x, 4x, 8x, 16x, etc.)
- Vulkan backend exposes `maxSamples` through feature limits, but D3D12 does not
- Cross-platform code must use try-catch or check errors at texture creation time
- No standardized way to query format-specific MSAA support across backends
- Advanced rendering features (e.g., deferred rendering with MSAA) cannot reliably detect capabilities
- Validation happens too late (at texture creation) instead of at capability query time

**Real-World Scenario:**
```cpp
// Vulkan: Can query max MSAA samples
auto device = createDevice(BackendType::Vulkan);
size_t maxSamples;
device->getFeatureLimits(DeviceFeatureLimits::MaxSamples, &maxSamples);
// maxSamples = 8 (for example)

// D3D12: No equivalent query available!
auto d3d12Device = createDevice(BackendType::D3D12);
// How many samples does this device support? Must try and fail!

// Must attempt texture creation to discover support
TextureDesc desc;
desc.format = TextureFormat::RGBA_UNorm8;
desc.numSamples = 8;  // Will this work? Unknown!

Result result;
auto texture = d3d12Device->createTexture(desc, &result);
if (!result.isOk()) {
  // Try 4x MSAA...
  desc.numSamples = 4;
  texture = d3d12Device->createTexture(desc, &result);
  // Still failing? Try 2x...
}
```

---

## Root Cause Analysis

### Current Implementation - The Mismatch:

#### D3D12 Backend (`src/igl/d3d12/Device.cpp:654-665`)
```cpp
// Validate sample count is supported for this format
D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msqLevels = {};
msqLevels.Format = dxgiFormat;
msqLevels.SampleCount = sampleCount;
msqLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msqLevels, sizeof(msqLevels))) ||
    msqLevels.NumQualityLevels == 0) {
  char errorMsg[256];
  snprintf(errorMsg, sizeof(errorMsg),
           "Device::createTexture: Format %d does not support %u samples (MSAA not supported)",
           static_cast<int>(dxgiFormat), sampleCount);
  // ... error handling ...
}
```
**Problem:** Validation happens at **texture creation time**, no proactive query available.

#### Vulkan Backend (Provides Query)
```cpp
// Vulkan exposes maxSamples through VkPhysicalDeviceLimits
case DeviceFeatureLimits::MaxSamples:
  result = limits.framebufferColorSampleCounts;  // Queried from device
  return true;
```
**Difference:** Vulkan applications can **query before creation**, D3D12 cannot.

### Missing D3D12 getFeatureLimits() Case

**File:** `src/igl/d3d12/Device.cpp` (around line 2478)

The `getFeatureLimits()` function does NOT have a case for:
```cpp
case DeviceFeatureLimits::MaxSamples:
  // MISSING - should query D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS
  // for common formats and return maximum supported sample count
```

### Why This Is Wrong:

1. **Inconsistent API Surface**: Vulkan provides query, D3D12 does not
2. **Deferred Validation**: Errors discovered too late (at texture creation vs. capability query)
3. **Trial-and-Error Required**: Applications must attempt creation to discover support
4. **Format-Dependent Support**: MSAA support varies by format, but no format-specific query exists
5. **No Fallback Guidance**: Applications cannot gracefully degrade (e.g., 8x → 4x → 2x → 1x)

---

## Official Documentation References

### D3D12 Documentation:

1. **D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS**:
   - [Microsoft D3D12 CheckFeatureSupport](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-checkfeaturesupport)
   - [D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS Structure](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_multisample_quality_levels)
   - Quote: "Describes the multi-sampling quality levels supported by the device for a given format."
   - Fields:
     - `Format`: DXGI_FORMAT to query
     - `SampleCount`: Number of samples (1, 2, 4, 8, 16, etc.)
     - `NumQualityLevels`: Output - number of quality levels supported (0 = not supported)

2. **Standard Sample Counts**:
   - [MSAA Overview](https://learn.microsoft.com/en-us/windows/win32/api/dxgicommon/ns-dxgicommon-dxgi_sample_desc)
   - Quote: "The number of multisamples per pixel. Valid values are 1, 2, 4, 8, and (optionally) 16."
   - Feature Level 11.0+: Minimum 2x/4x/8x MSAA for color formats

3. **Format Support Flags**:
   - [D3D12_FORMAT_SUPPORT1 Enumeration](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_format_support1)
   - `D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RENDERTARGET`: Format supports MSAA render targets
   - `D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE`: Format supports MSAA resolve

### Vulkan Documentation:

1. **VkPhysicalDeviceLimits**:
   - [Vulkan Spec - Device Limits](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceLimits.html)
   - Fields:
     - `framebufferColorSampleCounts`: Bitmask of supported color attachment sample counts
     - `framebufferDepthSampleCounts`: Bitmask of supported depth attachment sample counts
     - `sampledImageColorSampleCounts`: Bitmask for sampled color images
     - `sampledImageDepthSampleCounts`: Bitmask for sampled depth images
   - Quote: "Applications should query these limits to determine MSAA support."

2. **Sample Count Flags**:
   - [VkSampleCountFlagBits](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkSampleCountFlagBits.html)
   - Values: `VK_SAMPLE_COUNT_1_BIT` (0x1), `VK_SAMPLE_COUNT_2_BIT` (0x2), `VK_SAMPLE_COUNT_4_BIT` (0x4), `VK_SAMPLE_COUNT_8_BIT` (0x8), etc.

---

## Code Location Strategy

### Files to Modify:

1. **Device.cpp** (`getFeatureLimits()` method):
   - Location: `src/igl/d3d12/Device.cpp:2478` (inside switch statement)
   - Search for: `case DeviceFeatureLimits::MaxVertexInputAttributes:`
   - Context: Device capability query function
   - Action: Add new case for `DeviceFeatureLimits::MaxSamples`

2. **Device.cpp** (`createTexture()` method):
   - Location: `src/igl/d3d12/Device.cpp:654-665`
   - Search for: `D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msqLevels`
   - Context: MSAA validation at texture creation
   - Action: Add reference comment to `getFeatureLimits()` for proactive querying

3. **Common.h** (if needed):
   - Location: `src/igl/Common.h`
   - Search for: `enum class DeviceFeatureLimits`
   - Context: Feature limits enumeration
   - Action: Verify `MaxSamples` exists (likely already present for Vulkan)

### Files to Review (No Changes Expected):

1. **Vulkan Device.cpp** (`src/igl/vulkan/Device.cpp`)
   - Already implements `MaxSamples` query correctly

2. **Metal DeviceFeatureSet.mm** (`src/igl/metal/DeviceFeatureSet.mm`)
   - Likely already implements `MaxSamples` query

---

## Detection Strategy

### How to Reproduce the Inconsistency:

#### Test 1: Query MSAA Support on Different Backends
```cpp
// Test that all backends expose MaxSamples limit
auto backends = {BackendType::D3D12, BackendType::Vulkan, BackendType::Metal};

for (auto backendType : backends) {
  auto device = createDevice(backendType);

  size_t maxSamples;
  bool hasLimit = device->getFeatureLimits(DeviceFeatureLimits::MaxSamples, &maxSamples);

  // CURRENT: D3D12 returns false, Vulkan returns true
  EXPECT_TRUE(hasLimit) << "Backend " << toString(backendType)
                        << " does not support MaxSamples query";

  EXPECT_GE(maxSamples, 1) << "Invalid max samples: " << maxSamples;

  std::cout << toString(backendType) << " max MSAA samples: " << maxSamples << "\n";
}
```

#### Test 2: Validate MSAA Texture Creation Against Query
```cpp
auto device = createDevice(BackendType::D3D12);

// Query max samples
size_t maxSamples;
ASSERT_TRUE(device->getFeatureLimits(DeviceFeatureLimits::MaxSamples, &maxSamples));

// Try to create texture with maxSamples
TextureDesc desc;
desc.format = TextureFormat::RGBA_UNorm8;
desc.width = 1024;
desc.height = 768;
desc.numSamples = maxSamples;
desc.usage = TextureDesc::TextureUsageBits::Attachment;

Result result;
auto texture = device->createTexture(desc, &result);

// Should succeed if query is accurate
ASSERT_TRUE(result.isOk()) << "Failed to create texture with max samples "
                           << maxSamples << ": " << result.message;
```

#### Test 3: Format-Specific MSAA Query (Advanced)
```cpp
// Test MSAA support for different formats
std::vector<TextureFormat> formats = {
  TextureFormat::RGBA_UNorm8,
  TextureFormat::BGRA_UNorm8,
  TextureFormat::RGBA_F16,
  TextureFormat::Z_UNorm24
};

for (auto format : formats) {
  // Query max samples for this format
  size_t maxSamples = queryMaxMSAASamplesForFormat(device, format);

  std::cout << "Format " << toString(format)
            << " supports up to " << maxSamples << "x MSAA\n";

  // Validate by creating texture
  TextureDesc desc;
  desc.format = format;
  desc.numSamples = maxSamples;
  // ... other fields ...

  Result result;
  auto texture = device->createTexture(desc, &result);
  EXPECT_TRUE(result.isOk());
}
```

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Add MaxSamples Query to D3D12 Device

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** Line ~2478 (inside `getFeatureLimits()` switch statement)

**Add new case:**
```cpp
bool Device::getFeatureLimits(DeviceFeatureLimits featureLimits, size_t& result) const {
  switch (featureLimits) {
    // ... existing cases ...

    case DeviceFeatureLimits::MaxSamples: {
      // Query maximum MSAA sample count supported by device
      // Test common sample counts (1, 2, 4, 8, 16) for RGBA_UNorm8 (most common format)
      auto* device = ctx_->getDevice();
      if (!device) {
        result = 1;  // No MSAA support if device unavailable
        return false;
      }

      // Use RGBA8 as reference format (most widely supported)
      const DXGI_FORMAT referenceFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

      // Test sample counts in descending order: 16, 8, 4, 2, 1
      const uint32_t testCounts[] = {16, 8, 4, 2, 1};

      for (uint32_t sampleCount : testCounts) {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msqLevels = {};
        msqLevels.Format = referenceFormat;
        msqLevels.SampleCount = sampleCount;
        msqLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

        HRESULT hr = device->CheckFeatureSupport(
            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
            &msqLevels,
            sizeof(msqLevels));

        if (SUCCEEDED(hr) && msqLevels.NumQualityLevels > 0) {
          result = sampleCount;
          IGL_LOG_INFO("D3D12 Device::getFeatureLimits: MaxSamples = %u (for RGBA8)\n", sampleCount);
          return true;
        }
      }

      // Fallback to 1x (no MSAA)
      result = 1;
      IGL_LOG_WARNING("D3D12 Device::getFeatureLimits: No MSAA support detected, returning 1\n");
      return true;
    }

    // ... rest of cases ...
  }
}
```

**Rationale:**
- Tests standard MSAA sample counts (16, 8, 4, 2, 1) in descending order
- Uses `DXGI_FORMAT_R8G8B8A8_UNORM` as reference format (most common)
- Returns highest supported sample count
- Provides conservative estimate (actual support varies by format)

---

#### Step 2: Add Helper Function for Format-Specific MSAA Query (Optional)

**File:** `src/igl/d3d12/Device.h`

**Add public method declaration:**
```cpp
class Device final : public IDevice {
 public:
  // ... existing methods ...

  // Query maximum MSAA sample count for a specific format
  // Returns 1 if format does not support MSAA
  [[nodiscard]] uint32_t getMaxMSAASamplesForFormat(TextureFormat format) const;

  // ... rest of class ...
};
```

**File:** `src/igl/d3d12/Device.cpp`

**Add implementation:**
```cpp
uint32_t Device::getMaxMSAASamplesForFormat(TextureFormat format) const {
  auto* device = ctx_->getDevice();
  if (!device) {
    return 1;
  }

  // Convert IGL format to DXGI format
  const DXGI_FORMAT dxgiFormat = textureFormatToDXGIFormat(format);
  if (dxgiFormat == DXGI_FORMAT_UNKNOWN) {
    IGL_LOG_ERROR("Device::getMaxMSAASamplesForFormat: Unknown format %d\n", static_cast<int>(format));
    return 1;
  }

  // Test sample counts in descending order
  const uint32_t testCounts[] = {16, 8, 4, 2, 1};

  for (uint32_t sampleCount : testCounts) {
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msqLevels = {};
    msqLevels.Format = dxgiFormat;
    msqLevels.SampleCount = sampleCount;
    msqLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

    HRESULT hr = device->CheckFeatureSupport(
        D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
        &msqLevels,
        sizeof(msqLevels));

    if (SUCCEEDED(hr) && msqLevels.NumQualityLevels > 0) {
      return sampleCount;
    }
  }

  return 1;  // No MSAA support
}
```

---

#### Step 3: Update Texture Creation Error Message

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** Line 662 (MSAA validation error message)

**Before:**
```cpp
snprintf(errorMsg, sizeof(errorMsg),
         "Device::createTexture: Format %d does not support %u samples (MSAA not supported)",
         static_cast<int>(dxgiFormat), sampleCount);
```

**After:**
```cpp
// Query maximum supported samples for better error message
const uint32_t maxSamples = getMaxMSAASamplesForFormat(format);

snprintf(errorMsg, sizeof(errorMsg),
         "Device::createTexture: Format %d does not support %u samples (max supported: %u). "
         "Query DeviceFeatureLimits::MaxSamples before texture creation.",
         static_cast<int>(dxgiFormat), sampleCount, maxSamples);
```

**Rationale:** Provides actionable guidance to developers

---

#### Step 4: Add Documentation Comment

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** Line 649 (MSAA validation section)

**Add comment:**
```cpp
// MSAA validation: Check if format supports requested sample count
// NOTE: Applications should query DeviceFeatureLimits::MaxSamples proactively
//       to avoid runtime errors. Use getMaxMSAASamplesForFormat() for format-specific queries.
if (sampleCount > 1) {
  // ... existing validation code ...
}
```

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 device capability tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Device*Limit*"

# Run texture creation tests with MSAA
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Texture*MSAA*"

# Run cross-platform feature limit tests
./build/Debug/IGLTests.exe --gtest_filter="*DeviceFeatureLimits*MaxSamples*"
```

**Expected Results:**
- D3D12 backend now returns valid `MaxSamples` limit
- All MSAA texture creation tests pass
- Cross-platform consistency: all backends support `MaxSamples` query

### New Test Cases Required:

#### Test 1: D3D12 MaxSamples Query

**File:** `src/igl/tests/d3d12/Device.cpp`

```cpp
TEST_F(DeviceD3D12Test, MaxSamplesQuery) {
  size_t maxSamples;
  bool result = device_->getFeatureLimits(DeviceFeatureLimits::MaxSamples, &maxSamples);

  ASSERT_TRUE(result) << "D3D12 should support MaxSamples query";
  EXPECT_GE(maxSamples, 1) << "Max samples should be at least 1";
  EXPECT_LE(maxSamples, 16) << "Max samples should not exceed 16";

  // Feature Level 11.0+ guarantees at least 4x MSAA for color formats
  EXPECT_GE(maxSamples, 4) << "D3D12 FL11.0+ should support at least 4x MSAA";
}
```

#### Test 2: Format-Specific MSAA Query

```cpp
TEST_F(DeviceD3D12Test, FormatSpecificMSAAQuery) {
  auto d3d12Device = static_cast<igl::d3d12::Device*>(device_.get());

  // Test common formats
  std::vector<TextureFormat> formats = {
    TextureFormat::RGBA_UNorm8,
    TextureFormat::BGRA_UNorm8,
    TextureFormat::RGBA_F16,
    TextureFormat::Z_UNorm24
  };

  for (auto format : formats) {
    uint32_t maxSamples = d3d12Device->getMaxMSAASamplesForFormat(format);

    EXPECT_GE(maxSamples, 1) << "Format " << static_cast<int>(format);
    EXPECT_LE(maxSamples, 16) << "Format " << static_cast<int>(format);

    // Validate by creating texture with max samples
    TextureDesc desc;
    desc.format = format;
    desc.width = 256;
    desc.height = 256;
    desc.numSamples = maxSamples;
    desc.usage = TextureDesc::TextureUsageBits::Attachment;

    Result result;
    auto texture = device_->createTexture(desc, &result);

    EXPECT_TRUE(result.isOk())
        << "Format " << static_cast<int>(format)
        << " should support " << maxSamples << "x MSAA: " << result.message;
  }
}
```

#### Test 3: Cross-Backend Consistency

```cpp
TEST(CrossPlatform, MaxSamplesQueryConsistency) {
  auto backends = {BackendType::D3D12, BackendType::Vulkan, BackendType::Metal};

  for (auto backendType : backends) {
    auto device = createDevice(backendType);

    size_t maxSamples;
    bool hasLimit = device->getFeatureLimits(DeviceFeatureLimits::MaxSamples, &maxSamples);

    EXPECT_TRUE(hasLimit)
        << "Backend " << toString(backendType) << " should support MaxSamples query";

    EXPECT_GE(maxSamples, 1) << "Backend " << toString(backendType);

    // Try creating MSAA texture with reported limit
    TextureDesc desc;
    desc.format = TextureFormat::RGBA_UNorm8;
    desc.width = 512;
    desc.height = 512;
    desc.numSamples = maxSamples;
    desc.usage = TextureDesc::TextureUsageBits::Attachment;

    Result result;
    auto texture = device->createTexture(desc, &result);

    EXPECT_TRUE(result.isOk())
        << "Backend " << toString(backendType)
        << " should support " << maxSamples << "x MSAA: " << result.message;
  }
}
```

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions should continue working (no MSAA changes)
./test_all_sessions.bat
```

**Expected Changes:**
- No behavior change for existing sessions
- Applications can now query MSAA support proactively

**Test Modifications Allowed:**
- Add D3D12 backend-specific MSAA tests
- DO NOT modify cross-platform test logic

---

## Definition of Done

### Completion Criteria:

- [ ] `DeviceFeatureLimits::MaxSamples` case added to D3D12 `getFeatureLimits()`
- [ ] `getMaxMSAASamplesForFormat()` helper method implemented (optional but recommended)
- [ ] All unit tests pass (including new MSAA query tests)
- [ ] All render sessions pass
- [ ] D3D12 backend reports accurate `MaxSamples` limit (4-16 depending on device)
- [ ] Format-specific MSAA queries work correctly
- [ ] Error messages updated with actionable guidance
- [ ] Cross-platform consistency: D3D12 matches Vulkan/Metal query behavior
- [ ] Documentation comments added

### User Confirmation Required:

**STOP - Do NOT proceed to next task until user confirms:**

1. All D3D12 device limit tests pass
2. Format-specific MSAA query tests pass
3. Cross-platform consistency tests pass
4. All render sessions continue working

**Post in chat:**
```
I-003 Fix Complete - Ready for Review
- Unit tests: PASS
- Render sessions: PASS
- D3D12 MaxSamples query: IMPLEMENTED
- Format-specific queries: IMPLEMENTED
- Cross-platform consistency: VALIDATED
- MSAA support for RGBA8: [X]x (report actual value)

Awaiting confirmation to proceed.
```

---

## Related Issues

### Depends On:
- None (independent fix)

### Blocks:
- Deferred rendering with MSAA
- Dynamic quality settings (MSAA auto-detection)
- Cross-platform MSAA applications

### Related:
- **I-001** (Vertex Attribute Limit Inconsistency)
- **I-004** (Texture Format Support Query Incomplete)
- **I-005** (Descriptor Heap Size Limits)

---

## Implementation Priority

**Priority:** P2 - MEDIUM (Portability Enhancement)
**Estimated Effort:** 2-3 hours
**Risk:** Low (well-defined D3D12 API, extensive validation possible)
**Impact:** MEDIUM - Improves cross-platform consistency, enables proactive MSAA queries

**Rationale:**
- Aligns D3D12 backend with Vulkan's capability query model
- Prevents trial-and-error texture creation
- Low-risk API addition with clear D3D12 documentation
- Improves developer experience and application robustness

---

## References

### D3D12:
- [Microsoft D3D12 CheckFeatureSupport](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-checkfeaturesupport)
- [D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_multisample_quality_levels)
- [DXGI_SAMPLE_DESC](https://learn.microsoft.com/en-us/windows/win32/api/dxgicommon/ns-dxgicommon-dxgi_sample_desc)

### Vulkan:
- [VkPhysicalDeviceLimits](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceLimits.html)
- [VkSampleCountFlagBits](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkSampleCountFlagBits.html)

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
