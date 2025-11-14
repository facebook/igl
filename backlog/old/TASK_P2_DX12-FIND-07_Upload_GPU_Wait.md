# TASK_P2_DX12-FIND-07: Remove Synchronous GPU Waits from Upload Path

**Priority:** P2 - Medium
**Estimated Effort:** 1-2 days
**Status:** Open
**Issue ID:** DX12-FIND-07 (from Codex Audit)

---

## Problem Statement

Every buffer upload to DEFAULT heap calls `ctx_->waitForGPU()`, forcing a full GPU pipeline flush and eliminating all CPU/GPU parallelism. This causes severe performance degradation in upload-heavy scenarios.

### Current Behavior
- `Buffer::upload()` calls `waitForGPU()` after each upload
- GPU must complete all work before upload proceeds
- CPU blocked waiting for GPU
- Zero CPU/GPU overlap

### Expected Behavior
- Async uploads using fence tracking
- CPU continues while GPU processes upload
- Staging buffers retired when GPU completes
- Maintains CPU/GPU parallelism

---

## Evidence and Code Location

**File:** `src/igl/d3d12/Device.cpp`

**Search Pattern:**
Find around line 312-365:
```cpp
void Buffer::upload(...) {
    // ... copy to staging ...
    // ... record copy command ...

    ctx_->waitForGPU();  // ← PROBLEM: Blocks entire GPU queue
}
```

**Search Keywords:**
- `waitForGPU`
- Buffer upload
- DEFAULT heap upload

---

## Impact

**Severity:** Medium - Performance
**Performance:** Eliminates CPU/GPU overlap
**Throughput:** Poor performance on streaming workloads

---

## Official References

### Microsoft Documentation
1. **Asynchronous Uploads** - [Uploading Resources](https://learn.microsoft.com/en-us/windows/win32/direct3d12/uploading-resources)
   - Use fences to track completion, don't block

2. **Fence-Based Resource Management** - Best practice pattern

---

## Implementation Guidance

### High-Level Approach

1. **Replace waitForGPU() with Fence Tracking**
   - Signal fence after upload command
   - Track staging buffer lifetime with fence value
   - Retire staging buffer when fence signaled

2. **Staging Buffer Pool** (works with TASK_P1_DX12-009)
   - Use ring buffer or pool
   - Fence-based retirement

### Example Fix

Before (BLOCKING):
```cpp
ctx_->waitForGPU();  // Blocks!
```

After (ASYNC):
```cpp
uint64_t fenceValue = signalFence();
trackStagingBuffer(stagingBuffer, fenceValue);
// CPU continues immediately
```

---

## Testing Requirements

```bash
test_unittests.bat
test_all_sessions.bat
```

### Performance Validation
- Measure upload performance (should improve significantly)
- Check frame times in upload-heavy scenarios
- Verify no performance regression

---

## Success Criteria

- [ ] `waitForGPU()` removed from upload path
- [ ] Fence-based async upload implemented
- [ ] All tests pass
- [ ] Performance improvement measurable
- [ ] No upload data corruption
- [ ] User confirms performance gain

---

## Dependencies

**Works Better After:**
- TASK_P1_DX12-009 (Upload Ring Buffer) - Provides infrastructure for async uploads

**Related:**
- TASK_P0_DX12-005 (Upload Buffer Sync) - Fence tracking foundation

---

**Estimated Timeline:** 1-2 days
**Risk Level:** Medium
**Validation Effort:** 3-4 hours

---

*Task Created: 2025-11-08*
*Issue Source: DX12 Codex Audit - Finding 07*
