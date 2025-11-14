# TASK_P0_DX12-002: Implement Root Signature Caching

**Priority:** P0 - Critical (BLOCKING)
**Estimated Effort:** 1 day
**Status:** Open

---

## Problem Statement

The D3D12 backend currently serializes and creates a new root signature for every Pipeline State Object (PSO) creation. Root signatures define the shader resource binding layout and are expensive to create due to serialization overhead. Creating redundant root signatures for identical binding layouts wastes CPU time and memory.

### Current Behavior
- Every call to `createRenderPipeline()` serializes a new `D3D12_ROOT_SIGNATURE_DESC`
- `D3D12SerializeRootSignature()` called repeatedly for identical binding layouts
- `CreateRootSignature()` invoked without checking for existing compatible signatures
- No deduplication mechanism

### Expected Behavior
- Root signatures cached and reused across PSOs with identical binding layouts
- Serialization only performed once per unique layout
- Cache keyed by hash of root signature descriptor
- Reduced PSO creation overhead

---

## Evidence and Code Location

### Where to Find the Issue

**File:** `src/igl/d3d12/Device.cpp`

**Search Pattern:**
1. Locate function `Device::createRenderPipeline()`
2. Find the root signature creation code before `CreateGraphicsPipelineState()`
3. Look for calls to `D3D12SerializeRootSignature()` or `D3D12SerializeVersionedRootSignature()`
4. Look for calls to `CreateRootSignature()`
5. Notice there is NO cache lookup before serialization/creation
6. Notice there is NO cache insertion after creation

**Search Keywords:**
- `D3D12SerializeRootSignature`
- `CreateRootSignature`
- `D3D12_ROOT_SIGNATURE_DESC`
- `ID3D12RootSignature`

**What to Look For:**
- Direct serialization calls without caching
- Root signature creation inside pipeline creation function
- Absence of `std::unordered_map` for root signature cache
- No hash computation of root signature layout

---

## Impact

**Severity:** Critical - Blocks production use
**Performance:** Redundant serialization on every PSO creation (cumulative with TASK_P0_DX12-001)
**User Experience:** Increased PSO creation latency

**Affected Code Paths:**
- All render pipeline creation
- All compute pipeline creation
- Any code path that creates PSOs

**Blocks:**
- PSO creation performance optimization
- Production deployment

---

## Official References

### Microsoft Documentation

1. **Root Signatures** - [Root Signature Overview](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signatures)
   - "Applications should cache and reuse root signatures"
   - Root signature versioning (1.0 vs 1.1)

2. **D3D12SerializeRootSignature** - [API Reference](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-d3d12serializerootsignature)
   - Serialization cost documented

3. **CreateRootSignature** - [ID3D12Device Method](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createrootsignature)

4. **Root Signature Best Practices** - [Performance Guidelines](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signature-limits)
   - "Minimize the number of unique root signatures"
   - "Share root signatures across PSOs when possible"

### DirectX-Graphics-Samples

- **D3D12HelloConstBuffers** - Shows root signature creation pattern
- **D3D12Bundles** - Demonstrates root signature reuse
- **D3D12DynamicIndexing** - Complex root signature example

### Cross-Reference

- **Vulkan Backend:** Pipeline layouts cached via `VkPipelineLayout` cache
- **Metal Backend:** Uses `MTLRenderPipelineDescriptor` with implicit layout caching

---

## Implementation Guidance

### High-Level Approach

1. **Create Root Signature Cache Structure**
   - Add `std::unordered_map<size_t, ComPtr<ID3D12RootSignature>>` to `Device` class
   - Key: Hash of root signature descriptor layout
   - Value: Cached `ID3D12RootSignature` object

2. **Implement Root Signature Hash Function**
   - Hash all relevant fields from `D3D12_ROOT_SIGNATURE_DESC`:
     - Number and types of root parameters (constants, descriptors, tables)
     - Descriptor table ranges (type, count, register space)
     - Static samplers
     - Root signature flags
   - Use consistent hashing algorithm (FNV-1a, std::hash, etc.)

3. **Modify Pipeline Creation Flow**
   ```cpp
   // Pseudocode:
   D3D12_ROOT_SIGNATURE_DESC rootSigDesc = buildRootSignatureDesc(desc);
   size_t rootSigHash = computeRootSignatureHash(rootSigDesc);

   auto it = rootSignatureCache_.find(rootSigHash);
   if (it != rootSignatureCache_.end()) {
     rootSignature = it->second; // Reuse cached
   } else {
     // Serialize and create new
     ComPtr<ID3DBlob> signature, error;
     D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &error);
     device_->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                   IID_PPV_ARGS(&rootSignature));
     rootSignatureCache_[rootSigHash] = rootSignature;
   }

   // Use rootSignature in PSO creation
   psoDesc.pRootSignature = rootSignature.Get();
   ```

4. **Cache Lifetime Management**
   - Root signatures should outlive PSOs that use them
   - Keep alive via ComPtr reference counting
   - Optional: Clear cache when device is reset

### Detailed Steps

**Step 1:** Add cache member to `Device` class
- Location: `src/igl/d3d12/Device.h`
- Add: `std::unordered_map<size_t, ComPtr<ID3D12RootSignature>> rootSignatureCache_;`

**Step 2:** Implement root signature hash function
- Create `size_t Device::hashRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc)`
- Hash each root parameter:
  - Parameter type (constant, descriptor, table)
  - For tables: hash each descriptor range (type, count, base register)
  - Hash static samplers if present
  - Hash root signature flags

**Step 3:** Extract root signature creation into helper function
- Create `ComPtr<ID3D12RootSignature> Device::getOrCreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc)`
- Implement cache lookup logic
- Serialize and create only on cache miss

**Step 4:** Modify `Device::createRenderPipeline()`
- Replace inline root signature creation with `getOrCreateRootSignature()` call
- Ensure root signature is set in `psoDesc.pRootSignature`

**Step 5:** Modify `Device::createComputePipeline()` similarly

**Step 6:** Optional - Add versioned root signature support
- Support `D3D12_VERSIONED_ROOT_SIGNATURE_DESC` for root signature 1.1
- Use `D3D12SerializeVersionedRootSignature()` on supported systems

### Compatibility Considerations

- **Root Signature Version:** Prefer 1.1 if supported (better performance)
- **Fallback:** Use version 1.0 for broader compatibility
- Check feature level and use versioned APIs accordingly

---

## Testing Requirements

### Mandatory Tests - Must Pass

**Unit Tests:**
```bash
test_unittests.bat
```
- All D3D12 device and pipeline tests must pass
- No regressions in resource binding tests

**Render Sessions:**
```bash
test_all_sessions.bat
```
All sessions must pass:
- BasicFramebufferSession
- HelloTriangleSession
- HelloWorldSession
- ThreeCubesRenderSession
- Textured3DCubeSession
- DrawInstancedSession
- MRTSession
- All compute-based sessions
- All other sessions in test suite

### Validation Steps

1. **Functional Validation**
   - All render sessions produce correct output
   - No binding errors or validation layer warnings
   - Shaders receive correct resource bindings

2. **Performance Validation**
   - Log cache hit/miss rate for root signatures
   - Verify cache hit rate >90% for typical workloads
   - Measure reduction in serialization overhead

3. **Compatibility Validation**
   - Test on different GPU vendors (NVIDIA, AMD, Intel)
   - Test on WARP software rasterizer
   - Verify behavior on root signature version 1.0 and 1.1

4. **Regression Check**
   - Compare against baseline results
   - No visual differences in render sessions
   - No performance degradation

### Test-Specific Guidance

- **No modifications allowed** to test scripts or build directory
- If tests fail, STOP and request analysis
- Root signature validation errors indicate incorrect hash collisions

---

## Success Criteria

- [ ] Root signature cache implemented in `Device` class
- [ ] Hash function correctly identifies unique root signature layouts
- [ ] Cache lookup before serialization in `createRenderPipeline()`
- [ ] Cache lookup before serialization in `createComputePipeline()`
- [ ] Root signatures correctly shared across multiple PSOs
- [ ] All unit tests pass (`test_unittests.bat`)
- [ ] All render sessions pass (`test_all_sessions.bat`)
- [ ] No resource binding errors
- [ ] Cache hit rate >90% for repeated layouts
- [ ] No memory leaks (verify with debug layer)
- [ ] User confirms test results before proceeding

---

## Dependencies

**Depends On:**
- None (can be implemented independently)

**Works With:**
- TASK_P0_DX12-001 (PSO Caching) - Complementary optimization
- Both tasks reduce pipeline creation overhead

**Blocks:**
- Performance-critical features requiring fast pipeline creation

---

## Restrictions

**CRITICAL - Read Before Implementation:**

1. **Test Immutability**
   - ❌ DO NOT modify test scripts (`test_unittests.bat`, `test_all_sessions.bat`)
   - ❌ DO NOT modify render session source code
   - ❌ DO NOT modify build directory

2. **User Confirmation Required**
   - ⚠️ Run all tests after implementation
   - ⚠️ Report results to user
   - ⚠️ Wait for confirmation before next task
   - ⚠️ Any failure = STOP

3. **Code Modification Scope**
   - ✅ Modify `src/igl/d3d12/Device.h` and `Device.cpp`
   - ✅ Add helper functions for root signature hashing
   - ✅ Add telemetry for cache statistics
   - ❌ DO NOT modify IGL public API
   - ❌ DO NOT change resource binding semantics

4. **Validation**
   - Use D3D12 debug layer to detect validation errors
   - Check for root signature mismatch errors
   - Verify resource binding correctness

---

**Estimated Timeline:** 1 day
**Risk Level:** Low-Medium (isolated change, well-defined scope)
**Validation Effort:** 2-3 hours

---

*Task Created: 2025-11-08*
*Last Updated: 2025-11-08*
