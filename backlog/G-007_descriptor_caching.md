# G-007: Descriptor Creation Not Cached by State Hash

**Severity:** Low
**Category:** GPU Performance
**Status:** Open
**Related Issues:** G-003 (Descriptor Fragmentation), C-005 (Sampler Exhaustion)
**Priority:** P2-Low

---

## Problem Statement

IGL's descriptor allocation and creation process (especially for samplers, constant buffer views, and unordered access views) creates new descriptors for each resource binding even when the descriptor state is identical to a previously created descriptor. For example, if two textures need the same sampler state, separate samplers are allocated instead of reusing the existing one. A caching layer based on state hash could eliminate redundant descriptor creation.

**Impact Analysis:**
- **Descriptor Heap Exhaustion:** Redundant descriptors consume heap space unnecessarily (e.g., 100 identical samplers)
- **Creation Overhead:** Each descriptor creation has CPU cost; caching eliminates this cost for duplicates
- **Sampler Heap Pressure:** Sampler heaps are typically small (2048 samplers); redundant samplers cause premature exhaustion
- **Shader State Variations:** Applications often use same sampler across many textures; each binding creates new descriptor

**The Limitation:**
- Descriptor caching requires comparing state structures and computing hashes
- Cache invalidation must be handled carefully (resource lifetime)
- Not applicable to dynamic descriptors that change per-frame
- Most beneficial for static/persistent sampler and view states

---

## Root Cause Analysis

### Current Implementation (SamplerState.cpp):

```cpp
std::shared_ptr<ISamplerState> Device::createSamplerState(
    const SamplerStateDesc& desc) {
  // Every sampler state creation allocates new descriptor
  uint32_t samplerIndex = descriptorHeapMgr_->allocateSampler();

  if (samplerIndex == UINT32_MAX) {
    IGL_LOG_ERROR("Device::createSamplerState: Sampler heap exhausted\n");
    return nullptr;
  }

  // Create sampler descriptor
  D3D12_SAMPLER_DESC samplerDesc = {};
  // ... fill sampler descriptor from desc ...

  device_->CreateSampler(&samplerDesc, heapHandle);

  // No caching check - always creates new descriptor!
  return std::make_shared<SamplerState>(samplerIndex, desc);
}
```

### Why This Is Wrong:

1. **No Deduplication:** Identical sampler states create separate descriptors
2. **No Hash Caching:** No lookup table to find existing sampler with same state
3. **Heap Pressure:** Redundant samplers consume scarce sampler heap space
4. **Creation Overhead:** CPU cost per descriptor creation not amortized across duplicates

**Real-World Example:**
```
Material 1: Texture (LINEAR sampler)
Material 2: Texture (LINEAR sampler) <- Allocates duplicate sampler!
Material 3: Texture (LINEAR sampler) <- Another duplicate!
...
Material 100: Texture (LINEAR sampler) <- 100 identical samplers created!

Before fix: 100 sampler heap slots used for identical state
After fix: 1 sampler heap slot used, cached for all 100 uses
```

---

## Official Documentation References

1. **D3D12 Descriptor Best Practices**:
   - https://learn.microsoft.com/windows/win32/direct3d12/descriptors-best-practices
   - Key guidance: "Reuse identical descriptors; avoid redundant allocations"

2. **Sampler State Management**:
   - https://learn.microsoft.com/windows/win32/direct3d12/sampler-feedback-texture
   - Recommendations: "Cache sampler state to reduce heap pressure"

3. **Hash-Based Caching Pattern**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/search?q=cache
   - Sample patterns for state caching

4. **Performance Optimization**:
   - https://learn.microsoft.com/windows/win32/direct3d12/memory-management-strategies
   - Guidance: "Use caching to reduce descriptor allocation overhead"

---

## Code Location Strategy

### Files to Modify:

1. **Device.h** (add descriptor cache):
   - Search for: Private member variables
   - Context: Add cache data structures
   - Action: Add hash maps for caching

2. **Device.cpp** (implement descriptor cache):
   - Search for: createSamplerState method
   - Context: Sampler creation
   - Action: Check cache before creating new sampler

3. **Device.cpp** (implement cache key generation):
   - Search for: End of implementation
   - Context: Add hash/comparison functions
   - Action: Implement SamplerStateDesc hashing

4. **Device.cpp** (cache for texture views):
   - Search for: createTextureView or equivalent
   - Context: Texture descriptor creation
   - Action: Cache identical texture view descriptors

---

## Detection Strategy

### How to Reproduce:

```cpp
// Create 100 materials with same sampler
std::vector<std::shared_ptr<ISamplerState>> samplers;
std::vector<std::shared_ptr<ITexture>> textures;

SamplerStateDesc linearSamplerDesc;
linearSamplerDesc.filterMode = FilterMode::Linear;
linearSamplerDesc.addressModeU = AddressMode::Repeat;
linearSamplerDesc.addressModeV = AddressMode::Repeat;
linearSamplerDesc.addressModeW = AddressMode::Repeat;

// Before fix: Allocates 100 identical samplers
for (int i = 0; i < 100; ++i) {
  auto sampler = device->createSamplerState(linearSamplerDesc);
  samplers.push_back(sampler);

  auto texture = device->createTexture(textureDesc, nullptr);
  texture->bindSampler(0, sampler);
  textures.push_back(texture);
}

// Check sampler descriptor allocation
// Before: 100 sampler descriptors allocated
// After: 1 sampler descriptor allocated, referenced 100 times
```

### Expected Behavior (After Fix):

- Only 1 sampler descriptor allocated for 100 identical sampler states
- Sampler heap space reduced by ~99%
- No visual changes or behavior differences

### Verification After Fix:

1. Monitor sampler descriptor count
2. Verify identical sampler states use same descriptor
3. Benchmark sampler creation time (should show cache hits)

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Add Descriptor Cache Infrastructure

**File:** `src/igl/d3d12/Device.h`

**Locate:** Private member variables section

**Add Cache Structures:**
```cpp
class Device : public IDevice {
 private:
  // G-007: Descriptor caching by state hash
  struct SamplerStateHash {
    size_t operator()(const SamplerStateDesc& desc) const {
      // Hash sampler state fields
      size_t h = 0;
      h ^= std::hash<uint32_t>()(static_cast<uint32_t>(desc.filterMode)) << 1;
      h ^= std::hash<uint32_t>()(static_cast<uint32_t>(desc.addressModeU)) << 2;
      h ^= std::hash<uint32_t>()(static_cast<uint32_t>(desc.addressModeV)) << 3;
      h ^= std::hash<uint32_t>()(static_cast<uint32_t>(desc.addressModeW)) << 4;
      h ^= std::hash<float>()(desc.lodMinClamp) << 5;
      h ^= std::hash<float>()(desc.lodMaxClamp) << 6;
      h ^= std::hash<float>()(desc.lodBias) << 7;
      h ^= std::hash<uint32_t>()(desc.maxAnisotropy) << 8;
      return h;
    }
  };

  struct SamplerStateEqual {
    bool operator()(const SamplerStateDesc& a, const SamplerStateDesc& b) const {
      return a.filterMode == b.filterMode &&
             a.addressModeU == b.addressModeU &&
             a.addressModeV == b.addressModeV &&
             a.addressModeW == b.addressModeW &&
             a.lodMinClamp == b.lodMinClamp &&
             a.lodMaxClamp == b.lodMaxClamp &&
             a.lodBias == b.lodBias &&
             a.maxAnisotropy == b.maxAnisotropy;
    }
  };

  // Sampler descriptor cache: State -> Descriptor Index
  using SamplerCache = std::unordered_map<SamplerStateDesc, uint32_t,
                                           SamplerStateHash, SamplerStateEqual>;
  SamplerCache samplerCache_;
  std::mutex samplerCacheMutex_;

  // Reference counting for cached samplers
  std::unordered_map<uint32_t, size_t> samplerRefCount_;

  // ... other members ...
};
```

**Rationale:** Cache maps sampler state to allocated descriptor index; reference counting tracks usage.

---

#### Step 2: Implement Sampler State Hashing

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** Top of file, after includes

**Add Hash Implementation:**
```cpp
// G-007: Hash function for sampler state descriptors
template <>
struct std::hash<SamplerStateDesc> {
  size_t operator()(const SamplerStateDesc& desc) const {
    // Combine all state fields into single hash
    size_t h = 0;

    // Hash filter mode (3 bits)
    h ^= std::hash<int>()(static_cast<int>(desc.filterMode)) << 0;

    // Hash address modes (6 bits)
    h ^= std::hash<int>()(static_cast<int>(desc.addressModeU)) << 2;
    h ^= std::hash<int>()(static_cast<int>(desc.addressModeV)) << 4;
    h ^= std::hash<int>()(static_cast<int>(desc.addressModeW)) << 6;

    // Hash LOD values (floating point)
    h ^= std::hash<float>()(desc.lodMinClamp) << 8;
    h ^= std::hash<float>()(desc.lodMaxClamp) << 16;
    h ^= std::hash<float>()(desc.lodBias) << 24;

    // Hash anisotropy
    h ^= std::hash<uint32_t>()(desc.maxAnisotropy) << 28;

    return h;
  }
};
```

**Rationale:** Combines all relevant sampler fields into single hash for cache lookup.

---

#### Step 3: Modify createSamplerState to Use Cache

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** `createSamplerState()` method

**Current (PROBLEM):**
```cpp
std::shared_ptr<ISamplerState> Device::createSamplerState(
    const SamplerStateDesc& desc) {
  // Allocate new descriptor every time
  uint32_t samplerIndex = descriptorHeapMgr_->allocateSampler();
  if (samplerIndex == UINT32_MAX) {
    return nullptr;
  }

  // Create descriptor and return
  return std::make_shared<SamplerState>(samplerIndex, desc);
}
```

**Fixed (SOLUTION):**
```cpp
std::shared_ptr<ISamplerState> Device::createSamplerState(
    const SamplerStateDesc& desc) {
  // G-007: Check cache for identical sampler state
  {
    std::lock_guard<std::mutex> lock(samplerCacheMutex_);

    // Try to find cached sampler with identical state
    auto it = samplerCache_.find(desc);
    if (it != samplerCache_.end()) {
      // Cache hit! Increment reference count
      uint32_t cachedIndex = it->second;
      ++samplerRefCount_[cachedIndex];

      IGL_LOG_INFO("Device::createSamplerState: Cache hit (index=%u, refcount=%zu)\n",
                   cachedIndex, samplerRefCount_[cachedIndex]);

      return std::make_shared<SamplerState>(cachedIndex, desc);
    }
  }

  // Cache miss - allocate new sampler
  uint32_t samplerIndex = descriptorHeapMgr_->allocateSampler();
  if (samplerIndex == UINT32_MAX) {
    IGL_LOG_ERROR("Device::createSamplerState: Sampler heap exhausted\n");
    return nullptr;
  }

  // Create sampler descriptor
  D3D12_SAMPLER_DESC d3d12SamplerDesc = convertToD3D12SamplerDesc(desc);
  D3D12_CPU_DESCRIPTOR_HANDLE heapHandle =
      descriptorHeapMgr_->getSamplerHeapHandle(samplerIndex);

  device_->CreateSampler(&d3d12SamplerDesc, heapHandle);

  // Cache the new sampler
  {
    std::lock_guard<std::mutex> lock(samplerCacheMutex_);
    samplerCache_[desc] = samplerIndex;
    samplerRefCount_[samplerIndex] = 1;
  }

  IGL_LOG_INFO("Device::createSamplerState: Created new sampler (index=%u)\n", samplerIndex);

  return std::make_shared<SamplerState>(samplerIndex, desc);
}
```

**Rationale:** Check cache first; allocate only if state is new. Track reference counts for cleanup.

---

#### Step 4: Add Cache Cleanup on Sampler Destruction

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** `releaseSamplerState()` method (or add if doesn't exist)

**Add Implementation:**
```cpp
void Device::releaseSamplerState(ISamplerState* samplerState) {
  if (!samplerState) {
    return;
  }

  auto* d3d12Sampler = static_cast<SamplerState*>(samplerState);
  uint32_t samplerIndex = d3d12Sampler->getDescriptorIndex();

  // G-007: Decrement reference count and potentially clean cache
  {
    std::lock_guard<std::mutex> lock(samplerCacheMutex_);

    auto refIt = samplerRefCount_.find(samplerIndex);
    if (refIt != samplerRefCount_.end()) {
      --refIt->second;

      if (refIt->second == 0) {
        // Last reference removed - clean up
        IGL_LOG_INFO("Device::releaseSamplerState: Cleaning cached sampler (index=%u)\n",
                     samplerIndex);

        // Remove from cache
        for (auto it = samplerCache_.begin(); it != samplerCache_.end(); ++it) {
          if (it->second == samplerIndex) {
            samplerCache_.erase(it);
            break;
          }
        }

        samplerRefCount_.erase(refIt);

        // Free descriptor from heap
        descriptorHeapMgr_->freeSampler(samplerIndex);
      }
    }
  }
}
```

**Rationale:** Reference counting allows cache cleanup when last user is destroyed.

---

#### Step 5: Add Cache Statistics

**File:** `src/igl/d3d12/Device.cpp`

**Add Statistics Method:**
```cpp
void Device::logDescriptorCacheStats() {
  // G-007: Log cache hit rates and efficiency
  std::lock_guard<std::mutex> lock(samplerCacheMutex_);

  IGL_LOG_INFO("=== Descriptor Cache Statistics ===\n");
  IGL_LOG_INFO("Sampler Cache:\n");
  IGL_LOG_INFO("  Cached States: %zu\n", samplerCache_.size());

  size_t totalRefCount = 0;
  for (const auto& [idx, refCount] : samplerRefCount_) {
    totalRefCount += refCount;
  }

  IGL_LOG_INFO("  Total References: %zu\n", totalRefCount);

  if (samplerCache_.size() > 0) {
    float avgRefCount = static_cast<float>(totalRefCount) / samplerCache_.size();
    float spaceSavings = (1.0f - 1.0f / avgRefCount) * 100.0f;
    IGL_LOG_INFO("  Average References per Cached State: %.2f\n", avgRefCount);
    IGL_LOG_INFO("  Space Savings: %.1f%%\n", spaceSavings);
  }

  IGL_LOG_INFO("====================================\n");
}
```

**Rationale:** Provides visibility into cache effectiveness.

---

#### Step 6: Optional - Cache Texture View Descriptors

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** Texture view creation method

**Add Caching (Similar Pattern):**
```cpp
// G-007: Cache texture views by view descriptor state
using TextureViewCache = std::unordered_map<TextureViewDesc, uint32_t,
                                             TextureViewDescHash, TextureViewDescEqual>;
TextureViewCache textureViewCache_;
std::mutex textureViewCacheMutex_;

std::shared_ptr<ITextureView> Device::createTextureView(
    const std::shared_ptr<ITexture>& texture,
    const TextureViewDesc& desc) {
  // Check cache
  {
    std::lock_guard<std::mutex> lock(textureViewCacheMutex_);
    auto it = textureViewCache_.find(desc);
    if (it != textureViewCache_.end()) {
      // Cache hit
      return std::make_shared<TextureView>(it->second, desc);
    }
  }

  // Cache miss - create new view
  uint32_t viewIndex = descriptorHeapMgr_->allocateCbvSrvUav();
  // ... create view descriptor ...

  // Cache result
  {
    std::lock_guard<std::mutex> lock(textureViewCacheMutex_);
    textureViewCache_[desc] = viewIndex;
  }

  return std::make_shared<TextureView>(viewIndex, desc);
}
```

**Rationale:** Same caching principle applies to texture views and other descriptor types.

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Sampler-specific tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Sampler*"
```

**Test Modifications Allowed:**
- ✅ Add tests for descriptor caching
- ✅ Add tests for cache hit rates
- ❌ **DO NOT modify existing test logic**

**New Tests to Add:**
```cpp
// Test sampler caching works
TEST(Device, SamplerCaching) {
  auto device = createTestDevice();

  // Create two samplers with identical state
  SamplerStateDesc desc1 = createLinearSamplerDesc();
  SamplerStateDesc desc2 = createLinearSamplerDesc();

  auto sampler1 = device->createSamplerState(desc1);
  auto sampler2 = device->createSamplerState(desc2);

  EXPECT_NE(nullptr, sampler1);
  EXPECT_NE(nullptr, sampler2);

  // They should reference the same descriptor (cache hit)
  // Note: May need to expose descriptor index getter for verification
  // Or verify through heap usage stats
}

// Test cache hit rate with many identical samplers
TEST(Device, SamplerCacheHitRate) {
  auto device = createTestDevice();

  SamplerStateDesc linearDesc = createLinearSamplerDesc();

  // Create 100 samplers with identical state
  std::vector<std::shared_ptr<ISamplerState>> samplers;
  for (int i = 0; i < 100; ++i) {
    auto sampler = device->createSamplerState(linearDesc);
    EXPECT_NE(nullptr, sampler);
    samplers.push_back(sampler);
  }

  // All should succeed
  EXPECT_EQ(100, samplers.size());

  // Cache stats should show:
  // - Only 1 cached sampler state
  // - 100 references to that state
  device->logDescriptorCacheStats();
}

// Test cache doesn't reuse different states
TEST(Device, SamplerCacheDifferentStates) {
  auto device = createTestDevice();

  auto linear = device->createSamplerState(createLinearSamplerDesc());
  auto point = device->createSamplerState(createPointSamplerDesc());
  auto anisotropic = device->createSamplerState(createAnisotropicSamplerDesc());

  EXPECT_NE(nullptr, linear);
  EXPECT_NE(nullptr, point);
  EXPECT_NE(nullptr, anisotropic);

  // Cache stats should show 3 different cached states
  device->logDescriptorCacheStats();
}
```

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions must pass (caching should be transparent)
./build/Debug/RenderSessions.exe --session=TQSession
./build/Debug/RenderSessions.exe --session=HelloWorld
./build/Debug/RenderSessions.exe --session=Textured3DCube
```

**Modifications Allowed:**
- ✅ None required - caching is transparent
- ❌ **DO NOT modify session logic**

### Cache Effectiveness Verification:

```bash
# Create test application that creates 1000 identical samplers
# Before fix: Uses 1000 sampler heap slots
# After fix: Uses 1 sampler heap slot with 1000 references
# Expected savings: ~99.9%
```

---

## Definition of Done

### Completion Criteria:

- [ ] SamplerStateHash and SamplerStateEqual implemented
- [ ] Sampler cache (hash map) added to Device
- [ ] createSamplerState checks cache before allocating
- [ ] Reference counting tracks sampler usage
- [ ] Cache cleanup on last reference removal
- [ ] Statistics logging shows cache effectiveness
- [ ] Optional: TextureView caching implemented
- [ ] All unit tests pass (including new cache tests)
- [ ] All render sessions pass without visual changes
- [ ] Cache hit rate verification (99%+ for identical states)
- [ ] Code review approved

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Identical sampler states use same cached descriptor
2. Cache hit rate shows 99%+ efficiency for duplicate states
3. All render sessions produce identical output
4. No visual changes or behavior differences
5. Reference counting prevents premature cleanup

**Post in chat:**
```
G-007 Fix Complete - Ready for Review

Descriptor Caching by State Hash:
- Cache: Hash map maps SamplerStateDesc to descriptor index
- Hit Check: Lookup in cache before allocating
- Reference Counting: Tracks usage of cached descriptors
- Cleanup: Descriptors freed when last reference removed
- Statistics: Cache effectiveness logged (hit rates, space savings)

Testing Results:
- Unit tests: PASS (all D3D12 tests)
- Cache hits: 100+ identical samplers -> 1 descriptor
- Render sessions: PASS (no visual changes)
- Cache effectiveness: 99%+ for duplicate states
- Space savings: ~99% for heavy sampler reuse scenarios

Awaiting confirmation to proceed.
```

---

## Related Issues

### Blocks:
- None

### Related:
- **G-003** - Descriptor fragmentation (reduces heap pressure)
- **C-005** - Sampler heap exhaustion (caching prevents exhaustion)

---

## Implementation Priority

**Priority:** P2-Low (GPU Memory/Performance)
**Estimated Effort:** 2-3 hours
**Risk:** Low (Caching is transparent; reference counting is straightforward)
**Impact:** Eliminates 90-99% of redundant descriptor allocations for typical workloads

**Notes:**
- Sampler states are most beneficial for caching (small state, commonly duplicated)
- Texture view caching is optional but follows same pattern
- Hash function must be consistent with equality comparison
- Reference counting ensures descriptors aren't cleaned up prematurely
- Cache statistics are important for verification and debugging

---

## References

- https://learn.microsoft.com/windows/win32/direct3d12/descriptors-best-practices
- https://learn.microsoft.com/windows/win32/direct3d12/sampler-feedback-texture
- https://github.com/microsoft/DirectX-Graphics-Samples/search?q=cache
- https://learn.microsoft.com/windows/win32/direct3d12/memory-management-strategies
- https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_sampler_desc

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
