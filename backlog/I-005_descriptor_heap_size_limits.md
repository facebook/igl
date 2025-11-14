# I-005: Descriptor Heap Size Limits Differ from Vulkan

**Severity:** Medium
**Category:** Cross-Platform Portability & Resource Limits
**Status:** Open
**Related Issues:** C-005 (Sampler Heap Exhaustion), I-001 (Vertex Attribute Limits)

---

## Problem Statement

D3D12 backend uses **hardcoded descriptor heap sizes** in `DescriptorHeapManager` that are **significantly smaller** than D3D12 hardware limits and **inconsistent with Vulkan descriptor pool sizes**. This creates **portability issues** where applications running on Vulkan backends work fine but **hit descriptor exhaustion** on D3D12, despite D3D12 hardware supporting much larger heaps.

**The Impact:**
- **Sampler heap: 16 descriptors** vs D3D12 limit of 2048 (0.78% utilization!)
- **CBV/SRV/UAV heap: 256 descriptors** vs D3D12 limit of 1,000,000+ (0.0256% utilization!)
- **RTV heap: 64 descriptors** vs D3D12 limit of 16,384 (0.39% utilization!)
- **DSV heap: 32 descriptors** vs D3D12 limit of 16,384 (0.20% utilization!)
- Applications developed on Vulkan (larger descriptor pools) fail when ported to D3D12
- No exposed API to query or configure heap sizes at runtime
- Complex scenes with many materials/textures quickly exhaust heaps
- Cross-platform inconsistency forces lowest-common-denominator approach

**Real-World Scenario:**
```cpp
// Vulkan: Application uses 128 samplers for material system (works fine)
auto vkDevice = createDevice(BackendType::Vulkan);
// Vulkan descriptor pool allows 1000s of samplers

// D3D12: Same application ported
auto d3d12Device = createDevice(BackendType::D3D12);
// D3D12 DescriptorHeapManager hardcoded to 16 samplers!

// Material system creates 20 samplers
for (int i = 0; i < 20; i++) {
  SamplerStateDesc desc;
  desc.minFilter = SamplerMinMagFilter::Linear;
  Result result;
  auto sampler = d3d12Device->createSamplerState(desc, &result);
  // FAILS after 16th sampler: descriptor heap exhausted!
  // Error: "DescriptorHeapManager: Sampler heap exhausted!"
}

// Application crashes or fails to render - but Vulkan version works!
```

---

## Root Cause Analysis

### Current Implementation - Hardcoded Small Limits:

#### DescriptorHeapManager.h (Lines 20-24)
```cpp
struct Sizes {
  uint32_t cbvSrvUav = 256; // shader-visible
  uint32_t samplers = 16;    // shader-visible ← CRITICALLY SMALL!
  uint32_t rtvs = 64;        // CPU-visible
  uint32_t dsvs = 32;        // CPU-visible
};
```

**Problem:** These default sizes are **inadequate for production applications** and **orders of magnitude smaller** than D3D12 hardware supports.

#### DescriptorHeapManager.cpp (Exhaustion Logging - Lines 92-94, 112-114, 146-148)
```cpp
// RTV allocation
if (freeRtvs_.empty()) {
  IGL_LOG_ERROR("DescriptorHeapManager: RTV heap exhausted! "
                "Requested allocation failed (capacity: %u descriptors)\n",
                sizes_.rtvs);  // Fails at 64 descriptors
  return UINT32_MAX;
}

// DSV allocation
if (freeDsvs_.empty()) {
  IGL_LOG_ERROR("DescriptorHeapManager: DSV heap exhausted! "
                "Requested allocation failed (capacity: %u descriptors)\n",
                sizes_.dsvs);  // Fails at 32 descriptors
  return UINT32_MAX;
}

// CBV/SRV/UAV allocation
if (freeCbvSrvUav_.empty()) {
  IGL_LOG_ERROR("DescriptorHeapManager: CBV/SRV/UAV heap exhausted! "
                "Requested allocation failed (capacity: %u descriptors)\n",
                sizes_.cbvSrvUav);  // Fails at 256 descriptors
  return UINT32_MAX;
}
```

**Problem:** Exhaustion errors provide no guidance on reconfiguring heap sizes.

### D3D12 Hardware Limits vs Current Sizes:

| Heap Type       | Current Size | D3D12 Hardware Limit | Utilization | Gap       |
|----------------|--------------|---------------------|-------------|-----------|
| CBV/SRV/UAV    | 256          | 1,000,000+          | 0.0256%     | 3,906x    |
| Samplers       | 16           | 2,048               | 0.78%       | 128x      |
| RTVs           | 64           | 16,384              | 0.39%       | 256x      |
| DSVs           | 32           | 16,384              | 0.20%       | 512x      |

**Analysis:** IGL is using **less than 1%** of D3D12's descriptor heap capacity!

### Vulkan Comparison (Typical Descriptor Pool Sizes):

Vulkan applications typically create descriptor pools with:
- **Sampler descriptors**: 256-2048
- **Sampled image descriptors**: 1024-4096
- **Storage image descriptors**: 256-1024
- **Uniform buffers**: 512-2048

**Gap:** Vulkan pools are **16-256x larger** than D3D12 DescriptorHeapManager defaults!

### Why This Is Wrong:

1. **Artificial Limits**: D3D12 supports 2048 samplers, but IGL only allows 16
2. **Cross-Platform Inconsistency**: Vulkan apps work, D3D12 ports fail
3. **Lowest-Common-Denominator Forced**: All backends constrained by D3D12's conservative defaults
4. **No Runtime Configuration**: Heap sizes hardcoded, no API to adjust
5. **Production Inadequacy**: 16 samplers insufficient for modern material systems

---

## Official Documentation References

### D3D12 Documentation:

1. **Descriptor Heap Limits**:
   - [Microsoft D3D12 Descriptor Heaps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps)
   - Quote: "Applications can create multiple descriptor heaps. The size of each heap is configurable at creation time."

2. **D3D12_DESCRIPTOR_HEAP_TYPE Enumeration**:
   - [D3D12_DESCRIPTOR_HEAP_TYPE](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_descriptor_heap_type)
   - Maximum Descriptors per Heap Type:
     - `D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV`: 1,000,000+ (shader-visible)
     - `D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER`: 2,048 (shader-visible)
     - `D3D12_DESCRIPTOR_HEAP_TYPE_RTV`: 16,384 (non-shader-visible)
     - `D3D12_DESCRIPTOR_HEAP_TYPE_DSV`: 16,384 (non-shader-visible)

3. **Descriptor Heap Creation**:
   - [ID3D12Device::CreateDescriptorHeap](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createdescriptorheap)
   - Quote: "NumDescriptors: The number of descriptors in the heap."
   - Validation: "Must not exceed maximum for heap type."

4. **Best Practices**:
   - [D3D12 Best Practices - Descriptor Management](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps-overview#descriptor-heap-properties)
   - Quote: "Allocate descriptor heaps sized appropriately for your application's needs."

### Vulkan Documentation:

1. **Descriptor Pool Sizes**:
   - [vkCreateDescriptorPool](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCreateDescriptorPool.html)
   - [VkDescriptorPoolCreateInfo](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkDescriptorPoolCreateInfo.html)
   - Quote: "Applications specify the maximum number of descriptor sets and descriptors of each type when creating a descriptor pool."

2. **Typical Pool Sizes**:
   - [Vulkan Best Practices - Descriptor Management](https://github.com/KhronosGroup/Vulkan-Guide/blob/main/chapters/descriptor_management.adoc)
   - Recommendation: "Size descriptor pools based on application usage patterns, not hardware minimums."

---

## Code Location Strategy

### Files to Modify:

1. **DescriptorHeapManager.h** (Increase default sizes):
   - Location: `src/igl/d3d12/DescriptorHeapManager.h:20-24`
   - Search for: `struct Sizes {`
   - Context: Default descriptor heap sizes struct
   - Action: Increase default sizes to more reasonable production values

2. **DescriptorHeapManager.cpp** (Add exhaustion guidance):
   - Location: `src/igl/d3d12/DescriptorHeapManager.cpp:92-94, 112-114, 146-148`
   - Search for: `IGL_LOG_ERROR.*heap exhausted`
   - Context: Heap exhaustion error logging
   - Action: Add guidance on reconfiguring heap sizes

3. **D3D12Context.h** (Expose heap size configuration):
   - Location: `src/igl/d3d12/D3D12Context.h:23-33` (DescriptorHeapPage struct area)
   - Context: Frame context descriptor heap initialization
   - Action: Consider exposing DescriptorHeapManager::Sizes to initialization API

4. **Device.cpp** (Constructor - Line 33):
   - Location: `src/igl/d3d12/Device.cpp:33`
   - Search for: `Device::Device(std::unique_ptr<D3D12Context> ctx)`
   - Context: Device initialization
   - Action: Consider allowing descriptor heap size configuration

### Files to Review (No Changes Expected):

1. **Common.h** (`src/igl/Common.h`)
   - Check if any global descriptor constants exist

---

## Detection Strategy

### How to Reproduce the Exhaustion:

#### Test 1: Exhaust Sampler Heap (Current: 16 descriptors)
```cpp
auto device = createDevice(BackendType::D3D12);

// Create 20 samplers (exceeds current limit of 16)
std::vector<std::shared_ptr<ISamplerState>> samplers;

for (int i = 0; i < 20; i++) {
  SamplerStateDesc desc;
  desc.minFilter = SamplerMinMagFilter::Linear;
  desc.magFilter = SamplerMinMagFilter::Linear;

  Result result;
  auto sampler = device->createSamplerState(desc, &result);

  if (!result.isOk()) {
    std::cout << "Failed at sampler " << i << ": " << result.message << "\n";
    // CURRENT: Fails at sampler 16: "Sampler heap exhausted!"
    break;
  }

  samplers.push_back(sampler);
}

EXPECT_EQ(samplers.size(), 20) << "Should support at least 20 samplers";
```

#### Test 2: Exhaust CBV/SRV/UAV Heap (Current: 256 descriptors)
```cpp
// Create 300 textures (exceeds current limit of 256 CBV/SRV/UAV descriptors)
std::vector<std::shared_ptr<ITexture>> textures;

for (int i = 0; i < 300; i++) {
  TextureDesc desc;
  desc.format = TextureFormat::RGBA_UNorm8;
  desc.width = 64;
  desc.height = 64;
  desc.usage = TextureDesc::TextureUsageBits::Sampled;

  Result result;
  auto texture = device->createTexture(desc, &result);

  if (!result.isOk()) {
    std::cout << "Failed at texture " << i << ": " << result.message << "\n";
    // CURRENT: Fails around texture 256 (if each texture uses 1 SRV)
    break;
  }

  textures.push_back(texture);
}

EXPECT_EQ(textures.size(), 300) << "Should support at least 300 textures";
```

#### Test 3: Compare Vulkan vs D3D12 Capacity
```cpp
auto backends = {BackendType::Vulkan, BackendType::D3D12};

for (auto backendType : backends) {
  auto device = createDevice(backendType);

  // Count how many samplers can be created before exhaustion
  int samplerCount = 0;
  for (int i = 0; i < 2048; i++) {  // Test up to D3D12 max
    SamplerStateDesc desc;
    Result result;
    auto sampler = device->createSamplerState(desc, &result);

    if (!result.isOk()) {
      break;
    }
    samplerCount++;
  }

  std::cout << toString(backendType) << " sampler capacity: " << samplerCount << "\n";
  // CURRENT OUTPUT:
  //   Vulkan sampler capacity: 2048 (or more)
  //   D3D12 sampler capacity: 16
}
```

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Increase Default Descriptor Heap Sizes

**File:** `src/igl/d3d12/DescriptorHeapManager.h`

**Locate:** Line 20-24

**Before:**
```cpp
struct Sizes {
  uint32_t cbvSrvUav = 256; // shader-visible
  uint32_t samplers = 16;    // shader-visible
  uint32_t rtvs = 64;        // CPU-visible
  uint32_t dsvs = 32;        // CPU-visible
};
```

**After:**
```cpp
struct Sizes {
  // Increased to reasonable production values aligned with D3D12 capabilities
  // and Vulkan typical descriptor pool sizes
  //
  // D3D12 Hardware Limits:
  //   CBV/SRV/UAV: 1,000,000+ (shader-visible)
  //   Samplers: 2,048 (shader-visible)
  //   RTVs: 16,384 (CPU-only)
  //   DSVs: 16,384 (CPU-only)
  //
  // Vulkan Typical Pool Sizes:
  //   Sampled images: 1024-4096
  //   Samplers: 256-2048
  //   Storage images: 256-1024
  //   Uniform buffers: 512-2048
  //
  // Chosen sizes balance memory usage with practical application needs:
  uint32_t cbvSrvUav = 4096;  // 16x increase: supports 4K textures/buffers (shader-visible)
  uint32_t samplers = 256;     // 16x increase: supports 256 material samplers (shader-visible)
  uint32_t rtvs = 256;         // 4x increase: supports complex MRT scenarios (CPU-visible)
  uint32_t dsvs = 128;         // 4x increase: sufficient for depth/stencil (CPU-visible)
};
```

**Rationale:**
- **CBV/SRV/UAV: 256 → 4096**: Supports modern scenes with thousands of textures/buffers
- **Samplers: 16 → 256**: Allows rich material systems with many sampler configurations
- **RTVs: 64 → 256**: Supports complex multi-render-target scenarios
- **DSVs: 32 → 128**: Ample for depth/stencil attachments
- Still far below D3D12 maximums, leaving room for future growth
- Aligns with typical Vulkan descriptor pool sizing

**Memory Impact:**
- Each descriptor is ~32-48 bytes
- Total increase: ~180 KB per DescriptorHeapManager instance
- Negligible on modern hardware

---

#### Step 2: Add Configuration Parameter to D3D12Context (Optional)

**File:** `src/igl/d3d12/D3D12Context.h`

**Add after line 78:**
```cpp
class D3D12Context {
 public:
  // Configuration for descriptor heap sizes
  // Applications can override defaults via setDescriptorHeapSizes() before initialize()
  struct DescriptorHeapSizeConfig {
    uint32_t cbvSrvUav = 4096;  // Default from DescriptorHeapManager::Sizes
    uint32_t samplers = 256;
    uint32_t rtvs = 256;
    uint32_t dsvs = 128;
  };

  D3D12Context() = default;
  ~D3D12Context();

  // Set descriptor heap sizes before initialize()
  // Must be called before initialize() to take effect
  void setDescriptorHeapSizeConfig(const DescriptorHeapSizeConfig& config) {
    heapSizeConfig_ = config;
  }

  Result initialize(HWND hwnd, uint32_t width, uint32_t height);
  // ... rest of methods ...

 private:
  DescriptorHeapSizeConfig heapSizeConfig_;  // Applied during initialize()
  // ... rest of members ...
};
```

**File:** `src/igl/d3d12/D3D12Context.cpp`

**Update `initialize()` method to use config:**
```cpp
Result D3D12Context::initialize(HWND hwnd, uint32_t width, uint32_t height) {
  // ... existing initialization code ...

  // Initialize descriptor heap manager with configured sizes
  DescriptorHeapManager::Sizes heapSizes;
  heapSizes.cbvSrvUav = heapSizeConfig_.cbvSrvUav;
  heapSizes.samplers = heapSizeConfig_.samplers;
  heapSizes.rtvs = heapSizeConfig_.rtvs;
  heapSizes.dsvs = heapSizeConfig_.dsvs;

  descriptorHeapManager_ = std::make_unique<DescriptorHeapManager>();
  Result result = descriptorHeapManager_->initialize(device_.Get(), heapSizes);
  if (!result.isOk()) {
    return result;
  }

  IGL_LOG_INFO("D3D12Context: Descriptor heap manager initialized\n");
  IGL_LOG_INFO("  CBV/SRV/UAV: %u descriptors\n", heapSizes.cbvSrvUav);
  IGL_LOG_INFO("  Samplers: %u descriptors\n", heapSizes.samplers);
  IGL_LOG_INFO("  RTVs: %u descriptors\n", heapSizes.rtvs);
  IGL_LOG_INFO("  DSVs: %u descriptors\n", heapSizes.dsvs);

  // ... rest of initialization ...
}
```

**Rationale:** Allows advanced applications to customize heap sizes if needed

---

#### Step 3: Enhance Exhaustion Error Messages

**File:** `src/igl/d3d12/DescriptorHeapManager.cpp`

**Locate:** Line 92-96 (RTV exhaustion)

**Before:**
```cpp
if (freeRtvs_.empty()) {
  IGL_LOG_ERROR("DescriptorHeapManager: RTV heap exhausted! "
                "Requested allocation failed (capacity: %u descriptors)\n",
                sizes_.rtvs);
  return UINT32_MAX;
}
```

**After:**
```cpp
if (freeRtvs_.empty()) {
  IGL_LOG_ERROR("DescriptorHeapManager: RTV heap exhausted!\n");
  IGL_LOG_ERROR("  Requested allocation failed (capacity: %u descriptors)\n", sizes_.rtvs);
  IGL_LOG_ERROR("  Current usage: %u descriptors allocated\n",
                sizes_.rtvs - static_cast<uint32_t>(freeRtvs_.size()));
  IGL_LOG_ERROR("  High-water mark: %u descriptors\n", highWaterMarkRtvs_);
  IGL_LOG_ERROR("  D3D12 hardware limit: 16,384 descriptors\n");
  IGL_LOG_ERROR("  Solution: Increase DescriptorHeapManager::Sizes::rtvs or free unused RTVs\n");
  return UINT32_MAX;
}
```

**Apply similar changes to:**
- DSV exhaustion (line 112)
- CBV/SRV/UAV exhaustion (line 146)
- Sampler exhaustion (add similar logging if not present)

**Rationale:** Provides actionable guidance to developers encountering exhaustion

---

#### Step 4: Add Heap Usage Telemetry Logging

**File:** `src/igl/d3d12/DescriptorHeapManager.cpp`

**Add new public method:**
```cpp
void DescriptorHeapManager::logUsageStats() const {
  std::lock_guard<std::mutex> lock(mutex_);

  const uint32_t cbvSrvUavUsed = sizes_.cbvSrvUav - static_cast<uint32_t>(freeCbvSrvUav_.size());
  const uint32_t samplersUsed = sizes_.samplers - static_cast<uint32_t>(freeSamplers_.size());
  const uint32_t rtvsUsed = sizes_.rtvs - static_cast<uint32_t>(freeRtvs_.size());
  const uint32_t dsvsUsed = sizes_.dsvs - static_cast<uint32_t>(freeDsvs_.size());

  IGL_LOG_INFO("=== DescriptorHeapManager Usage Statistics ===\n");
  IGL_LOG_INFO("  CBV/SRV/UAV: %u / %u (%.1f%%) [High-water: %u]\n",
               cbvSrvUavUsed, sizes_.cbvSrvUav,
               100.0f * cbvSrvUavUsed / sizes_.cbvSrvUav,
               highWaterMarkCbvSrvUav_);
  IGL_LOG_INFO("  Samplers: %u / %u (%.1f%%) [High-water: %u]\n",
               samplersUsed, sizes_.samplers,
               100.0f * samplersUsed / sizes_.samplers,
               highWaterMarkSamplers_);
  IGL_LOG_INFO("  RTVs: %u / %u (%.1f%%) [High-water: %u]\n",
               rtvsUsed, sizes_.rtvs,
               100.0f * rtvsUsed / sizes_.rtvs,
               highWaterMarkRtvs_);
  IGL_LOG_INFO("  DSVs: %u / %u (%.1f%%) [High-water: %u]\n",
               dsvsUsed, sizes_.dsvs,
               100.0f * dsvsUsed / sizes_.dsvs,
               highWaterMarkDsvs_);
}
```

**Rationale:** Allows runtime monitoring of heap usage for optimization

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 descriptor heap tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Descriptor*"

# Run resource creation tests (samplers, textures)
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Sampler*"
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Texture*Create*"

# Run all render sessions
./test_all_sessions.bat
```

**Expected Results:**
- All existing tests pass with larger heap sizes
- No descriptor exhaustion errors
- Memory usage increased by ~180 KB (negligible)

### New Test Cases Required:

#### Test 1: Increased Sampler Capacity

**File:** `src/igl/tests/d3d12/DescriptorHeap.cpp`

```cpp
TEST_F(DescriptorHeapD3D12Test, IncreasedSamplerCapacity) {
  // Create 256 samplers (new limit, was 16)
  std::vector<std::shared_ptr<ISamplerState>> samplers;

  for (int i = 0; i < 256; i++) {
    SamplerStateDesc desc;
    desc.minFilter = static_cast<SamplerMinMagFilter>(i % 2);
    desc.magFilter = static_cast<SamplerMinMagFilter>(i % 2);

    Result result;
    auto sampler = device_->createSamplerState(desc, &result);

    ASSERT_TRUE(result.isOk()) << "Failed at sampler " << i << ": " << result.message;
    samplers.push_back(sampler);
  }

  EXPECT_EQ(samplers.size(), 256);
}
```

#### Test 2: Increased Texture Capacity

```cpp
TEST_F(DescriptorHeapD3D12Test, IncreasedTextureCapacity) {
  // Create 512 textures (well within new 4096 CBV/SRV/UAV limit)
  std::vector<std::shared_ptr<ITexture>> textures;

  for (int i = 0; i < 512; i++) {
    TextureDesc desc;
    desc.format = TextureFormat::RGBA_UNorm8;
    desc.width = 64;
    desc.height = 64;
    desc.usage = TextureDesc::TextureUsageBits::Sampled;

    Result result;
    auto texture = device_->createTexture(desc, &result);

    ASSERT_TRUE(result.isOk()) << "Failed at texture " << i << ": " << result.message;
    textures.push_back(texture);
  }

  EXPECT_EQ(textures.size(), 512);
}
```

#### Test 3: Heap Usage Telemetry

```cpp
TEST_F(DescriptorHeapD3D12Test, HeapUsageTelemetry) {
  auto d3d12Device = static_cast<igl::d3d12::Device*>(device_.get());
  auto* heapManager = d3d12Device->getContext()->getDescriptorHeapManager();

  // Create some resources
  for (int i = 0; i < 50; i++) {
    SamplerStateDesc desc;
    device_->createSamplerState(desc, nullptr);
  }

  // Log usage statistics (should show 50 samplers used out of 256)
  heapManager->logUsageStats();
  // Validate output manually or capture log and parse
}
```

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions should continue working with larger heaps
./test_all_sessions.bat
```

**Expected Changes:**
- No behavior change for existing sessions
- No descriptor exhaustion errors
- Slightly increased memory usage (~180 KB)

**Test Modifications Allowed:**
- Add D3D12 backend-specific descriptor capacity tests
- DO NOT modify cross-platform test logic

---

## Definition of Done

### Completion Criteria:

- [ ] `DescriptorHeapManager::Sizes` defaults increased to production-appropriate values
- [ ] All unit tests pass with new heap sizes
- [ ] All render sessions pass
- [ ] Descriptor exhaustion error messages enhanced with actionable guidance
- [ ] Heap usage telemetry method added (`logUsageStats()`)
- [ ] Optional: Runtime configuration API added for custom heap sizes
- [ ] D3D12 backend now supports 256 samplers (was 16)
- [ ] D3D12 backend now supports 4096 CBV/SRV/UAV descriptors (was 256)
- [ ] Cross-platform portability improved (closer to Vulkan capacity)
- [ ] Documentation comments updated

### User Confirmation Required:

**STOP - Do NOT proceed to next task until user confirms:**

1. All D3D12 descriptor heap tests pass
2. Increased capacity tests pass (256 samplers, 512 textures)
3. All render sessions continue working
4. No unexpected memory issues

**Post in chat:**
```
I-005 Fix Complete - Ready for Review
- Unit tests: PASS
- Render sessions: PASS
- Sampler heap: 16 → 256 (16x increase)
- CBV/SRV/UAV heap: 256 → 4096 (16x increase)
- RTV heap: 64 → 256 (4x increase)
- DSV heap: 32 → 128 (4x increase)
- Memory overhead: ~180 KB (negligible)
- Cross-platform consistency: IMPROVED

Awaiting confirmation to proceed.
```

---

## Related Issues

### Depends On:
- None (independent fix)

### Blocks:
- **C-005** (Sampler Heap Exhaustion) - Partially resolved by increasing sampler heap
- Material systems with many samplers
- Complex scenes with many textures

### Related:
- **I-001** (Vertex Attribute Limit Inconsistency)
- **I-003** (MSAA Sample Count Limits)
- **I-004** (Texture Format Support Query)

---

## Implementation Priority

**Priority:** P2 - MEDIUM (Resource Limit Enhancement)
**Estimated Effort:** 2-3 hours
**Risk:** LOW (well-tested change, extensive validation possible)
**Impact:** HIGH - Eliminates descriptor exhaustion for typical applications, improves cross-platform portability

**Rationale:**
- Simple constant change with massive impact
- Aligns D3D12 backend with Vulkan typical usage
- Negligible memory overhead (~180 KB)
- Low risk: all existing code continues to work
- Unblocks production applications with complex material systems

---

## References

### D3D12:
- [Microsoft D3D12 Descriptor Heaps](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps)
- [D3D12_DESCRIPTOR_HEAP_TYPE](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_descriptor_heap_type)
- [ID3D12Device::CreateDescriptorHeap](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createdescriptorheap)
- [D3D12 Best Practices - Descriptor Management](https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps-overview#descriptor-heap-properties)

### Vulkan:
- [vkCreateDescriptorPool](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCreateDescriptorPool.html)
- [VkDescriptorPoolCreateInfo](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkDescriptorPoolCreateInfo.html)
- [Vulkan Best Practices - Descriptor Management](https://github.com/KhronosGroup/Vulkan-Guide/blob/main/chapters/descriptor_management.adoc)

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
