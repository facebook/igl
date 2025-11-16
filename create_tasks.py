#!/usr/bin/env python3
"""Generate all remaining D3D12 task files"""

import os

backlog_dir = "backlog"

# Task definitions (condensed format for efficiency)
tasks = {
    "T09": ("Refactor CommandQueue submit from 453 to under 100 Lines", "High",
            "Monolithic 453-line submit() handles everything. 10x larger than Vulkan/Metal.",
            "Extract FrameManager, PresentManager, StagingManager. Create FenceWaiter RAII. Target <100 lines.",
            "CommandQueue.cpp/h, new FrameManager, FenceWaiter", "T04, T07",
            "ID3D12CommandQueue, ID3D12Fence, SetEventOnCompletion",
            "All render sessions, presentation, frame pacing"),

    "T10": ("Simplify State Tracking to Per-Resource Fields", "High",
            "Buffer mutable in const. Texture 275-line dual-mode. Vulkan: single field.",
            "Simple per-resource currentState_ field. Remove heterogeneous. Non-const updates only.",
            "Buffer.cpp/h, Texture.cpp/h, D3D12StateTransition.h", "T05, T15",
            "D3D12_RESOURCE_STATES conceptual model",
            "All buffer/texture tests, state transition tests"),

    "T11": ("Create D3D12 Pipeline Builder", "High",
            "700-line monolithic PSO. Hard-coded root sigs. Hard-coded limits.",
            "D3D12PipelineBuilder with fluent interface. Generate root sigs from reflection. Query limits.",
            "New D3D12PipelineBuilder.h/cpp, Device.cpp, PSOs", "T06",
            "ID3D12ShaderReflection, D3D12_GRAPHICS_PIPELINE_STATE_DESC",
            "All pipeline creation, shader reflection tests"),

    "T12": ("Refactor RenderCommandEncoder Constructor (398 to <50 Lines)", "High",
            "398-line constructor does work. Per-encoder RTV/DSV allocation. Vulkan: 20 lines.",
            "Move work to begin(). Cache framebuffers. Target <50 lines.",
            "RenderCommandEncoder.cpp/h", "None",
            "RAII principles", "All render encoder, framebuffer, MRT tests"),

    "T13": ("Normalize Error Macros and Reduce Logging", "High",
            "Double logging. 10x-20x more logs than Vulkan. ~250 vs ~25 calls.",
            "Single log + assert. Macros: IGL_D3D12_PRINT_COMMANDS, DEBUG_VERBOSE. Remove hot-path logs. Target ~25-50 calls.",
            "Common.h, all D3D12 files", "Complements T03",
            "Not needed - library policy", "Log volume reduced 10x, no double-logging"),

    "T14": ("Move Hard-Coded Sizes to Config", "High",
            "Magic numbers: heap sizes, kMaxFramesInFlight=3, 256, 128MB. No rationale.",
            "D3D12ContextConfig struct. Query device limits. Use D3D12 constants. Document rationale.",
            "Common.h, DescriptorHeapManager, UploadRingBuffer, CommandQueue", "None",
            "D3D12_FEATURE_DATA_D3D12_OPTIONS, CheckFeatureSupport",
            "Tests with default and custom configs"),

    "T15": ("Fix Const-Correctness and Static State", "High",
            "Const methods mutate via mutable. Static DXC init not thread-safe.",
            "Remove const from mutating. std::call_once for DXC. Remove static counters.",
            "Buffer.cpp/h, Device.cpp/h, Texture.cpp", "Complements T10",
            "C++ const-correctness, std::call_once", "Thread-safety, no const violations"),

    "T16": ("Implement Unified Logging Controls", "High",
            "250+ always-on logs. INFO spam in hot paths. 10x-20x more than Vulkan.",
            "Macros: IGL_D3D12_PRINT_COMMANDS, DEBUG_VERBOSE. Move 90% behind macros. Keep ERROR/WARNING only. Target ~25-30 calls.",
            "All D3D12 backend files", "None",
            "Not needed", "Normal runs minimal, verbose available when enabled"),

    "T17": ("Replace Custom ComPtr with WRL ComPtr", "Medium",
            "Reimplements Microsoft::WRL::ComPtr. Missing As<>, comparison ops.",
            "Use <wrl/client.h> OR rename to igl::d3d12::ComPtr. Implement missing functionality if custom.",
            "D3D12Headers.h, all D3D12 files", "None",
            "Microsoft::WRL::ComPtr standard smart pointer",
            "Build on all platforms, no COM leaks"),

    "T18": ("Move Library Linking to CMake", "Low",
            "#pragma comment(lib) MSVC-specific. All backends use CMake.",
            "Remove pragmas. Add target_link_libraries in CMakeLists.txt.",
            "D3D12Headers.h, CMakeLists.txt", "None",
            "CMake best practice", "Builds succeed, proper linking"),

    "T19": ("Fix Const in Device and Buffer", "High",
            "Const methods modify mutable: processCompletedUploads(), upload().",
            "Remove const from mutating. Remove mutable. Proper synchronization.",
            "Device.cpp/h, Buffer.cpp/h", "Related to T05, T15",
            "C++ const-correctness", "No const with mutable abuse"),

    "T20": ("Consolidate Descriptor Management", "High",
            "Three systems: per-frame, DescriptorHeapManager, inline. Complex, unclear lifetimes.",
            "D3D12ResourcesBinder abstraction. Centralize allocation/binding. Clear separation.",
            "New D3D12ResourcesBinder, D3D12Context, encoders", "T01, T16",
            "Descriptor heap management patterns", "All binding tests, PIX validation"),

    "T21": ("Simplify or Remove D3D12StateTransition", "Medium",
            "Complex validation, unused logging (60 lines, zero callers). Default 'allow all'.",
            "Option 1: Remove entirely. Option 2: Trim to <50 lines. Delete unused functions.",
            "D3D12StateTransition.h", "T04",
            "COMMON intermediate sufficient per spec", "Correct transitions, no unnecessary barriers"),

    "T22": ("Delete or Minimize ResourceAlignment.h", "Low",
            "Unused placed-resource code. Redefines SDK constants. Inline overkill.",
            "Delete except AlignUp<>, move to Common.h. Use SDK constants directly.",
            "ResourceAlignment.h, Common.h", "None",
            "D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT in d3d12.h",
            "Build succeeds, AlignUp preserved if needed"),

    "T23": ("Fix or Remove UploadRingBuffer", "Medium",
            "May be unnecessary. Atomics under mutex. Complex empty. Hard-coded 128MB. Dead code.",
            "Profile vs simple. If keeping: fix atomics, empty, chunked, configurable. If no benefit: remove.",
            "UploadRingBuffer.cpp/h, Device.cpp", "T07, T08, T26",
            "D3D12_HEAP_TYPE_UPLOAD already CPU-visible",
            "Performance measured before/after"),

    "T24": ("Simplify Texture State Tracking", "Medium",
            "275-line dual-mode. Heterogeneous rarely used. Vulkan: 1 line.",
            "Single field like Vulkan. Remove heterogeneous. Use COMMON when differ.",
            "Texture.cpp/h", "T04, T21",
            "Not needed - simplification", "All texture tests, performance acceptable"),

    "T25": ("Create FenceWaiter RAII Wrapper", "Medium",
            "40+ lines manual event creation. Defensive coding. Vulkan/Metal: simple wait.",
            "RAII class: constructor SetEventOnCompletion, wait() WaitForSingleObject, destructor close. ~40 to ~5 lines.",
            "New FenceWaiter.h/cpp, CommandQueue.cpp", "None",
            "ID3D12Fence::SetEventOnCompletion standard pattern",
            "All fence waits work, no leaks, no hangs"),

    "T26": ("Shared Staging Infrastructure", "Medium",
            "Scattered staging, per-call allocators/waits. More complex than Vulkan.",
            "D3D12ImmediateCommands + D3D12StagingDevice. Unify all upload/readback. Shared fence.",
            "New Immediate/Staging, Buffer, Texture, CommandBuffer, Framebuffer", "T05, T15, T23",
            "Not needed - architectural refactoring",
            "All upload/readback tests, performance same or better"),

    "T27": ("Pre-Allocate Descriptor Pages", "Medium",
            "Dynamic growth mid-frame invalidates descriptors. Dangerous. Vulkan: fail-fast.",
            "Pre-allocate at init. Fail early if exhausted. Configurable. Remove dynamic growth.",
            "CommandBuffer.cpp/h", "T14, T20",
            "Vulkan fail-fast pattern", "No mid-frame allocations, clear errors"),

    "T28": ("Pre-Compile Mipmap Shader", "Low",
            "Runtime HLSL compilation, embedded strings (duplicated). Adds latency.",
            "Pre-compile at device init, store bytecode. Remove embedded strings.",
            "Texture.cpp, Device.cpp", "T06 (must deduplicate first)",
            "Not needed", "No runtime compilation, performance improved"),

    "T29": ("Fix Hard-Coded Alignment Constants", "Low",
            "kUploadAlignment = 256 magic number.",
            "Use D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT. Add comment.",
            "Buffer.cpp (lines 143, 221)", "None",
            "D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT = 256 bytes",
            "All buffer tests pass"),

    "T30": ("Remove Paranoid Validation Macros", "Low",
            "IGL_D3D12_VALIDATE_CPU_HANDLE redundant with debug layer.",
            "Remove paranoid macros. Rely on D3D12 debug layer. Keep logic error assertions only.",
            "Common.h, usage sites", "None",
            "D3D12 Debug Layer (D3D12_CREATE_DEVICE_DEBUG)",
            "Tests pass, debug layer catches errors"),

    "T31": ("Fix Stencil Format Conversion", "Medium",
            "Returns DXGI_FORMAT_UNKNOWN for stencil without error. Silent failure.",
            "Log error + TODO. Optionally: implement via typed subresource views.",
            "Common.cpp (lines 78-79)", "None",
            "DXGI_FORMAT_X24_TYPELESS_G8_UINT for stencil plane",
            "Clear error or proper support, no silent failures"),

    "T32": ("Remove Dead Code", "Low",
            "Empty SamplerState.cpp, unused high-watermark, canAllocate, validateTransition, logging.",
            "Delete SamplerState.cpp (or impl per T33), ResourceAlignment.h, unused tracking/functions. ~150-200 lines.",
            "SamplerState, ResourceAlignment, DescriptorHeapManager, UploadRingBuffer, D3D12StateTransition", "T22, T21, T23",
            "Not needed", "Build succeeds, ~150-200 lines removed"),

    "T33": ("Add SamplerState Hash", "Low",
            "Missing hash() for caching. Empty .cpp.",
            "Implement hash() on all params. Add operator==. Use for cache. Add to .cpp or remove file.",
            "SamplerState.h/cpp, Device.cpp", "None",
            "D3D12_SAMPLER_DESC fields", "Hash uniqueness, cache working"),

    "T34": ("Reduce Device Members (30+ to <15)", "Medium",
            "30+ members: mutexes, caches, stats, telemetry. Vulkan: ~8, Metal: ~6.",
            "Extract managers: PipelineCache, SamplerCache, AllocatorPool. Target <15 members.",
            "Device.cpp/h, new manager files", "None",
            "SOLID principles", "Device <15 members, all tests pass"),

    "T35": ("Create D3D12PipelineBuilder", "High",
            "700-line monolithic creation. Hard-coded root sig/limits. No builder.",
            "Builder with fluent interface. Generate root sig from reflection. Query limits. Target ~100 lines.",
            "New D3D12PipelineBuilder.h/cpp, Device.cpp, PSOs", "None",
            "ID3D12ShaderReflection, D3D12_FEATURE_DATA_D3D12_OPTIONS",
            "All pipeline tests, reflection-based root sigs"),

    "T36": ("Fix Compute Descriptor Hot Path", "High",
            "dispatchThreadGroups allocates descriptors per dispatch. Hot path allocation.",
            "Pre-allocate. Create when bound, not dispatch. Cache tables, reuse if unchanged.",
            "ComputeCommandEncoder.cpp/h", "T20",
            "Not needed - optimization", "Dispatch workloads faster, no per-dispatch alloc"),

    "T37": ("Replace Global UAV Barriers", "Medium",
            "barrier.UAV.pResource = nullptr flushes all. Conservative.",
            "Track UAV writes. Use resource-specific barriers. Measure performance.",
            "ComputeCommandEncoder.cpp/h", "None",
            "D3D12_RESOURCE_BARRIER_TYPE_UAV with specific pResource",
            "Compute tests pass, no races, performance improved"),

    "T38": ("Add Push Constant Support", "Medium",
            "bindBytes allocates buffer every call. High overhead.",
            "<=64 bytes: SetComputeRoot32BitConstants (no alloc). Larger: buffer. Update root sig.",
            "ComputeCommandEncoder.cpp/h, root sig generation", "T35",
            "SetComputeRoot32BitConstants up to 256 bytes",
            "Small bindBytes use root constants, performance improved"),

    "T39": ("Remove Task ID Comments", "Low",
            "P0_DX12-005, B-008 etc. meaningless. Verbose AI comments. Long explanations.",
            "Remove all task IDs. Remove obvious comments. Keep only why explanations. Concise style.",
            "All D3D12 files", "None",
            "Not needed", "Zero task IDs (grep verify), meaningful comments only"),

    "T40": ("Optimize DescriptorHeapManager Locking", "Low",
            "Mutex for read-only checks. 10-50ns overhead per bind.",
            "Use std::atomic<bool> for read-only. Mutex only for write. Or remove validation.",
            "DescriptorHeapManager.cpp/h", "T20 (may remove manager)",
            "Not needed", "Performance improved, thread-safety maintained"),

    "T41": ("Remove Redundant Allocation Tracking", "Low",
            "Free lists AND bitmaps. Doubles memory (20KB for 4096).",
            "Keep free lists, remove bitmaps. Reduce memory 50%.",
            "DescriptorHeapManager.cpp/h", "T20",
            "Not needed", "Memory reduced ~50%, all tests pass"),

    "T42": ("Consolidate Descriptor APIs", "Low",
            "Multiple getXHandle variants. ~800+ lines duplicated validation.",
            "Keep bool-returning versions only. Remove non-bool. Update call sites.",
            "DescriptorHeapManager.cpp/h, usage sites", "T20",
            "Not needed", "Single API, all sites updated"),

    "T43": ("Replace kMaxFramesInFlight", "Low",
            "kMaxFramesInFlight = 3 hard-coded. Manual arithmetic.",
            "Query swapchain GetDesc() for BufferCount. Use queried value.",
            "CommandQueue.cpp, Swapchain.cpp", "None",
            "DXGI_SWAP_CHAIN_DESC::BufferCount",
            "Works with 2, 3, 4 buffers, all presentation tests"),

    "T44": ("Condense Adapter Logging", "Low",
            "7 lines per adapter at INFO. Log spam.",
            "1 line: 'Adapter: [name] (Level X, Memory Y)'. Details behind DEBUG_VERBOSE.",
            "D3D12Context.cpp (lines 631-678)", "T16",
            "Not needed", "1-2 lines normally, verbose available"),

    "T45": ("Clean Up Deprecated Accessor", "Low",
            "cbvSrvUavHeap() marked deprecated via comment, not attribute.",
            "Remove if unused OR use [[deprecated]] attribute.",
            "D3D12Context.cpp/h, usage sites", "None",
            "C++ [[deprecated]] attribute", "Removed OR [[deprecated]], compile warnings if used"),

    "T46": ("Move Struct Logic to CPP", "Low",
            "Nested structs with methods in headers. Header pollution.",
            "Move implementations to .cpp. Reduce header complexity, compile time.",
            "D3D12Context.h/cpp (lines 92-138)", "None",
            "C++ best practice", "Implementations in .cpp, compile time improved"),

    "T47": ("Gate Debug Diagnostics", "Low",
            "logInfoQueueMessages, logDredInfo always on. Verbose.",
            "Guard behind IGL_D3D12_DEBUG or DIAGNOSTICS. Off normally.",
            "CommandQueue.cpp, diagnostic code", "T16",
            "Not needed", "Diagnostics off normally, available when debugging"),

    "T48": ("Remove Destructor Logging", "Low",
            "60k+ messages/sec at 60fps. Refcount logging. Excessive.",
            "Remove destructor logs OR gate #ifdef IGL_DEBUG + verbosity. Remove refcount logs.",
            "Buffer.cpp (line 77), Texture.cpp, destructors", "T16",
            "Not needed", "Log volume reduced, performance improved"),

    "T49": ("Fix Texture Recreation Validation", "Medium",
            "generateMipmap recreates texture without validating data preservation.",
            "Option 1: Fail early if flags missing. Option 2: Document + validate preservation. Option 3: Blit without recreation.",
            "Texture.cpp (lines 528-643)", "T06 (mipmap dedup)",
            "Not needed - design decision", "Clear error or documented, no silent data loss"),

    "T50": ("Remove Static Call Counters", "Low",
            "Log first N pattern. Not thread-safe. Wrong approach.",
            "Remove all static counters. Use macro-gated logging. Ensure thread-safety.",
            "Device.cpp (589-596), RenderCommandEncoder.cpp, etc.", "T16",
            "Not needed", "Zero static counters (grep verify), proper controls"),
}

# Generate task files
for task_id, (title, priority, problem, solution, files, depends, ms_docs, verification) in tasks.items():
    filename = f"{task_id}_{title.replace(' ', '_').replace('/', '_').replace(':', '')[:50]}.md"
    filepath = os.path.join(backlog_dir, filename)

    content = f"""# {task_id} – {title}

## Priority
**{priority}**

## Problem Summary
{problem}

## Proposed Solution / Approach
{solution}

## Scope / Impact

### Files
{files}

## Dependencies
{depends}

## Microsoft Docs Reference
{ms_docs}

## Acceptance Criteria

### Functional
- [ ] Implementation complete per proposed solution
- [ ] Code quality improved
- [ ] No regressions

### Testing (per VERIFICATION_COMBINED.md)
- [ ] **Targeted verification**: {verification}
- [ ] **Mandatory gate**: Execute `cmd /c mandatory_all_tests.bat` from repo root
  - [ ] Zero test failures in render sessions (`artifacts\\mandatory\\render_sessions.log`)
  - [ ] Zero test failures in unit tests (`artifacts\\mandatory\\unit_tests.log`)
  - [ ] No GPU validation/DRED errors in logs
  - [ ] All render sessions show "Frame 0" + "Signaled fence" markers
- [ ] **Artifacts inspection**:
  - Review `artifacts\\last_sessions.log` for render session behavior
  - Review `artifacts\\unit_tests\\D3D12\\failed_tests.txt` (should be empty)
  - Review `artifacts\\unit_tests\\D3D12\\IGLTests.log` for warnings

## Implementation Notes
- Follow proposed solution step-by-step
- Test incrementally
- Document any deviations from plan
- Ensure cross-backend consistency where applicable

## Related Tasks
See dependencies and ALL_TASKS_REFERENCE.md for task relationships
"""

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

    print(f"Created {filename}")

print(f"\n✓ All {len(tasks)} task files created successfully!")
