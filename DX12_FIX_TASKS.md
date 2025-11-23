# DX12 Fix Tasks (Current Audit)

Each task below corresponds to a finding in `CODE_QUALITY_AUDIT.md`. Every task includes file/line pointers, a problem summary, relevant validation/test evidence where applicable, official Microsoft references, a minimal patch plan, and explicit acceptance criteria.

---

## Task Q1 – Root Signatures: Version 1.0 vs Unbounded Ranges

- **Files / Lines**
  - `src/igl/d3d12/Device.cpp:969-972`, `1296`
  - `src/igl/d3d12/D3D12PipelineCache.h:183-190`
  - `src/igl/d3d12/D3D12PipelineBuilder.cpp:574-599`
  - `src/igl/d3d12/D3D12Context.cpp:560-572`
- **Problem Summary**
  - Compute and graphics root signatures set descriptor ranges to `UINT_MAX` (`uavBound`, `srvBound`, `cbvBound`, `samplerBound`) when `D3D12_RESOURCE_BINDING_TIER` is Tier‑2/3, but all root signatures are serialized with `D3D_ROOT_SIGNATURE_VERSION_1` and a `D3D12_ROOT_SIGNATURE_DESC`. The device’s `highestRootSignatureVersion_` (queried via `D3D12_FEATURE_ROOT_SIGNATURE`) is never consulted when choosing the serialization version or descriptor range pattern.
- **Failing Rule / Message**
  - No explicit D3D12 debug‑layer message captured in this run, but this pattern conflicts with the Root Signature 1.1 guidance that unbounded ranges (`NumDescriptors = UINT_MAX`) are a v1.1 feature and should be expressed via `D3D12_VERSIONED_ROOT_SIGNATURE_DESC` and `D3D12_DESCRIPTOR_RANGE1` with proper binding‑tier gating.
- **Official Microsoft References**
  - Root signatures (including v1.1):  
    `https://learn.microsoft.com/windows/win32/direct3d12/root-signatures`  
    `https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_root_signature`
  - Hardware resource binding tiers:  
    `https://learn.microsoft.com/windows/win32/direct3d12/hardware-resource-binding-tiers`
  - Sample pattern: `DirectX-Graphics-Samples/MiniEngine/Core/RootSignature.cpp` (uses `D3D12_VERSIONED_ROOT_SIGNATURE_DESC` and v1.1 where available).
- **Minimal Patch Plan**
  1. Introduce a helper in `D3D12Context` (or a small utility) that exposes `highestRootSignatureVersion_` and `resourceBindingTier_` to root‑signature creation code.
  2. Refactor `D3D12PipelineCache::getOrCreateRootSignature` and `D3D12RootSignatureBuilder::buildRootSignature` to:
     - Use `D3D12_VERSIONED_ROOT_SIGNATURE_DESC` and `D3D12_ROOT_SIGNATURE_VERSION_1_1` when `highestRootSignatureVersion_ >= D3D_ROOT_SIGNATURE_VERSION_1_1`.
     - Fall back to v1.0 (`D3D_ROOT_SIGNATURE_VERSION_1`) and **bounded** `NumDescriptors` (using `D3D12RootSignatureBuilder::getMaxDescriptorCount`) when only v1.0 is supported or when `resourceBindingTier_ == D3D12_RESOURCE_BINDING_TIER_1`.
  3. For v1.1 path, switch descriptor ranges to `D3D12_DESCRIPTOR_RANGE1` and set appropriate `Flags` (e.g., `D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE` where tables are updated frequently).
  4. Ensure that unbounded ranges (`UINT_MAX`) are only used when both `highestRootSignatureVersion_ == D3D_ROOT_SIGNATURE_VERSION_1_1` **and** `resourceBindingTier_` allows them (Tier‑2/3).
  5. Add assertions/logging around root‑signature creation so that any serialization failure logs `HighestVersion`, `ResourceBindingTier`, and the offending range.
- **Acceptance Checks**
  - Compile succeeds on hardware with:
    - Root Signature v1.0 + Binding Tier 1 (bounded ranges path).
    - Root Signature v1.1 + Binding Tier 2/3 (unbounded ranges path).
  - Run `cmd /c mandatory_all_tests.bat` with the same D3D12 debug/GBV/DRED env vars as this audit:
    - `IGL_D3D12_DEBUG=1`, `IGL_D3D12_GPU_VALIDATION=1`, `IGL_D3D12_DRED=1`, `IGL_DXGI_DEBUG=1`.
  - Confirm no D3D12 debug‑layer warnings or errors related to root signatures or descriptor ranges in the debug output or InfoQueue.
  - Existing render sessions and unit tests that exercise complex descriptor usage (bind groups, many textures/buffers) continue to pass.

---

## Task Q2 – Wire Root Signature Version & Binding Tier into All RS Builders

- **Files / Lines**
  - `src/igl/d3d12/D3D12Context.cpp:560-580` (root signature feature query)
  - `src/igl/d3d12/D3D12PipelineCache.h:145-210`
  - `src/igl/d3d12/D3D12PipelineBuilder.cpp:540-620`, `681-710`
- **Problem Summary**
  - `D3D12Context` correctly queries `D3D12_FEATURE_ROOT_SIGNATURE` and `D3D12_FEATURE_D3D12_OPTIONS` (for resource binding tier) and stores the results, but neither `D3D12PipelineCache` nor `D3D12PipelineBuilder` consumes these caps when building root signatures. All pipelines are effectively “hard‑wired” to v1.0 semantics and descriptor layouts, even where the device advertises more capable configurations.
- **Failing Rule / Message**
  - No direct debug‑layer message yet, but this is a conformance/robustness gap: pipeline state creation ignores discovered caps and therefore does not fully adhere to the “query caps, then configure root signatures and descriptor tables accordingly” pattern in the official documentation.
- **Official Microsoft References**
  - Root signature feature query:  
    `https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_root_signature`
  - Hardware resource binding tiers:  
    `https://learn.microsoft.com/windows/win32/direct3d12/hardware-resource-binding-tiers`
  - MiniEngine patterns: `DirectX-Graphics-Samples/MiniEngine/Core/RootSignature.cpp` (different layouts chosen based on caps).
- **Minimal Patch Plan**
  1. Expose small, const accessors on `D3D12Context` for `getHighestRootSignatureVersion()` and `getResourceBindingTier()`.
  2. Thread a `const D3D12Context*` (or a minimal caps struct) into all root‑signature builders:
     - Graphics: `D3D12GraphicsPipelineBuilder` / `D3D12PipelineCache::hashRenderPipelineDesc`.
     - Compute: `Device::createComputePipelineState` / `D3D12PipelineCache::getOrCreateRootSignature`.
  3. For each root‑signature construction site, choose:
     - Range sizes via `D3D12RootSignatureBuilder::getMaxDescriptorCount(context, rangeType)` when tier == 1.
     - Unbounded or larger ranges only when tier >= 2 **and** root signature version >= 1.1 (see Task Q1).
  4. Add debug logging at INFO/VERBOSE level showing the chosen layout (version, number of parameters, descriptor counts per table) for the first few pipelines created, so misconfigurations surface during validation.
- **Acceptance Checks**
  - On Tier‑1 hardware (or when forcing conservative caps), pipelines must:
    - Use bounded descriptor ranges (no `UINT_MAX`).
    - Create root signatures without debug‑layer warnings.
  - On Tier‑2/3 hardware with RS 1.1, pipelines with many descriptors must still create and bind correctly without InfoQueue warnings.
  - Render sessions with heavy descriptor usage (e.g., MRTs, bind groups, compute workloads) behave identically before and after the change.

---

## Task Q3 – Replace Per‑Upload `waitForGPU` with Frame‑Scoped Fences

- **Files / Lines**
  - `src/igl/d3d12/Buffer.cpp:180-260,260-520` (upload path and fence handling)
  - `src/igl/d3d12/CommandQueue.cpp:40-68` (`executeDeferredCopies` uses `ctx.waitForGPU()`)
  - `src/igl/d3d12/D3D12FrameManager.cpp` (frame advancement/fence model)
- **Problem Summary**
  - `Buffer::upload` and deferred texture‑to‑buffer copy paths use `ctx.waitForGPU()` / `waitForUploadFence` to block the CPU until each upload or readback completes. This contradicts D3D12 best practices, which recommend asynchronous uploads and frame‑ring fences instead of per‑operation global waits, and may have significant performance impact under load.
- **Failing Rule / Message**
  - No direct D3D12 validation message, but this is a quality/performance defect relative to official guidance on fences and command queue synchronization.
- **Official Microsoft References**
  - Fences and synchronization:  
    `https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-signal`  
    `https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12fence-seteventoncompletion`
  - Sample pattern: `DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloFrameBuffering/D3D12HelloFrameBuffering.cpp` (uses frame‑ring fences and avoids per‑draw waits).
- **Minimal Patch Plan**
  1. In `Buffer::upload`:
     - Keep the staging/upload command list and barrier logic but remove the final `waitForUploadFence(uploadFenceValue)` call.
     - Track the upload fence value in a per‑device/per‑frame structure (e.g., `pendingUploads_`) and reuse the existing allocator pool’s fence‑based reclamation.
  2. In `CommandQueue::executeDeferredCopies`:
     - Replace the unconditional `ctx.waitForGPU()` with a per‑queue fence sequence that:
       - Signals a fence after executing the main rendering command list.
       - Uses a small helper to schedule a dedicated copy list that waits on that fence (GPU wait via `ID3D12CommandQueue::Wait`), avoiding a CPU‑side stall.
  3. Add a maintenance hook in `FrameManager` (or `Device`) that waits for upload/copy fences once per frame (or at safe points) instead of per call.
  4. Ensure all existing callers that relied on synchronous semantics either:
     - Explicitly request a blocking upload API (rare); or
     - Are updated to handle asynchronous completion (more typical).
- **Acceptance Checks**
  - Under heavy upload/copy workloads, CPU frame time should improve measurably compared to the current implementation (no extra GPU idle segments in PIX).
  - Render correctness must remain unchanged (no partially uploaded resources observed in render sessions).
  - D3D12 debug layer should not report any fence misuse (double‑signal, invalid wait).
  - All render sessions and unit tests continue to pass under `cmd /c mandatory_all_tests.bat`.

---

## Task Q4 – Make D3D12 Debug Layer / GBV / DRED Explicit in Test Harness

- **Files / Lines**
  - `src/igl/tests/util/device/d3d12/TestDevice.cpp:14-24`
  - `src/igl/d3d12/D3D12Context.cpp:390-418,460-480`
  - `artifacts/unit_tests/D3D12/IGLTests_Debug.log:2982-2985`
  - `artifacts/unit_tests/D3D12/IGLTests_Release.log:tail`
- **Problem Summary**
  - The D3D12 test device factory takes a `bool enableDebugLayer` argument but does not pass it into `D3D12Context` or use it to configure the debug layer, GPU‑based validation, DXGI debug, or DRED. Validation behavior is controlled solely by environment variables, and no structured InfoQueue/validation logs are written to `artifacts/validation/*`, making it impossible to prove that all D3D12 validation warnings have been observed and recorded.
- **Failing Rule / Message**
  - Process requirement: “D3D12 debug validation must be enabled, all violations must be recorded.”  
  - Current state: unit‑test logs show `[Tests] D3D12 test device requested (debug layer: disabled)` banners and no captured InfoQueue messages.
- **Official Microsoft References**
  - D3D12 debug layer: `https://learn.microsoft.com/windows/win32/direct3d12/debug-layer`
  - GPU‑based validation: `https://learn.microsoft.com/windows/win32/direct3d12/gpu-based-validation`
  - DRED overview: `https://learn.microsoft.com/windows/win32/direct3d12/device-removed-extended-data`
  - Sample pattern: `DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloTriangle/D3D12HelloTriangle.cpp` (enables debug layer in debug builds).
- **Minimal Patch Plan**
  1. Extend `HeadlessD3D12Context::initializeHeadless` (and/or its configuration) so it accepts an explicit debug configuration struct:
     - `enableDebugLayer`, `enableGPUValidation`, `enableDRED`, `enableDXGIDebug`, `breakOnError`, `breakOnWarning`.
  2. In `createTestDevice(bool enableDebugLayer)`:
     - Map `enableDebugLayer` into the context configuration instead of ignoring it.
     - Add a secondary environment check (e.g., `IGL_D3D12_TEST_GPU_VALIDATION`) that, when set, forces GPU‑based validation ON regardless of the parameter.
  3. Add a small InfoQueue dump helper used only in tests:
     - After each test suite or at test harness shutdown, query `ID3D12InfoQueue`, pull stored messages, and write them into a dedicated folder `artifacts/validation` (e.g., `validation/IGLTests_Debug_infoqueue.log`, `validation/RenderSessions_infoqueue.log`).
  4. Ensure DRED state is kept as‑is (already configured in `D3D12Context::createDevice`) but make sure device removal in tests also writes a DRED summary file under `artifacts/validation`.
- **Acceptance Checks**
  - When running `cmd /c mandatory_all_tests.bat` in this configuration:
    - The D3D12 test logs should clearly state the debug layer and GPU‑based validation status (enabled/disabled) in a deterministic way, not just via environment‑dependent verbosity.
    - Files appear under `artifacts/validation` with InfoQueue and DRED summaries for both unit tests and render sessions.
  - No D3D12 debug‑layer warnings should be present in these logs for passing tests; any warnings should be triaged or fixed and tracked separately.

---

## Task Q5 – Fix TextureLoader Tests in D3D12 Configuration

- **Files / Lines**
  - Harness logs:  
    - `artifacts/mandatory/unit_tests.log:1-24`  
    - `artifacts/unit_tests/D3D12/IGLTests_Debug.log:2932`  
    - `artifacts/unit_tests/D3D12/IGLTests_Release.log:2837`
  - Related engine code (likely touch points):  
    - `src/igl/Texture.cpp:178-199` (`TextureRangeDesc::validate`)  
    - Texture loader and factory files under `src/igl` (not D3D12‑specific, but run through D3D12 tests).
- **Problem Summary**
  - In the D3D12 test configuration:
    - Debug run fails `TextureLoaderFactoryTest.loadKtx2` with an SEH exception (`0x87d`) and multiple `TextureRangeDesc::validate` assertion messages.
    - Release run fails `TextureLoaderFactoryTest.loadHdr` with a similar SEH exception; the gtest summary is missing, indicating abnormal termination.
  - These failures likely stem from mismatches between what the tests expect from the loader and the stricter `TextureRangeDesc::validate` checks and/or resource creation paths used when D3D12 is the active backend.
- **Failing Rule / Message**
  - Harness: `UNIT TESTS FAILED` (2 failing targets).  
  - GTest:  
    - `[  FAILED  ] TextureLoaderFactoryTest.loadKtx2 (176 ms)` (Debug)  
    - `[  FAILED  ] TextureLoaderFactoryTest.loadHdr (211 ms)` (Release)  
  - Multiple `Verify failed in 'TextureRangeDesc::validate'` messages in both logs.
- **Official Microsoft References**
  - Resource uploading and subresource footprints:  
    `https://learn.microsoft.com/windows/win32/direct3d12/uploading-resources`
  - Texture formats and DXGI formats:  
    `https://learn.microsoft.com/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format`
  - Sample pattern for basic texture loading and uploads:  
    `DirectX-Graphics-Samples/Samples/Desktop/D3D12HelloTexture/D3D12HelloTexture.cpp`.
- **Minimal Patch Plan**
  1. Inspect `TextureLoaderFactoryTest.loadKtx2` and `TextureLoaderFactoryTest.loadHdr` to see which ranges and mip/array parameters they feed into `TextureRangeDesc` and the loader pipeline.
  2. Cross‑check the ranges against `TextureRangeDesc::validate` constraints (width/height/depth > 0, mip/array bounds, face counts) and fix either:
     - The tests (if they intentionally pass out‑of‑range dimensions that are no longer valid), or
     - The loader/texture init code (if they are computing incorrect ranges for KTX2/HDR data).
  3. Validate that the textures created in these tests use legal D3D12 resource descriptions (dimensions, mip levels, format) and legal subresource upload patterns per `Uploading resources` guidance.
  4. Add small, targeted logging in the failure path (for these tests only) to print the offending range (mipLevel, x/y/z, width/height/depth, layers/faces) so future regressions are easier to diagnose.
- **Acceptance Checks**
  - Both `TextureLoaderFactoryTest.loadKtx2` and `TextureLoaderFactoryTest.loadHdr` pass in Debug and Release D3D12 configurations.
  - No additional test failures are introduced in the D3D12 unit‑test suite.
  - D3D12 debug layer and GPU‑based validation remain clean for these tests (no InfoQueue warnings about resource dimensions, subresource footprints, or copy regions).
  - `cmd /c mandatory_all_tests.bat` reports `Unit Tests: PASS` with the same D3D12 validation env vars used in this audit.

