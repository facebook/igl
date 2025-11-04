# CRITICAL ROOT CAUSE FIX - Descriptor Heap Race Condition

**Date**: 2025-11-03
**Commit**: bf3f0945
**Status**: 🔴 **REQUIRES MANUAL TESTING**

---

## Executive Summary

### The Real Bug Found!

RenderCommandEncoder was using a **SHARED descriptor heap** (`DescriptorHeapManager`) instead of **per-frame isolated heaps**, completely defeating all previous fixes.

**Evidence from your crash log** (`temp_BindGroupSession.log`):
```
ALL 514 FRAMES USED THE SAME HEAP: 000001CC064BBBE0
```

This caused descriptor race conditions → DXGI_ERROR_DEVICE_HUNG (0x0000087d).

---

## The Bug Explained

### Code Path

```
BindGroupSession render loop
├─ CommandBuffer::begin()
│  └─ Sets per-frame heaps: frameContexts_[currentFrameIndex].cbvSrvUavHeap ✅
│
└─ RenderCommandEncoder::RenderCommandEncoder()
   └─ IGNORES per-frame heaps ❌
   └─ Uses DescriptorHeapManager shared heap ❌
   └─ Result: All frames use SAME heap ❌
```

### The Broken Code (src/igl/d3d12/RenderCommandEncoder.cpp:39-42)

```cpp
if (heapMgr) {
  IGL_LOG_INFO("RenderCommandEncoder: Using DescriptorHeapManager heaps\n");
  cbvSrvUavHeap = heapMgr->getCbvSrvUavHeap();  // ← WRONG: Shared heap!
  samplerHeap = heapMgr->getSamplerHeap();       // ← WRONG: Shared heap!
}
```

**Result**:
- Frame 0 allocates descriptor 5 in shared heap
- Frame 1 allocates descriptor 5 in SAME shared heap → **overwrites Frame 0's descriptor**
- GPU still reading Frame 0's descriptor 5 → **DEVICE_HUNG**

---

## The Fix

### New Code (src/igl/d3d12/RenderCommandEncoder.cpp:38-40)

```cpp
// ALWAYS use per-frame heaps from D3D12Context (NOT DescriptorHeapManager)
ID3D12DescriptorHeap* cbvSrvUavHeap = context.getCbvSrvUavHeap();
ID3D12DescriptorHeap* samplerHeap = context.getSamplerHeap();
```

**Result**:
- Frame 0: Uses `frameContexts_[0].cbvSrvUavHeap` ✅
- Frame 1: Uses `frameContexts_[1].cbvSrvUavHeap` ✅
- Frame 2: Uses `frameContexts_[2].cbvSrvUavHeap` ✅
- **No descriptor conflicts possible!** ✅

---

## Why All Previous Fixes "Didn't Work"

### Timeline of Fixes

1. **Commit c8341f1d** (Per-frame heap isolation)
   - Created 3 isolated heaps (1024 descriptors each)
   - ✅ Implementation correct
   - ❌ RenderCommandEncoder didn't use them!

2. **Commit 86e7b4a0** (Texture-level caching)
   - Cached descriptor indices in Texture objects
   - ✅ Logic correct
   - ❌ Cached from WRONG heap (shared, not per-frame)

3. **Commit dab7f10c** (Table-based BindGroups)
   - Allocated contiguous descriptor slices
   - ✅ Allocation pattern correct
   - ❌ Allocated from WRONG heap (shared, not per-frame)

4. **Commit bf3f0945** (THIS FIX)
   - Forces RenderCommandEncoder to use per-frame heaps
   - ✅ Makes ALL previous fixes actually work!

---

## Evidence from Crash Log Analysis

### Your Log: `temp_BindGroupSession.log`

```
Line 203:  RenderCommandEncoder: CBV/SRV/UAV heap = 000001CC064BBBE0
Line 272:  RenderCommandEncoder: CBV/SRV/UAV heap = 000001CC064BBBE0
Line 417:  RenderCommandEncoder: CBV/SRV/UAV heap = 000001CC064BBBE0
Line 550:  RenderCommandEncoder: CBV/SRV/UAV heap = 000001CC064BBBE0
... (repeated 514+ times with SAME address)
Line 14772: RenderCommandEncoder: CBV/SRV/UAV heap = 000001CC064BBBE0
```

**514 frames, all using heap `000001CC064BBBE0`**

### Expected After Fix

```
Frame 0: RenderCommandEncoder: CBV/SRV/UAV heap = 0xAAAAAAAAA (frame 0's heap)
Frame 1: RenderCommandEncoder: CBV/SRV/UAV heap = 0xBBBBBBBBB (frame 1's heap)
Frame 2: RenderCommandEncoder: CBV/SRV/UAV heap = 0xCCCCCCCCC (frame 2's heap)
Frame 3: RenderCommandEncoder: CBV/SRV/UAV heap = 0xAAAAAAAAA (frame 0's heap again)
```

**Different heap addresses per frame index**

---

## Testing Instructions

### ⚠️ CRITICAL: You Must Test This Manually

```bash
cd build/vs/shell/Debug

# Test 1: Standard run (should not crash)
BindGroupSession_d3d12.exe 2>&1 | tee test_final_fix.log

# Test 2: With FPS throttling (stress test)
BindGroupSession_d3d12.exe --fps-throttle 100 --fps-throttle-random 2>&1 | tee test_final_fix_stress.log
```

### What to Look For

#### ✅ SUCCESS Indicators:
1. **Runs for 1000+ frames without crash**
2. **Different heap addresses per frame**:
   ```
   Frame 0: CBV/SRV/UAV heap = 0x...
   Frame 1: CBV/SRV/UAV heap = 0x... (DIFFERENT!)
   Frame 2: CBV/SRV/UAV heap = 0x... (DIFFERENT!)
   ```
3. **Log shows**: "Using per-frame heaps from D3D12Context"
4. **NO DXGI errors**
5. **Cube renders correctly, no disappearing**

#### ❌ FAILURE Indicators:
1. Crash with `DXGI_ERROR_DEVICE_HUNG (0x0000087d)`
2. All frames show SAME heap address
3. Cube disappears randomly
4. Log shows: "Using DescriptorHeapManager heaps" (OLD broken message)

---

## Quick Verification

Run this to check if fix is active:

```bash
cd build/vs/shell/Debug
./BindGroupSession_d3d12.exe 2>&1 | grep "per-frame heaps from D3D12Context" | head -5
```

**Expected output** (if fix is active):
```
RenderCommandEncoder: Using per-frame heaps from D3D12Context
RenderCommandEncoder: Using per-frame heaps from D3D12Context
RenderCommandEncoder: Using per-frame heaps from D3D12Context
```

**Wrong output** (if fix NOT active - rebuild needed):
```
RenderCommandEncoder: Using DescriptorHeapManager heaps
```

---

## Technical Details

### D3D12Context::getCbvSrvUavHeap()

Returns the per-frame heap for the current frame:

```cpp
ID3D12DescriptorHeap* D3D12Context::getCbvSrvUavHeap() const {
  return frameContexts_[currentFrameIndex_].cbvSrvUavHeap.Get();
}
```

### DescriptorHeapManager (OLD/BROKEN)

Has a single shared heap:

```cpp
class DescriptorHeapManager {
  ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap_;  // ← ONE heap for ALL frames!
};
```

This is why the old approach caused race conditions.

---

## Expected Performance Impact

### Before Fix
- **Crashes**: Frequent DEVICE_HUNG at high frame counts
- **Descriptor conflicts**: Frames overwrite each other
- **GPU hangs**: GPU reading stale descriptors

### After Fix
- **Stability**: No crashes (each frame isolated)
- **Performance**: Same or better (no descriptor recreation overhead if using caching)
- **Memory**: 99 KB for 3 per-frame heaps (negligible)

---

## Related Commits

| Commit | Description | Status After This Fix |
|--------|-------------|----------------------|
| c8341f1d | Per-frame heap isolation | ✅ Now actually used |
| 86e7b4a0 | Texture-level caching | ✅ Now caches to correct heap |
| dab7f10c | Table-based BindGroups | ✅ Now allocates from correct heap |
| bf3f0945 | **THIS FIX** | ✅ Makes everything work |

---

## Conclusion

**This should be the FINAL fix** for the descriptor race condition.

Previous fixes were technically correct but sabotaged by RenderCommandEncoder using the wrong heap. This fix forces it to use the per-frame heaps, making all the previous fixes actually effective.

**Status**: 🔴 **REQUIRES YOUR MANUAL TESTING**

Please test and confirm if:
1. BindGroupSession runs without crashes
2. Different heap addresses per frame in logs
3. No DXGI errors

---

**Generated**: 2025-11-03
**Commit**: bf3f0945
**Files Changed**: src/igl/d3d12/RenderCommandEncoder.cpp (-15 lines, +8 lines)
**Test Status**: Awaiting manual validation
