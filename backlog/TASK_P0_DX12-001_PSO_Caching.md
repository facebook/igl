# TASK_P0_DX12-001: Implement Pipeline State Object (PSO) Caching

**Priority:** P0 - Critical (BLOCKING)
**Estimated Effort:** 1-2 days
**Status:** Open

---

## Problem Statement

The D3D12 backend currently creates a new Pipeline State Object (PSO) on every call to `createRenderPipeline()` and `createComputePipeline()` without any caching mechanism. PSO creation is an expensive operation (10-100ms) involving shader compilation, state validation, and driver optimizations. Repeated creation of identical PSOs causes severe performance overhead in any application that recreates pipelines.

### Current Behavior
- Every `Device::createRenderPipeline()` call invokes `ID3D12Device::CreateGraphicsPipelineState()`
- No deduplication or caching of previously created PSOs
- Identical pipeline configurations result in redundant D3D12 API calls
- Performance degrades linearly with number of pipeline creations

### Expected Behavior
- First creation of a pipeline with specific configuration creates and caches the PSO
- Subsequent requests with identical configuration return cached PSO
- Cache keyed by hash of pipeline descriptor (shaders, render targets, blend state, etc.)
- Cache invalidation when underlying resources are destroyed

---

## Evidence and Code Location

### Where to Find the Issue

**File:** `src/igl/d3d12/Device.cpp`

**Search Pattern:**
1. Locate function `Device::createRenderPipeline(const RenderPipelineDesc& desc, Result* outResult)`
2. Find the call to `device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso))`
3. Notice there is NO cache lookup before creation
4. Notice there is NO cache insertion after creation

**Similar issue in:**
- `Device::createComputePipeline()` - Same pattern for compute PSOs

**What to Look For:**
- Absence of `std::unordered_map` or similar cache structure for PSOs
- Direct `CreateGraphicsPipelineState()` / `CreateComputePipelineState()` calls without cache checks
- No hash computation of pipeline descriptor

---

## Impact

**Severity:** Critical - Blocks production use
**Performance:** 10-100ms overhead per uncached PSO creation
**User Experience:** Severe frame drops, stuttering during rendering

**Affected Code Paths:**
- All render pipeline creation
- All compute pipeline creation
- Applications with dynamic shader switching
- Applications that recreate pipelines per frame

**Blocks:**
- All performance-critical rendering paths
- Production deployment of D3D12 backend

---

## Official References

### Microsoft Documentation
1. **Pipeline State Objects** - [D3D12 Graphics Pipeline](https://learn.microsoft.com/en-us/windows/win32/direct3d12/managing-graphics-pipeline-state-in-direct3d-12)
   - "Applications should cache pipeline state objects and reuse them"
   - "PSO creation is expensive and should be done at initialization time"

2. **CreateGraphicsPipelineState** - [ID3D12Device Method](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-creategraphicspipelinestate)
   - Performance implications documented

3. **Pipeline State Caching Best Practices** - [D3D12 Best Practices](https://learn.microsoft.com/en-us/windows/win32/direct3d12/efficiency-best-practices)

### DirectX-Graphics-Samples
- **D3D12HelloTriangle** - Shows PSO creation pattern (though simple, no caching)
- **D3D12Multithreading** - Demonstrates PSO reuse across frames
- **D3D12PredicationQueries** - Example of managing multiple PSOs

### Cross-Reference
- **Vulkan Backend:** `src/igl/vulkan/VulkanPipelineBuilder.cpp` - Has pipeline cache via `VkPipelineCache`
- **Metal Backend:** Uses `MTLRenderPipelineState` with caching in `Device.mm`

---

## Implementation Guidance

### High-Level Approach

1. **Create PSO Cache Structure**
   - Add `std::unordered_map<size_t, ComPtr<ID3D12PipelineState>>` to `Device` class
   - Separate caches for graphics and compute PSOs (or unified with type flag)

2. **Implement Hash Function**
   - Hash all relevant fields from `RenderPipelineDesc`:
     - Shader module IDs/bytecode hashes
     - Vertex input layout
     - Render target formats, sample counts
     - Blend state, depth/stencil state, rasterizer state
     - Primitive topology
   - Use `std::hash` or FNV-1a for combining field hashes

3. **Modify Pipeline Creation Functions**
   ```cpp
   // Pseudocode pattern:
   size_t psoHash = computePipelineHash(desc);
   auto it = psoCache_.find(psoHash);
   if (it != psoCache_.end()) {
     return it->second; // Return cached PSO
   }

   // Create new PSO
   ComPtr<ID3D12PipelineState> pso;
   HRESULT hr = device_->CreateGraphicsPipelineState(...);
   if (SUCCEEDED(hr)) {
     psoCache_[psoHash] = pso;
   }
   ```

4. **Cache Invalidation**
   - Track PSO lifetimes with shared_ptr or reference counting
   - Invalidate cache entries when shader modules are destroyed
   - Optional: Implement LRU eviction for memory management

### Detailed Steps

**Step 1:** Add cache member variables to `Device` class
- Location: `src/igl/d3d12/Device.h`
- Add: `std::unordered_map<size_t, ComPtr<ID3D12PipelineState>> graphicsPSOCache_;`
- Add: `std::unordered_map<size_t, ComPtr<ID3D12PipelineState>> computePSOCache_;`

**Step 2:** Implement hash function
- Create helper function `size_t Device::hashRenderPipelineDesc(const RenderPipelineDesc& desc)`
- Hash shader handles, vertex attributes, render target formats, state objects
- Consider using `boost::hash_combine` pattern or similar

**Step 3:** Modify `Device::createRenderPipeline()`
- Before calling `CreateGraphicsPipelineState()`, compute hash and check cache
- If found, increment ref count and return cached PSO
- If not found, create new PSO and insert into cache

**Step 4:** Repeat for `Device::createComputePipeline()`

**Step 5:** Add cache management
- Optional: Implement `Device::clearPSOCache()` for debugging
- Optional: Add telemetry (cache hit rate logging)

### Thread Safety Considerations
- If multi-threaded pipeline creation is planned (P3 task), add mutex for cache access
- Current single-threaded implementation doesn't require locks

---

## Testing Requirements

### Mandatory Tests - Must Pass

**Unit Tests:**
```bash
test_unittests.bat
```
- All existing D3D12 device tests must pass
- Pipeline creation tests must complete successfully

**Render Sessions:**
```bash
test_all_sessions.bat
```
All render sessions must complete without regression:
- BasicFramebufferSession
- ColorSession
- DrawInstancedSession
- EmptySession
- HelloTriangleSession
- HelloWorldSession
- MRTSession
- ThreeCubesRenderSession
- Textured3DCubeSession
- TQSession
- All other sessions listed in test script

### Validation Steps

1. **Functional Validation**
   - Run all render sessions - visual output must be identical
   - No device removal errors
   - No validation layer errors (if D3D12 debug layer enabled)

2. **Performance Validation**
   - Add logging to track PSO cache hits/misses
   - Verify cache hit rate >90% for repeated pipeline usage
   - Measure PSO creation time reduction (should be <1ms for cache hits)

3. **Regression Check**
   - Any test failure triggers STOP and agent analysis
   - Compare against baseline test results
   - Visual comparison for render sessions (screenshot diffs)

### Test-Specific Guidance

- **No modifications allowed** to `test_unittests.bat` or `test_all_sessions.bat`
- **No modifications allowed** to test source files unless D3D12-specific bug fixes
- **No modifications allowed** to build directory structure

---

## Success Criteria

- [ ] PSO cache implemented in `Device` class
- [ ] Hash function covers all relevant pipeline descriptor fields
- [ ] `createRenderPipeline()` checks cache before creating new PSO
- [ ] `createComputePipeline()` checks cache before creating new PSO
- [ ] Cache correctly handles hash collisions (if any)
- [ ] All unit tests pass (`test_unittests.bat`)
- [ ] All render sessions pass (`test_all_sessions.bat`)
- [ ] No visual regression in any render session
- [ ] No performance regression in existing tests
- [ ] Cache hit rate >90% for repeated pipeline usage (measurable via logging)
- [ ] No memory leaks (verify with D3D12 debug layer live object reporting)
- [ ] User confirms test results before proceeding to next task

---

## Dependencies

**Depends On:**
- None (foundational task)

**Blocks/Enhances:**
- Works synergistically with TASK_P0_DX12-002 (Root Signature Caching)
- Enables performance optimizations across all P1/P2 tasks
- Required for production-grade D3D12 backend

**Related Tasks:**
- TASK_P0_DX12-002 - Root signature caching (complementary optimization)

---

## Restrictions

**CRITICAL - Read Before Implementation:**

1. **Test Immutability**
   - ❌ DO NOT modify `test_unittests.bat`
   - ❌ DO NOT modify `test_all_sessions.bat`
   - ❌ DO NOT modify test scripts in build directory
   - ❌ DO NOT modify render session source code (unless fixing D3D12-specific bugs)

2. **User Confirmation Required**
   - ⚠️ After implementation, run all tests
   - ⚠️ Report test results to user
   - ⚠️ Wait for explicit user confirmation before proceeding to next task
   - ⚠️ If ANY test fails, STOP and request agent analysis

3. **Code Modification Scope**
   - ✅ Modify `src/igl/d3d12/Device.h` and `Device.cpp`
   - ✅ Add helper functions for PSO hashing
   - ✅ Add telemetry/logging for cache statistics
   - ❌ DO NOT modify IGL public API headers
   - ❌ DO NOT change pipeline descriptor structures

4. **Regression Policy**
   - Any test failure = STOP immediately
   - Any visual regression = STOP immediately
   - Any performance regression = investigate and report

---

**Estimated Timeline:** 1-2 days
**Risk Level:** Medium (isolated to Device class, well-defined scope)
**Validation Effort:** 2-3 hours (full test suite + manual verification)

---

*Task Created: 2025-11-08*
*Last Updated: 2025-11-08*
