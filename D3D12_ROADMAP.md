# IGL D3D12 Backend - Implementation Roadmap

**Last Updated:** 2025-11-01
**Current Status:** Production Ready (85.7% test pass rate, Grade B+)
**Quick Links:** [Status Report](docs/D3D12_STATUS.md) | [API Conformance](docs/D3D12_API_CONFORMANCE_REPORT.md) | [Action Items](docs/D3D12_CONFORMANCE_ACTION_ITEMS.md)

---

## Overview

This roadmap defines the remaining work for the IGL D3D12 backend in two categories:

1. **Task 1: Remaining Implementation Work** - Critical fixes and missing features
2. **Task 2: Future Enhancements** - Performance optimizations and advanced features

All tasks are prioritized, estimated, and linked to technical documentation for autonomous agent execution.

---

# TASK 1: Remaining Implementation Work

## Critical Priority (Must Complete)

### 1.1 Add TQMultiRenderPassSession D3D12 Shaders
**Status:** ❌ Not Started
**Effort:** 2-4 hours
**Priority:** HIGH
**Impact:** Completes render session test coverage (18/21 → 19/21)

**Description:**
TQMultiRenderPassSession currently aborts with "Code should NOT be reached" because D3D12 shaders are not implemented.

**Files to Modify:**
- `shell/renderSessions/TQMultiRenderPassSession.cpp:133`

**Implementation Steps:**
1. Read existing Vulkan/OpenGL shader code
2. Write equivalent D3D12 HLSL shaders
3. Add to `getShaderStagesForBackend()` function
4. Test with screenshot mode

**Success Criteria:**
- Session runs without abort
- Screenshot generated successfully
- No D3D12 validation errors

---

### 1.2 Remove Redundant SetDescriptorHeaps Calls
**Status:** ❌ Not Started
**Effort:** 1 hour
**Priority:** HIGH
**Impact:** 1-5% draw call overhead reduction

**Description:**
SetDescriptorHeaps is called in both constructor AND every draw call, causing unnecessary overhead.

**Files to Modify:**
- `src/igl/d3d12/RenderCommandEncoder.cpp:796,858`

**Success Criteria:**
- All 18 sessions still pass
- PIX shows SetDescriptorHeaps called once per encoder
- 1-5% FPS improvement

**References:**
- D3D12_CONFORMANCE_ACTION_ITEMS.md #2

---

### 1.3 Implement Per-Frame Fencing
**Status:** ❌ Not Started
**Effort:** 2-3 days
**Priority:** HIGH
**Impact:** 20-40% FPS improvement

**Description:**
Replace synchronous GPU wait with ring buffer and per-frame fences.

**Files to Modify:**
- `src/igl/d3d12/CommandQueue.cpp:100-114`
- `src/igl/d3d12/D3D12Context.h/cpp`

**Success Criteria:**
- No device removal in 1000-frame tests
- 20-40% FPS increase in GPUStressSession
- PIX shows 90%+ GPU utilization

**References:**
- D3D12_CONFORMANCE_ACTION_ITEMS.md #1

---

## Medium Priority

### 1.4 Fix ComputeSession Visual Output
**Status:** ⚠️ Needs Investigation
**Effort:** 4-8 hours
**Impact:** Demonstrates compute shader functionality

---

### 1.5 Upgrade to Root Signature v1.1
**Status:** ❌ Not Started
**Effort:** 4-8 hours
**Impact:** Driver optimizations

**References:**
- D3D12_CONFORMANCE_ACTION_ITEMS.md #3

---

### 1.6 Migrate to DXC Compiler
**Status:** ❌ Not Started
**Effort:** 1-2 weeks
**Impact:** Unlocks Shader Model 6.0+ features

**References:**
- D3D12_CONFORMANCE_ACTION_ITEMS.md #4

---

### 1.7 Add YUV Texture Format Support
**Status:** ❌ Not Started
**Effort:** 8-16 hours
**Priority:** LOW
**Impact:** Enables YUVColorSession

---

# TASK 2: Future Enhancements

## Performance Optimizations

### 2.1 Descriptor Suballocator
**Effort:** 1-2 weeks
**Impact:** 50% descriptor overhead reduction

### 2.2 Resource Pooling
**Effort:** 1-2 weeks
**Impact:** 80-90% allocation reduction

### 2.3 Async Texture Upload Queue
**Effort:** 2-3 weeks
**Impact:** Zero CPU wait for uploads

---

## Advanced Features

### 2.4 GPU-Based Validation
**Effort:** 4 hours
**Impact:** Development tool

### 2.5 DRED Integration
**Effort:** 8 hours
**Impact:** Better crash diagnostics

### 2.6 PIX Markers
**Effort:** 4 hours
**Impact:** Better profiling

### 2.7 Indirect Draw Support
**Effort:** 2-3 weeks
**Impact:** GPU-driven rendering

### 2.8 Query Support
**Effort:** 1-2 weeks
**Impact:** Occlusion queries

### 2.9 Shader Debug Compilation
**Effort:** 4 hours
**Impact:** Development tool

### 2.10 Static Samplers
**Effort:** 4 hours
**Impact:** 5-10% performance
**Blocked by:** Task 1.5

---

## Testing

### 2.11 Performance Benchmark Suite
**Effort:** 1 week
**Impact:** Measurable performance tracking

### 2.12 Memory Leak Detection
**Effort:** 1 week
**Status:** ⚠️ Partial

---

# Summary Tables

## Task 1: Remaining Implementation (7 items)
| Task | Effort | Impact | Status |
|------|--------|--------|--------|
| 1.1 TQMultiRenderPassSession | 2-4h | Complete coverage | ❌ |
| 1.2 Remove redundant SetDescriptorHeaps | 1h | 1-5% perf | ❌ |
| 1.3 Per-frame fencing | 2-3d | 20-40% perf | ❌ |
| 1.4 ComputeSession visual | 4-8h | Demo | ⚠️ |
| 1.5 Root signature v1.1 | 4-8h | Optimizations | ❌ |
| 1.6 DXC migration | 1-2w | SM 6.0+ | ❌ |
| 1.7 YUV support | 8-16h | Video | ❌ |

## Task 2: Future Enhancements (12 items)
| Task | Effort | Impact | Status |
|------|--------|--------|--------|
| 2.1-2.3 Performance | 4-7w | Major perf gains | ❌ |
| 2.4-2.10 Features | 3-6w | Advanced features | ❌/⚠️ |
| 2.11-2.12 Testing | 2w | Quality | ⚠️ |

---

**For Details:** See full descriptions in sections above or refer to technical reports in `docs/`.
