# G-008: PSO Cache Has High Miss Rate

**Priority**: MEDIUM
**Category**: Graphics - Performance
**Estimated Effort**: 3-4 days
**Status**: Not Started

---

## 1. Problem Statement

The Pipeline State Object (PSO) cache in the D3D12 backend has a high miss rate, causing frequent PSO creation at runtime. PSO creation is an expensive operation that can cause frame hitches and stuttering, especially during scene transitions or when new rendering effects are first used.

**Symptoms**:
- Frame time spikes when new rendering states are encountered
- Excessive PSO compilation during gameplay
- Higher than expected CPU usage in PSO creation paths
- Poor performance on first-time rendering operations

**Impact**:
- Degraded user experience with visible stuttering
- Increased frame time variance
- Suboptimal CPU utilization
- Longer initial load times if PSOs aren't pre-cached

---

## 2. Root Cause Analysis

The PSO cache miss rate is likely high due to one or more of the following issues:

### 2.1 Inadequate Cache Key Design
The cache key used to identify PSOs may not be comprehensive enough, causing identical pipeline configurations to be treated as different states. Or conversely, the key may be too specific, preventing reuse of compatible PSOs.

### 2.2 Missing Pre-Warming Strategy
PSOs may not be pre-compiled during initialization or load screens, forcing runtime compilation when objects are first rendered.

### 2.3 Inefficient Hash Function
A poor hash function for the PSO cache key could cause hash collisions or slow lookups, reducing cache effectiveness.

### 2.4 Missing Serialization Support
PSO cache may not persist between application runs, forcing recompilation every launch instead of leveraging disk-based caching.

### 2.5 Cache Size Limitations
The cache may have size limits that cause eviction of frequently-used PSOs, or the eviction policy may not be LRU-based.

---

## 3. Official Documentation References

**Primary Resources**:

1. **Pipeline State Objects Overview**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3d12/managing-graphics-pipeline-state-in-direct3d-12
   - Comprehensive guide to PSO management and best practices

2. **PSO Caching and Serialization**:
   - https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12pipelinestate-getcachedblob
   - Documents `GetCachedBlob()` for PSO serialization

3. **PSO Library for Pre-Compilation**:
   - https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nn-d3d12-id3d12pipelinelibrary1
   - Explains `ID3D12PipelineLibrary1` for efficient PSO management

4. **Performance Tuning Guide**:
   - https://docs.microsoft.com/en-us/windows/win32/direct3d12/performance-tips
   - Section on minimizing PSO compilation hitches

**Sample Code**:

5. **D3D12 Mini Engine - PSO Management**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/MiniEngine/Core
   - See `GraphicsPSO.cpp` and `PipelineState.h` for reference implementation

---

## 4. Code Location Strategy

### 4.1 Primary Investigation Targets

**Search for PSO cache implementation**:
```
Pattern: "PipelineState" OR "PSO" + "cache" OR "map" OR "unordered_map"
Files: src/igl/d3d12/**/*.{cpp,h}
Focus: Look for std::unordered_map or cache container storing PSO objects
```

**Search for PSO creation functions**:
```
Pattern: "CreateGraphicsPipelineState" OR "CreateComputePipelineState"
Files: src/igl/d3d12/Device.cpp, src/igl/d3d12/PipelineState*.cpp
Focus: Where ID3D12Device::CreateGraphicsPipelineState is called
```

**Search for cache key generation**:
```
Pattern: "hash" + "pipeline" OR "PSO" OR "state"
Files: src/igl/d3d12/**/*.cpp
Focus: Hash function implementations for pipeline state
```

### 4.2 Likely File Locations

Based on typical D3D12 backend architecture:
- `src/igl/d3d12/Device.cpp` - Device-level PSO creation
- `src/igl/d3d12/PipelineState.cpp` or `src/igl/d3d12/ComputePipelineState.cpp` - PSO wrapper classes
- `src/igl/d3d12/RenderPipelineState.cpp` - Graphics pipeline state management
- `src/igl/d3d12/CommandQueue.cpp` - May trigger PSO compilation
- `src/igl/d3d12/RenderCommandEncoder.cpp` - Where PSOs are bound

### 4.3 Key Data Structures to Find

Look for:
- PSO cache container (e.g., `std::unordered_map<PSOKey, ComPtr<ID3D12PipelineState>>`)
- Cache key structure (likely includes shader hashes, render target formats, blend state, etc.)
- PSO descriptor structures
- Mutex or synchronization for thread-safe cache access

---

## 5. Detection Strategy

### 5.1 Add Instrumentation Code

**Step 1: Add cache hit/miss counters**

Find the PSO cache lookup function and add:

```cpp
// In PipelineState cache lookup function
struct PSOCacheStats {
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> totalLookups{0};

    float getHitRate() const {
        uint64_t total = totalLookups.load();
        return total > 0 ? (float)hits.load() / total : 0.0f;
    }
};

static PSOCacheStats g_psoCacheStats;

// In cache lookup:
g_psoCacheStats.totalLookups++;
auto it = psoCache_.find(key);
if (it != psoCache_.end()) {
    g_psoCacheStats.hits++;
    return it->second;
} else {
    g_psoCacheStats.misses++;
    // Create new PSO
}
```

**Step 2: Add periodic reporting**

```cpp
// Log cache statistics every 60 frames or on shutdown
void logPSOCacheStats() {
    IGL_LOG_INFO("PSO Cache Statistics:\n");
    IGL_LOG_INFO("  Total Lookups: %llu\n", g_psoCacheStats.totalLookups.load());
    IGL_LOG_INFO("  Hits: %llu\n", g_psoCacheStats.hits.load());
    IGL_LOG_INFO("  Misses: %llu\n", g_psoCacheStats.misses.load());
    IGL_LOG_INFO("  Hit Rate: %.2f%%\n", g_psoCacheStats.getHitRate() * 100.0f);
    IGL_LOG_INFO("  Cache Size: %zu\n", psoCache_.size());
}
```

### 5.2 Manual Testing Procedure

**Test 1: Run existing test suite with instrumentation**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"
```

**Test 2: Run sample applications**
```bash
./build/Debug/samples/Textured3DCube
./build/Debug/samples/TQSession
./build/Debug/samples/MRTSession
```

Monitor console output for PSO cache statistics. A good hit rate should be >95% after initial warm-up.

**Test 3: Analyze cache key diversity**

Add logging to print unique cache keys:
```cpp
IGL_LOG_DEBUG("PSO Cache Miss - Key: %016llx\n", key.hash());
```

Look for patterns indicating unnecessary key variations.

### 5.3 Expected Baseline Metrics

- **Initial warm-up phase**: 0-50% hit rate (first 1-2 seconds)
- **Steady state**: >95% hit rate
- **Cache size**: Should stabilize after initial scene load
- **Miss causes**: Should primarily be from new materials/effects

---

## 6. Fix Guidance

### 6.1 Improve Cache Key Design

**Current State Investigation**: Find the PSO cache key structure.

**Example Fix**: Ensure comprehensive but not over-specific keys:

```cpp
// Define a robust PSO cache key
struct GraphicsPSOKey {
    uint64_t vertexShaderHash;
    uint64_t pixelShaderHash;
    uint64_t geometryShaderHash;  // 0 if not used
    uint32_t renderTargetFormatHash;  // Combined hash of all RT formats
    uint32_t depthStencilFormat;
    uint32_t sampleDesc;  // Combined count + quality
    uint32_t blendStateHash;
    uint32_t rasterizerStateHash;
    uint32_t depthStencilStateHash;
    uint32_t topologyType;
    uint32_t rootSignatureHash;

    bool operator==(const GraphicsPSOKey& other) const {
        return memcmp(this, &other, sizeof(GraphicsPSOKey)) == 0;
    }

    struct Hasher {
        size_t operator()(const GraphicsPSOKey& key) const {
            // Use high-quality hash function (e.g., xxHash or FNV-1a)
            return XXH3_64bits(&key, sizeof(key));
        }
    };
};

// Use as cache container
std::unordered_map<GraphicsPSOKey, ComPtr<ID3D12PipelineState>, GraphicsPSOKey::Hasher> psoCache_;
```

### 6.2 Implement PSO Pre-Warming

Add a method to pre-compile commonly used PSOs:

```cpp
// In Device or dedicated PSO manager class
void preWarmPSOCache() {
    IGL_LOG_INFO("Pre-warming PSO cache...\n");

    // Define common pipeline configurations
    std::vector<PipelineDesc> commonPipelines = {
        // Opaque geometry pipeline
        {.blend = BlendMode::Opaque, .depthTest = true, .cullMode = CullMode::Back},
        // Alpha-blended pipeline
        {.blend = BlendMode::AlphaBlend, .depthTest = true, .cullMode = CullMode::None},
        // UI rendering pipeline
        {.blend = BlendMode::AlphaBlend, .depthTest = false, .cullMode = CullMode::None},
        // Shadow mapping pipeline
        {.blend = BlendMode::Opaque, .depthTest = true, .colorWrite = false},
    };

    for (const auto& desc : commonPipelines) {
        // Create PSO synchronously during load
        auto pso = createRenderPipelineState(desc, nullptr);
        // PSO is now cached for later use
    }

    IGL_LOG_INFO("PSO cache pre-warmed with %zu pipelines\n", commonPipelines.size());
}
```

Call this during device initialization or during load screens.

### 6.3 Implement PSO Serialization

Add support for saving/loading PSO cache to disk:

```cpp
// Save PSO cache to disk
void serializePSOCache(const std::string& cacheFile) {
    std::ofstream file(cacheFile, std::ios::binary);
    if (!file) {
        IGL_LOG_ERROR("Failed to open PSO cache file for writing\n");
        return;
    }

    // Write cache version and entry count
    uint32_t version = 1;
    uint32_t count = static_cast<uint32_t>(psoCache_.size());
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    // Write each PSO
    for (const auto& [key, pso] : psoCache_) {
        // Write key
        file.write(reinterpret_cast<const char*>(&key), sizeof(key));

        // Get cached blob from PSO
        ComPtr<ID3DBlob> cachedBlob;
        HRESULT hr = pso->GetCachedBlob(&cachedBlob);
        if (SUCCEEDED(hr) && cachedBlob) {
            uint32_t blobSize = static_cast<uint32_t>(cachedBlob->GetBufferSize());
            file.write(reinterpret_cast<const char*>(&blobSize), sizeof(blobSize));
            file.write(static_cast<const char*>(cachedBlob->GetBufferPointer()), blobSize);
        } else {
            uint32_t blobSize = 0;
            file.write(reinterpret_cast<const char*>(&blobSize), sizeof(blobSize));
        }
    }

    IGL_LOG_INFO("Serialized %u PSOs to cache file\n", count);
}

// Load PSO cache from disk
void deserializePSOCache(const std::string& cacheFile) {
    std::ifstream file(cacheFile, std::ios::binary);
    if (!file) {
        IGL_LOG_INFO("No PSO cache file found, starting fresh\n");
        return;
    }

    uint32_t version, count;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    if (version != 1) {
        IGL_LOG_WARNING("PSO cache version mismatch, ignoring cache\n");
        return;
    }

    for (uint32_t i = 0; i < count; ++i) {
        GraphicsPSOKey key;
        file.read(reinterpret_cast<char*>(&key), sizeof(key));

        uint32_t blobSize;
        file.read(reinterpret_cast<char*>(&blobSize), sizeof(blobSize));

        if (blobSize > 0) {
            std::vector<uint8_t> blobData(blobSize);
            file.read(reinterpret_cast<char*>(blobData.data()), blobSize);

            // Create PSO from cached blob
            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = getPipelineDescFromKey(key);
            desc.CachedPSO.pCachedBlob = blobData.data();
            desc.CachedPSO.CachedBlobSizeInBytes = blobSize;

            ComPtr<ID3D12PipelineState> pso;
            HRESULT hr = device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
            if (SUCCEEDED(hr)) {
                psoCache_[key] = pso;
            }
        }
    }

    IGL_LOG_INFO("Loaded %zu PSOs from cache file\n", psoCache_.size());
}
```

### 6.4 Optimize Cache Management

Implement proper cache eviction policy:

```cpp
struct PSOCacheEntry {
    ComPtr<ID3D12PipelineState> pso;
    uint64_t lastAccessFrame;
    uint32_t accessCount;
};

class PSOCache {
private:
    std::unordered_map<GraphicsPSOKey, PSOCacheEntry, GraphicsPSOKey::Hasher> cache_;
    uint64_t currentFrame_ = 0;
    size_t maxCacheSize_ = 1024;  // Configurable limit

public:
    ID3D12PipelineState* lookup(const GraphicsPSOKey& key) {
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            it->second.lastAccessFrame = currentFrame_;
            it->second.accessCount++;
            return it->second.pso.Get();
        }
        return nullptr;
    }

    void insert(const GraphicsPSOKey& key, ComPtr<ID3D12PipelineState> pso) {
        // Evict least recently used if at capacity
        if (cache_.size() >= maxCacheSize_) {
            evictLRU();
        }

        PSOCacheEntry entry;
        entry.pso = pso;
        entry.lastAccessFrame = currentFrame_;
        entry.accessCount = 1;
        cache_[key] = std::move(entry);
    }

    void advanceFrame() {
        currentFrame_++;
    }

private:
    void evictLRU() {
        auto lruIt = cache_.begin();
        uint64_t oldestFrame = lruIt->second.lastAccessFrame;

        for (auto it = cache_.begin(); it != cache_.end(); ++it) {
            if (it->second.lastAccessFrame < oldestFrame) {
                oldestFrame = it->second.lastAccessFrame;
                lruIt = it;
            }
        }

        IGL_LOG_DEBUG("Evicting PSO last used at frame %llu\n", oldestFrame);
        cache_.erase(lruIt);
    }
};
```

---

## 7. Testing Requirements

### 7.1 Functional Testing

**Test 1: Verify cache functionality**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*PipelineState*"
```

**Expected**: All existing tests pass with improved cache hit rate.

**Test 2: Run all rendering sessions**
```bash
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Session*"
```

**Expected**: No visual regressions, improved frame times.

**Test 3: Run sample applications**
```bash
./build/Debug/samples/Textured3DCube
./build/Debug/samples/TQSession
./build/Debug/samples/MRTSession
```

**Expected**: Smooth rendering with high PSO cache hit rate (>95% after warm-up).

### 7.2 Performance Testing

**Metric Collection**:
- Measure PSO cache hit rate over time
- Record number of PSO compilations per frame
- Measure frame time variance before/after optimization

**Target Metrics**:
- Cache hit rate: >95% in steady state
- PSO compilations after warm-up: <1 per 60 frames
- Frame time variance: Reduced by at least 30%

### 7.3 Test Modification Restrictions

**CRITICAL CONSTRAINTS**:
- Do NOT modify any existing test assertions
- Do NOT change test expected values
- Do NOT alter test logic or flow
- Changes must be implementation-only, not test-visible
- All tests must pass without modification

If tests fail, the implementation needs adjustment, not the tests.

---

## 8. Definition of Done

### 8.1 Implementation Checklist

- [ ] PSO cache key design reviewed and optimized
- [ ] Cache hit rate improved to >95% in steady state
- [ ] PSO pre-warming implemented during initialization
- [ ] PSO serialization/deserialization implemented
- [ ] Cache eviction policy implemented (LRU or similar)
- [ ] Instrumentation code added for monitoring
- [ ] Cache statistics logging implemented

### 8.2 Testing Checklist

- [ ] All unit tests pass: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"`
- [ ] PSO cache statistics show >95% hit rate after warm-up
- [ ] No visual regressions in sample applications
- [ ] Frame time variance reduced measurably
- [ ] PSO cache correctly persists between runs
- [ ] Cache eviction doesn't cause performance degradation

### 8.3 Documentation Checklist

- [ ] Code comments explain cache key design decisions
- [ ] Cache statistics interpretation documented
- [ ] Configuration options documented (cache size, eviction policy)

### 8.4 Sign-Off Criteria

**Before proceeding with this task, YOU MUST**:
1. Read and understand all 11 sections of this document
2. Verify you can locate the PSO cache implementation
3. Confirm you can build and run the test suite
4. Get explicit approval to proceed with implementation

**Do not make any code changes until all criteria are met and approval is given.**

---

## 9. Related Issues

### 9.1 Blocking Issues
None - this can be implemented independently.

### 9.2 Blocked Issues
- May improve stability of other rendering issues by reducing PSO creation overhead

### 9.3 Related Performance Issues
- G-009: Copy operations not async (both affect frame time consistency)
- G-010: Resource barrier stalls (both affect pipeline efficiency)
- H-013: PSO cache implementation (prerequisite fix)

---

## 10. Implementation Priority

**Priority Level**: MEDIUM

**Rationale**:
- Improves user experience by reducing stuttering
- Performance optimization rather than correctness fix
- Relatively isolated change with low risk
- Provides measurable improvement in frame time consistency

**Recommended Order**:
1. Implement instrumentation first to measure baseline
2. Optimize cache key design
3. Add pre-warming support
4. Implement serialization last (optional enhancement)

**Estimated Timeline**:
- Day 1: Investigation, instrumentation, baseline measurements
- Day 2: Cache key optimization, initial testing
- Day 3: Pre-warming implementation
- Day 4: Serialization support, final testing and validation

---

## 11. References

### 11.1 Microsoft Official Documentation
- Pipeline State Objects: https://docs.microsoft.com/en-us/windows/win32/direct3d12/managing-graphics-pipeline-state-in-direct3d-12
- GetCachedBlob: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12pipelinestate-getcachedblob
- ID3D12PipelineLibrary1: https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nn-d3d12-id3d12pipelinelibrary1
- Performance Tuning: https://docs.microsoft.com/en-us/windows/win32/direct3d12/performance-tips

### 11.2 Sample Code References
- DirectX-Graphics-Samples MiniEngine: https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/MiniEngine/Core
  - `GraphicsPSO.cpp` - PSO creation and caching
  - `PipelineState.h` - PSO wrapper interface

### 11.3 Additional Reading
- PIX documentation for analyzing PSO creation: https://devblogs.microsoft.com/pix/
- D3D12 Memory Management Patterns: https://docs.microsoft.com/en-us/windows/win32/direct3d12/memory-management-strategies

### 11.4 Internal Codebase
- Search for "CreateGraphicsPipelineState" in src/igl/d3d12/
- Review existing PSO cache implementation in Device.cpp or PipelineState.cpp
- Check for existing instrumentation in debug builds

---

**Document Version**: 1.0
**Last Updated**: 2025-11-12
**Author**: Development Team
**Reviewer**: [Pending]
