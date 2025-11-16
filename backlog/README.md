# D3D12 Backend Refactoring - Task Backlog

## Overview
This folder contains the comprehensive backlog of tasks for refactoring and improving the D3D12 backend based on a unified code review analysis.

## Documents

### Core References
- **[VERIFICATION_COMBINED.md](VERIFICATION_COMBINED.md)** – Authoritative verification guide for all tasks
  - Mandatory test harness: `cmd /c mandatory_all_tests.bat`
  - Targeted verification per task
  - Artifact inspection procedures

- **[ALL_TASKS_REFERENCE.md](ALL_TASKS_REFERENCE.md)** – Complete reference for all 50 tasks
  - Condensed problem/solution/scope for each task
  - Dependencies graph
  - Microsoft Docs findings
  - Quick lookup for task details

- **[TASK_GENERATION_STATUS.md](TASK_GENERATION_STATUS.md)** – Task file generation tracking
  - Template for individual task files
  - Completion status
  - Organization by priority and category

### Source Analysis
- **[../D3D12_UNIFIED_CODE_REVIEW.md](../D3D12_UNIFIED_CODE_REVIEW.md)** – Original comprehensive code review
  - Merged findings from two independent reviews
  - ~130+ issues identified across 44 files
  - ~800 lines of duplicated code cataloged
  - Cross-backend consistency analysis

- **[../D3D12_BACKEND_TASKS.md](../D3D12_BACKEND_TASKS.md)** – Original 18 tasks
  - Now superseded by expanded 50-task analysis
  - Integrated into new task set

## Task Files (Individual)

### Created (6/50)
- ✅ [T01_Fix_Descriptor_Root_Binding.md](T01_Fix_Descriptor_Root_Binding.md) – **CRITICAL**
- ✅ [T02_Fix_GPU_Timer_Implementation.md](T02_Fix_GPU_Timer_Implementation.md) – **CRITICAL**
- ✅ [T03_Replace_Exceptions_With_Result.md](T03_Replace_Exceptions_With_Result.md) – **CRITICAL**
- ✅ [T04_Fix_State_Transition_Rules.md](T04_Fix_State_Transition_Rules.md) – **HIGH**
- ✅ [T05_Fix_Async_Upload_Race.md](T05_Fix_Async_Upload_Race.md) – **HIGH**
- ✅ [T06_Deduplicate_Code.md](T06_Deduplicate_Code.md) – **HIGH**

### To Be Created (44)
Refer to [ALL_TASKS_REFERENCE.md](ALL_TASKS_REFERENCE.md) for details on all remaining tasks.

Use [TASK_GENERATION_STATUS.md](TASK_GENERATION_STATUS.md) template to create additional task files as needed.

## Task Summary

### By Priority
- **Critical (13)**: Correctness bugs, must be done immediately
- **High (15)**: Major improvements, deduplication, architecture
- **Medium (17)**: Quality improvements, simplification
- **Low (5)**: Polish, cleanup

### By Category
- **Correctness (10)**: T01-T05, T15, T19, T31, T49, T50
- **Deduplication (5)**: T06-T10
- **Architecture (8)**: T08, T11, T20, T26, T34, T35
- **Simplification (9)**: T09, T12, T21-T24, T27
- **Cleanup (18)**: T13, T14, T16-T18, T28-T33, T36-T48

### Key Wins
- **~800 lines of code eliminated** via deduplication (T06-T10)
- **Monolithic functions refactored**: 453-line submit() → <100, 398-line constructor → <50
- **Cross-backend consistency**: Exception handling, logging, error patterns aligned
- **Microsoft Docs grounded**: All uncertain questions researched and answered definitively

## Verification Process

### Every Task Must Pass

1. **Targeted Verification** (task-specific)
   - Relevant unit tests
   - Specific render session tests
   - Feature-specific validation

2. **Mandatory Full Harness**
   ```bash
   cmd /c mandatory_all_tests.bat
   ```

3. **Passing Criteria**
   - Zero render session failures
   - Zero unit test failures
   - No GPU validation/DRED errors
   - All render sessions show "Frame 0" + "Signaled fence"

4. **Artifact Inspection**
   - `artifacts\mandatory\render_sessions.log`
   - `artifacts\mandatory\unit_tests.log`
   - `artifacts\unit_tests\D3D12\failed_tests.txt` (should be empty)

See [VERIFICATION_COMBINED.md](VERIFICATION_COMBINED.md) for complete details.

## How to Use This Backlog

### For Implementers
1. Pick a task file (start with Critical priority)
2. Read Problem Summary and Proposed Solution
3. Review Microsoft Docs Reference for API details
4. Implement following step-by-step approach
5. Run targeted verification for the specific task
6. Run full mandatory harness: `cmd /c mandatory_all_tests.bat`
7. Inspect artifacts for any failures
8. Only commit if all tests pass

### For Task File Creation
1. Use template from [TASK_GENERATION_STATUS.md](TASK_GENERATION_STATUS.md)
2. Pull details from [ALL_TASKS_REFERENCE.md](ALL_TASKS_REFERENCE.md)
3. Ensure all sections filled:
   - Problem Summary
   - Proposed Solution (step-by-step)
   - Scope/Impact with file paths
   - Dependencies
   - Microsoft Docs Reference (if applicable)
   - Full Acceptance Criteria
   - Testing requirements per VERIFICATION_COMBINED.md
4. Include verification checklist
5. Name file `T##_Short_Title.md`

### For Orchestration
Tasks have dependencies (see ALL_TASKS_REFERENCE.md dependency graph):
- Do T01-T06 first (critical + foundational)
- T07-T11 build on early tasks
- T20+ are incremental improvements

Some tasks can run in parallel if no dependencies.

## Microsoft Documentation Integration

All tasks with uncertain questions have been researched against official Microsoft D3D12 documentation:

### Key Findings
1. **State Transitions** (T04): Write-to-write transitions require intermediate state (COMMON or read state)
2. **Timer Implementation** (T02): Timestamps are bottom-of-pipe, results only valid after fence signals
3. **Upload Synchronization** (T05): Map doesn't block, explicit fence sync mandatory
4. **Descriptor Binding** (T01): SetDescriptorHeaps + SetGraphicsRootDescriptorTable required before draw
5. **Push Constants** (T38): SetComputeRoot32BitConstants supports up to 256 bytes
6. **Alignment** (T29): D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT = 256 bytes

Each task file includes specific API references where applicable.

## Success Metrics

Upon completion of all tasks:
- ✅ Zero critical correctness bugs
- ✅ ~800+ lines of duplicated code eliminated
- ✅ Logging volume reduced 10x to match Vulkan/Metal
- ✅ Error handling consistent across backends (no exceptions)
- ✅ Descriptor management unified and clear
- ✅ State tracking simplified (single field like Vulkan)
- ✅ All functions <100 lines (no 400+ line monsters)
- ✅ All hard-coded constants replaced with config/SDK values
- ✅ 100% test pass rate maintained throughout

## Contact / Questions

For questions about:
- **Task content**: Refer to ALL_TASKS_REFERENCE.md and original code review
- **Verification**: See VERIFICATION_COMBINED.md
- **Dependencies**: Check dependency graph in ALL_TASKS_REFERENCE.md
- **Microsoft Docs**: Check MS Docs Reference section in each task
