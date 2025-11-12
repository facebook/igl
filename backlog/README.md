# D3D12 Backend Backlog

This directory contains task specifications for **unimplemented** D3D12 backend improvements.

## Status Overview

### Completed Tasks (Moved to Git History)
- **P0 Critical:** 8 of 11 completed (73%)
  - ✅ C-001: Descriptor heap overflow
  - ✅ C-003: FXC fallback
  - ✅ C-006: Excessive logging
  - ✅ C-007: DXIL reflection
  - ✅ C-008: Debug-only validation
  - ✅ C-009: DXGI debug flag
  - ✅ COD-001: Swapchain resize
  - ✅ COD-004: Upload fence mismatch

- **P1 High Priority:** 5 of 20 completed (25%)
  - ✅ H-007: Descriptor bounds checking (already in C-001)
  - ✅ H-008: Shader bytecode validation
  - ✅ H-010: Shader model query
  - ✅ H-011: Complete DRED configuration
  - ✅ H-013: PSO cache thread-safety

### Skipped Tasks (Too Complex)
See [backlog/skipped/](skipped/) directory:
- C-002: Pipeline Library (5 days)
- C-004: Async readback (3 days)
- C-005: Mipmap fix (3 days)

---

## Remaining Tasks (15 Total)

### Quick Wins (1-2 days each) - 4 tasks

1. **[H-004_unbounded_allocator_pool.md](H-004_unbounded_allocator_pool.md)** (1 day)
   - Cap command allocator pool at 64
   - Prevent memory leak during upload spikes

2. **[H-009_missing_shader_model_detection.md](H-009_missing_shader_model_detection.md)** (1 day)
   - Use H-010's query for compilation targets
   - Dynamic SM 5.1/6.0/6.6 selection

3. **[H-014_swapchain_resize_exception.md](H-014_swapchain_resize_exception.md)** (1 day)
   - Graceful fallback on resize failure
   - Swapchain recreation support

4. **[DX12-COD-003_cube_array_wrong_layers.md](DX12-COD-003_cube_array_wrong_layers.md)** (1 day)
   - Fix `DepthOrArraySize` for cube arrays
   - Multiply by `numLayers * 6`

### Medium Complexity (2-3 days each) - 7 tasks

5. **[H-003_texture_state_tracking_race.md](H-003_texture_state_tracking_race.md)** (2 days)
   - Add mutex/atomic for state tracking
   - Fix multi-threaded barrier race

6. **[H-005_missing_uav_barriers.md](H-005_missing_uav_barriers.md)** (2 days)
   - Add `uavBarrier()` API
   - Compute→render synchronization

7. **[H-006_root_signature_bloat.md](H-006_root_signature_bloat.md)** (2 days)
   - Reduce from 53 to <48 DWORDs
   - Convert root CBVs to descriptor tables

8. **[H-012_descriptor_creation_redundancy.md](H-012_descriptor_creation_redundancy.md)** (2 days)
   - Cache texture→descriptor mappings
   - Save 100-200μs per frame

9. **[DX12-COD-002_root_signature_v10_only.md](DX12-COD-002_root_signature_v10_only.md)** (2 days)
   - Serialize as v1.1 when available
   - Add descriptor range flags

10. **[DX12-COD-005_cbv_64kb_violation.md](DX12-COD-005_cbv_64kb_violation.md)** (2 days)
    - Enforce 64 KB CBV limit
    - Respect requested buffer ranges

11. **[DX12-COD-006_structured_buffer_stride.md](DX12-COD-006_structured_buffer_stride.md)** (2 days)
    - Add `elementStride` to BufferDesc
    - Fix hard-coded 4-byte stride

### High Complexity (5-7 days each) - 3 tasks

12. **[H-001_no_multi_queue_support.md](H-001_no_multi_queue_support.md)** (7 days)
    - Create compute and copy queues
    - Cross-queue synchronization
    - **Impact:** 20% GPU utilization gain

13. **[H-002_no_parallel_command_recording.md](H-002_no_parallel_command_recording.md)** (3 days)
    - Atomic descriptor counters
    - Thread-local allocators
    - **Impact:** 40-60% CPU time reduction

14. **[H-015_hardcoded_limits.md](H-015_hardcoded_limits.md)** (2 days)
    - Query device capabilities
    - Remove hard-coded constants

---

## Task Categories

**By Priority:**
- High Impact: H-001, H-002, H-012
- Debug Layer Compliance: COD-002, COD-005, COD-006
- Robustness: H-003, H-004, H-005, H-014
- Feature Parity: H-009, H-015, COD-003

**By Complexity:**
- Quick (1-2 days): 4 tasks
- Medium (2-3 days): 7 tasks
- Complex (5-7 days): 3 tasks

**Total Estimated Effort:** ~30-35 days (6-7 weeks)

---

## Implementation Notes

Each task file contains:
- Problem statement with impact analysis
- Technical details with code examples
- Location guidance (files, line numbers)
- Microsoft documentation references
- Step-by-step implementation guidance
- Testing requirements
- Definition of done checklist

Refer to individual markdown files for complete specifications.
