# D3D12 Backend Task Backlog Index

**Last Updated:** 2025-11-08
**Total Tasks:** 23
**Status:** All Open

---

## Priority Breakdown

| Priority | Count | Status |
|----------|-------|--------|
| P0 (Critical) | 7 | All Open |
| P1 (High) | 8 | All Open |
| P2 (Medium) | 6 | All Open |
| P3 (Low) | 2 | All Open |

---

## P0 - Critical Priority (BLOCKING)

### Performance & Caching
1. **[TASK_P0_DX12-001_PSO_Caching.md](TASK_P0_DX12-001_PSO_Caching.md)**
   - **Title:** Implement Pipeline State Object (PSO) Caching
   - **Impact:** 10-100ms overhead per PSO without caching
   - **Effort:** 1-2 days
   - **Blocks:** All performance-critical paths

2. **[TASK_P0_DX12-002_Root_Signature_Caching.md](TASK_P0_DX12-002_Root_Signature_Caching.md)**
   - **Title:** Implement Root Signature Caching
   - **Impact:** Redundant serialization on every PSO creation
   - **Effort:** 1 day
   - **Dependency:** Works with TASK_P0_DX12-001

### Descriptors & Root Signatures
3. **[TASK_P0_DX12-FIND-01_Unbounded_Descriptor_Ranges.md](TASK_P0_DX12-FIND-01_Unbounded_Descriptor_Ranges.md)**
   - **Title:** Fix Unbounded Descriptor Ranges for Tier 1 Hardware
   - **Impact:** Device creation fails on Tier 1 adapters/WARP
   - **Effort:** 1-2 days
   - **Critical:** Breaks compatibility with low-end hardware

4. **[TASK_P0_DX12-FIND-02_Descriptor_Heap_Overflow.md](TASK_P0_DX12-FIND-02_Descriptor_Heap_Overflow.md)**
   - **Title:** Fix Descriptor Heap Overflow and Missing Bounds Checks
   - **Impact:** Memory corruption, device removal in headless/long sessions
   - **Effort:** 2-3 days
   - **Critical:** Crashes automated tests

### Resources & Memory
5. **[TASK_P0_DX12-003_Resource_Leaks_GenerateMipmap.md](TASK_P0_DX12-003_Resource_Leaks_GenerateMipmap.md)**
   - **Title:** Fix Resource Leaks in Texture::generateMipmap()
   - **Impact:** 8+ leak paths on error returns
   - **Effort:** 4-6 hours
   - **Critical:** Memory exhaustion over time

6. **[TASK_P0_DX12-004_HRESULT_Validation.md](TASK_P0_DX12-004_HRESULT_Validation.md)**
   - **Title:** Add Missing HRESULT Validation in Descriptor Creation
   - **Impact:** Silent failures lead to crashes/undefined behavior
   - **Effort:** 3-4 hours
   - **Critical:** Production stability

7. **[TASK_P0_DX12-005_Upload_Buffer_Sync.md](TASK_P0_DX12-005_Upload_Buffer_Sync.md)**
   - **Title:** Fix Upload Buffer Command Allocator Synchronization
   - **Impact:** Device removal risk - resources destroyed before GPU completion
   - **Effort:** 4-6 hours
   - **Critical:** Can cause GPU crashes

---

## P1 - High Priority

### Synchronization & Command Queues
8. **[TASK_P1_DX12-FIND-03_Schedule_Fence_Monotonic.md](TASK_P1_DX12-FIND-03_Schedule_Fence_Monotonic.md)**
   - **Title:** Fix Non-Monotonic Schedule Fence Values
   - **Impact:** waitUntilScheduled() broken after first submit
   - **Effort:** 3-4 hours
   - **Blocks:** Timeline synchronization features

9. **[TASK_P1_DX12-006_Device_Removal_Exceptions.md](TASK_P1_DX12-006_Device_Removal_Exceptions.md)**
   - **Title:** Replace Device Removal Exceptions with Result Codes
   - **Impact:** Exceptions crash app, violates IGL convention
   - **Effort:** 6-8 hours

### Descriptors & Binding
10. **[TASK_P1_DX12-FIND-04_Compute_CBV_Binding.md](TASK_P1_DX12-FIND-04_Compute_CBV_Binding.md)**
    - **Title:** Implement Compute Constant Buffer Descriptor Binding
    - **Impact:** Compute shaders read garbage for CBVs
    - **Effort:** 1-2 days
    - **Critical:** Compute shaders unusable

11. **[TASK_P1_DX12-FIND-05_Storage_Buffer_Binding.md](TASK_P1_DX12-FIND-05_Storage_Buffer_Binding.md)**
    - **Title:** Implement Bind Group Storage Buffer Support
    - **Impact:** Storage buffers via bind groups don't work
    - **Effort:** 2-3 days
    - **Dependency:** Requires TASK_P0_DX12-FIND-02 fix first

### Resources
12. **[TASK_P1_DX12-FIND-06_RGBA_Channel_Swap.md](TASK_P1_DX12-FIND-06_RGBA_Channel_Swap.md)**
    - **Title:** Fix RGBA Texture Channel Swap Bug
    - **Impact:** All RGBA textures corrupted (R/B swapped)
    - **Effort:** 2-3 hours
    - **Critical:** Visual corruption in all RGBA content

### Cross-API Consistency
13. **[TASK_P1_DX12-007_Compute_Binding_Limits.md](TASK_P1_DX12-007_Compute_Binding_Limits.md)**
    - **Title:** Align Compute Shader Binding Limits to IGL Contract
    - **Impact:** Breaks cross-API portability (8 vs 16 slots)
    - **Effort:** 2-3 hours
    - **Blocks:** Portable compute shaders

14. **[TASK_P1_DX12-008_Missing_Limit_Queries.md](TASK_P1_DX12-008_Missing_Limit_Queries.md)**
    - **Title:** Implement Missing DeviceFeatureLimits Queries
    - **Impact:** Apps can't query texture dimensions, compute limits
    - **Effort:** 3-4 hours

15. **[TASK_P1_DX12-009_Upload_Ring_Buffer.md](TASK_P1_DX12-009_Upload_Ring_Buffer.md)**
    - **Title:** Implement Upload Ring Buffer for Streaming Resources
    - **Impact:** Allocator churn, memory fragmentation
    - **Effort:** 1-2 weeks

---

## P2 - Medium Priority

### Performance
16. **[TASK_P2_DX12-FIND-07_Upload_GPU_Wait.md](TASK_P2_DX12-FIND-07_Upload_GPU_Wait.md)**
    - **Title:** Remove Synchronous GPU Waits from Upload Path
    - **Impact:** Eliminates CPU/GPU overlap, poor performance
    - **Effort:** 1-2 days
    - **Dependency:** Works better after TASK_P1_DX12-009

17. **[TASK_P2_DX12-FIND-08_Sampler_Heap_Limits.md](TASK_P2_DX12-FIND-08_Sampler_Heap_Limits.md)**
    - **Title:** Remove 32 Sampler Hard Limit and Support Dynamic Growth
    - **Impact:** Complex scenes limited to 32 samplers vs 2048 spec
    - **Effort:** 1-2 days

18. **[TASK_P2_DX12-FIND-09_Push_Constant_Size.md](TASK_P2_DX12-FIND-09_Push_Constant_Size.md)**
    - **Title:** Increase Push Constant Budget to Match Vulkan
    - **Impact:** Shaders using >64 bytes fail on D3D12
    - **Effort:** 4-6 hours

### Features
19. **[TASK_P2_DX12-FIND-10_Depth_Stencil_Readback.md](TASK_P2_DX12-FIND-10_Depth_Stencil_Readback.md)**
    - **Title:** Implement Depth/Stencil Readback Support
    - **Impact:** Framebuffer depth/stencil readbacks unavailable
    - **Effort:** 1-2 days

20. **[TASK_P2_DX12-FIND-11_GPU_Timers.md](TASK_P2_DX12-FIND-11_GPU_Timers.md)**
    - **Title:** Implement GPU Timestamp Query Support
    - **Impact:** No GPU profiling capability
    - **Effort:** 2-3 days

21. **[TASK_P2_DX12-FIND-12_Cube_Texture_Upload.md](TASK_P2_DX12-FIND-12_Cube_Texture_Upload.md)**
    - **Title:** Implement Cube Texture Upload
    - **Impact:** Cube maps cannot be populated
    - **Effort:** 4-6 hours

---

## P3 - Low Priority

22. **[TASK_P3_DX12-FIND-13_Multi_Draw_Indirect.md](TASK_P3_DX12-FIND-13_Multi_Draw_Indirect.md)**
    - **Title:** Implement Multi-Draw Indirect Support
    - **Impact:** ExecuteIndirect APIs unavailable
    - **Effort:** 1-2 weeks
    - **Dependency:** Requires descriptor fixes first

23. **[TASK_P3_DX12-010_Multi_Threading.md](TASK_P3_DX12-010_Multi_Threading.md)**
    - **Title:** Implement Multi-Threaded Command Recording
    - **Impact:** Cannot leverage multi-core CPUs
    - **Effort:** 2-3 weeks
    - **Future:** Major architectural change

---

## Dependency Graph

```
P0 Foundation Layer:
├─ DX12-001 (PSO Caching) ─┐
├─ DX12-002 (Root Sig Cache) ─┤→ Enables performance optimizations
├─ DX12-FIND-01 (Unbounded Descriptors) ─┐
├─ DX12-FIND-02 (Heap Overflow) ────────┼→ BLOCKS: DX12-FIND-05, DX12-FIND-13
├─ DX12-003 (Resource Leaks) ────────────┤
├─ DX12-004 (HRESULT Validation) ────────┤
└─ DX12-005 (Upload Sync) ───────────────┘

P1 Critical Fixes:
├─ DX12-FIND-03 (Fence Monotonic)
├─ DX12-006 (Exception Handling)
├─ DX12-FIND-04 (Compute CBV) ←─── Depends on: DX12-FIND-02
├─ DX12-FIND-05 (Storage Buffers) ←── Depends on: DX12-FIND-02
├─ DX12-FIND-06 (RGBA Swap) ←───────── URGENT visual bug
├─ DX12-007 (Compute Limits)
├─ DX12-008 (Limit Queries)
└─ DX12-009 (Ring Buffer) ─────────┐
                                    ├─→ Enhances: DX12-FIND-07
P2 Enhancements:                    │
├─ DX12-FIND-07 (Upload Wait) ←────┘
├─ DX12-FIND-08 (Sampler Limits)
├─ DX12-FIND-09 (Push Constants)
├─ DX12-FIND-10 (Depth Readback)
├─ DX12-FIND-11 (GPU Timers)
└─ DX12-FIND-12 (Cube Upload)

P3 Future Work:
├─ DX12-FIND-13 (Multi-Draw) ←──── Depends on: DX12-FIND-02
└─ DX12-010 (Multi-Threading) ←─── Major architectural change
```

---

## Test Validation

**All tasks must pass:**
```bash
# Unit Tests
test_unittests.bat

# Render Sessions
test_all_sessions.bat
```

**Regression Policy:**
- ✅ Green: All tests pass → Proceed
- ❌ Red: Any test fails → STOP, trigger agent analysis
- 🔧 Test changes only allowed for D3D12-specific backend issues
- 🚫 NO changes to test scripts or build directory

---

## Completion Checklist

### P0 (Week 1) - BLOCKING PRODUCTION
- [ ] TASK_P0_DX12-001 - PSO Caching
- [ ] TASK_P0_DX12-002 - Root Signature Caching
- [ ] TASK_P0_DX12-FIND-01 - Unbounded Descriptors
- [ ] TASK_P0_DX12-FIND-02 - Heap Overflow
- [ ] TASK_P0_DX12-003 - Resource Leaks
- [ ] TASK_P0_DX12-004 - HRESULT Validation
- [ ] TASK_P0_DX12-005 - Upload Buffer Sync

### P1 (Weeks 2-3) - CRITICAL FEATURES
- [ ] TASK_P1_DX12-FIND-03 - Schedule Fence
- [ ] TASK_P1_DX12-006 - Device Removal
- [ ] TASK_P1_DX12-FIND-04 - Compute CBV
- [ ] TASK_P1_DX12-FIND-05 - Storage Buffers
- [ ] TASK_P1_DX12-FIND-06 - RGBA Swap **URGENT**
- [ ] TASK_P1_DX12-007 - Compute Limits
- [ ] TASK_P1_DX12-008 - Limit Queries
- [ ] TASK_P1_DX12-009 - Ring Buffer

### P2 (Month 2) - ENHANCEMENTS
- [ ] TASK_P2_DX12-FIND-07 - Upload Wait
- [ ] TASK_P2_DX12-FIND-08 - Sampler Limits
- [ ] TASK_P2_DX12-FIND-09 - Push Constants
- [ ] TASK_P2_DX12-FIND-10 - Depth Readback
- [ ] TASK_P2_DX12-FIND-11 - GPU Timers
- [ ] TASK_P2_DX12-FIND-12 - Cube Upload

### P3 (Future) - NICE TO HAVE
- [ ] TASK_P3_DX12-FIND-13 - Multi-Draw
- [ ] TASK_P3_DX12-010 - Multi-Threading

---

**Estimated Timeline to Production:**
- P0 fixes: 1 week (7 tasks)
- P1 improvements: 2-3 weeks (8 tasks)
- P2 optimizations: 3-4 weeks (6 tasks)
- **Total to Production-Ready:** 6-8 weeks

---

*Last Updated: 2025-11-08*
*Total Issues from Audit: 116 findings (merged into 23 actionable tasks)*
