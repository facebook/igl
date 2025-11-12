# H-006: Root Signature Bloat - 53/64 DWORDs Used (High Priority)

**Priority:** P1 (High)
**Category:** Descriptors & Root Signatures
**Status:** Open
**Estimated Effort:** 2 days

---

## Problem Statement

The D3D12 backend uses 53 out of 64 DWORDs in the root signature, leaving only 11 DWORDs for future extensibility. This causes:

1. **Limited extensibility** - Cannot add new features without root signature redesign
2. **Performance overhead** - Root CBVs consume 16 DWORDs (2 per CBV) when descriptor tables would use 1 DWORD
3. **Wasteful binding model** - Individual root descriptors instead of efficient descriptor tables
4. **Future incompatibility** - Ray tracing, bindless resources require additional root parameters

This is a **high-priority architecture issue** - the root signature design limits future D3D12 feature adoption.

---

## Technical Details

### Current Problem

**In `Device.cpp:1393`:**
```cpp
// ❌ WASTEFUL: Uses 8 root CBVs (16 DWORDs total)
// Each CBV = 2 DWORDs (GPU virtual address)

D3D12_ROOT_PARAMETER rootParams[13];

// Parameters 0-7: Individual root CBVs (2 DWORDs each = 16 total)
for (UINT i = 0; i < 8; i++) {
    rootParams[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;  // ❌ 2 DWORDs
    rootParams[i].Descriptor.ShaderRegister = i;
    rootParams[i].Descriptor.RegisterSpace = 0;
    rootParams[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
}

// Parameter 8: Push constants (32 DWORDs)
rootParams[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;  // ❌ 32 DWORDs
rootParams[8].Constants.Num32BitValues = 32;

// Parameter 9-12: Descriptor tables (1 DWORD each = 4 total)
// Textures, samplers, etc.

// Total: 16 + 32 + 4 + 1 = 53 DWORDs (only 11 remaining!)
```

**Breakdown:**
- Root CBVs (8 × 2 DWORDs) = **16 DWORDs** ← Wasteful
- Push constants (32 DWORDs) = **32 DWORDs**
- Descriptor tables (4 × 1 DWORD) = **4 DWORDs**
- UAV root descriptor (1 × 2 DWORDs) = **2 DWORDs**
- **Total: 53/64 DWORDs used**

### Microsoft Best Practice

From [Root Signatures documentation](https://learn.microsoft.com/windows/win32/direct3d12/root-signatures):

> **Root signature cost budget:**
> - Maximum 64 DWORDs per root signature
> - **Root descriptors cost 2 DWORDs** (GPU virtual address)
> - **Descriptor tables cost 1 DWORD** (GPU handle)
> - **32-bit constants cost 1 DWORD per value**
>
> **Recommendation:** Use descriptor tables instead of root descriptors for frequently changing resources. Reserve root descriptors for constants that change per draw call.

### Correct Pattern (from MiniEngine)

```cpp
// ✅ EFFICIENT: MiniEngine root signature (28 DWORDs total)

enum RootParameter {
    kRootParameterCBV0,         // 1 DWORD (descriptor table, not root descriptor!)
    kRootParameterSRVs,         // 1 DWORD (descriptor table)
    kRootParameterUAVs,         // 1 DWORD (descriptor table)
    kRootParameterSamplers,     // 1 DWORD (descriptor table)
    kRootParameterPushConstants, // 16 DWORDS (inline constants)
    kNumRootParameters
};

// Total: 1 + 1 + 1 + 1 + 16 = 20 DWORDs
// Remaining: 44 DWORDs for future features
```

---

## Location Guidance

### Files to Modify

1. **`src/igl/d3d12/Device.cpp:1393`**
   - Replace 8 root CBVs with single descriptor table
   - Reduce push constants from 32 to 16 DWORDs (if possible)
   - Consolidate descriptor tables

2. **`src/igl/d3d12/RenderCommandEncoder.cpp`**
   - Update CBV binding to use descriptor tables
   - Allocate CBV descriptors from heap instead of root signature

3. **`src/igl/d3d12/ComputeCommandEncoder.cpp`**
   - Same pattern - use descriptor tables for CBVs

4. **`src/igl/d3d12/D3D12Context.h`**
   - Document root signature layout
   - Add constants for root parameter indices

### Key Identifiers

- **Root parameter creation:** `D3D12_ROOT_PARAMETER`
- **Root CBVs:** `D3D12_ROOT_PARAMETER_TYPE_CBV`
- **Descriptor tables:** `D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE`
- **Root signature serialization:** `D3D12SerializeRootSignature`

---

## Official References

### Microsoft Documentation

- [Root Signatures](https://learn.microsoft.com/windows/win32/direct3d12/root-signatures)
  - Section: "Root Signature Limits"
  - Key rule: "64 DWORD maximum, use descriptor tables for efficiency"

- [Root Signature Best Practices](https://learn.microsoft.com/windows/win32/direct3d12/root-signature-limits#cost)
  - Section: "Cost of Root Signature Elements"
  - Shows DWORD cost for each root parameter type

- [MiniEngine RootSignature.cpp](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/RootSignature.cpp)
  - Lines 80-150: Efficient root signature design
  - Uses descriptor tables instead of root descriptors

---

## Implementation Guidance

### Step 1: Define Efficient Root Signature Layout

```cpp
// Device.h - root parameter indices
enum class RootParameter : UINT {
    CBVTable = 0,          // 1 DWORD - descriptor table for CBVs
    SRVTable = 1,          // 1 DWORD - textures
    UAVTable = 2,          // 1 DWORD - storage buffers/images
    SamplerTable = 3,      // 1 DWORD - samplers
    PushConstants = 4,     // 16 DWORDS - inline constants (reduced from 32)
    Count = 5
};

// Total: 1 + 1 + 1 + 1 + 16 = 20 DWORDs (was 53)
// Savings: 33 DWORDs (51% reduction!)
```

### Step 2: Create Descriptor Table for CBVs

```cpp
// Device.cpp
Result Device::createRootSignature() {
    // ✅ CBV descriptor table (1 DWORD instead of 16)
    D3D12_DESCRIPTOR_RANGE cbvRange = {};
    cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    cbvRange.NumDescriptors = 8;  // Support up to 8 CBVs
    cbvRange.BaseShaderRegister = 0;  // b0-b7
    cbvRange.RegisterSpace = 0;
    cbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER cbvTableParam = {};
    cbvTableParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    cbvTableParam.DescriptorTable.NumDescriptorRanges = 1;
    cbvTableParam.DescriptorTable.pDescriptorRanges = &cbvRange;
    cbvTableParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // ✅ Reduced push constants (16 DWORDs instead of 32)
    D3D12_ROOT_PARAMETER pushConstantsParam = {};
    pushConstantsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    pushConstantsParam.Constants.ShaderRegister = 8;  // b8
    pushConstantsParam.Constants.RegisterSpace = 0;
    pushConstantsParam.Constants.Num32BitValues = 16;  // ✅ Reduced from 32

    // Assemble root signature
    D3D12_ROOT_PARAMETER rootParams[] = {
        cbvTableParam,       // 1 DWORD
        srvTableParam,       // 1 DWORD
        uavTableParam,       // 1 DWORD
        samplerTableParam,   // 1 DWORD
        pushConstantsParam   // 16 DWORDs
    };

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = _countof(rootParams);
    rootSigDesc.pParameters = rootParams;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // Serialize and create
    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootSigDesc,
        D3D_ROOT_SIGNATURE_VERSION_1_1,  // Use v1.1 for descriptor flags
        &signature,
        &error);

    if (FAILED(hr)) {
        if (error) {
            IGL_LOG_ERROR("Root signature serialization failed: %s",
                         (char*)error->GetBufferPointer());
        }
        return Result{Result::Code::RuntimeError, "Failed to serialize root signature"};
    }

    hr = device_->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));

    if (FAILED(hr)) {
        return Result{Result::Code::RuntimeError, "Failed to create root signature"};
    }

    IGL_LOG_INFO("Created optimized root signature (20 DWORDs, was 53)");

    return Result{};
}
```

### Step 3: Update CBV Binding to Use Descriptor Table

```cpp
// RenderCommandEncoder.cpp
void RenderCommandEncoder::bindBuffer(IBuffer* buffer, uint32_t index) {
    auto* d3d12Buffer = static_cast<Buffer*>(buffer);

    // ❌ OLD: Root CBV (2 DWORDs per bind)
    // cmdList_->SetGraphicsRootConstantBufferView(
    //     index,
    //     d3d12Buffer->getGPUVirtualAddress());

    // ✅ NEW: Descriptor table (allocate CBV descriptor from heap)
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;

    auto result = commandBuffer_.getNextCbvSrvUavDescriptor(cpuHandle, gpuHandle);
    if (!result.isOk()) {
        IGL_LOG_ERROR("Failed to allocate CBV descriptor");
        return;
    }

    // Create CBV descriptor
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = d3d12Buffer->getGPUVirtualAddress();
    cbvDesc.SizeInBytes = d3d12Buffer->getAlignedSize();  // Must be 256-byte aligned

    device_->CreateConstantBufferView(&cbvDesc, cpuHandle);

    // Bind descriptor table (1 DWORD instead of 2)
    cmdList_->SetGraphicsRootDescriptorTable(
        static_cast<UINT>(RootParameter::CBVTable),
        gpuHandle);
}
```

### Step 4: Reduce Push Constants (If Possible)

```cpp
// Analyze push constant usage:
// - Do we need 32 DWORDs (128 bytes)?
// - Can we use 16 DWORDs (64 bytes)?

// If 64 bytes is sufficient:
pushConstantsParam.Constants.Num32BitValues = 16;  // ✅ Reduced

// If more space needed, consider splitting:
// - Frequently updated constants → push constants (16 DWORDs)
// - Rarely updated constants → small CBV (descriptor table)
```

---

## Testing Requirements

### Unit Tests

```cpp
TEST(DeviceTest, OptimizedRootSignature) {
    auto device = createDevice();

    // Verify root signature created successfully
    EXPECT_NE(device->getRootSignature(), nullptr);

    // Verify DWORD count (should be ≤32 for optimized version)
    // (Manual verification via PIX or shader reflection)
}

TEST(RenderCommandEncoderTest, CBVDescriptorTableBinding) {
    auto device = createDevice();
    auto cmdBuffer = device->createCommandBuffer();
    auto encoder = cmdBuffer->createRenderCommandEncoder({...});

    auto buffer = device->createBuffer({
        .length = 256,
        .usage = BufferUsage::Uniform
    });

    // Should bind via descriptor table (not root CBV)
    encoder->bindBuffer(buffer.get(), 0);

    encoder->endEncoding();

    // Expected: No validation errors, correct rendering
}
```

### Validation Tests

```bash
# Use PIX to inspect root signature
# Tools → Graphics Analyzer → Capture Frame
# Inspect root signature in PSO

# Expected:
# - Total DWORDs: ~20-32 (not 53)
# - CBV table present (not individual root CBVs)
```

### Performance Tests

```cpp
void BenchmarkRootSignatureCost() {
    auto device = createDevice();

    // Measure time to bind 100 buffers
    auto start = std::chrono::high_resolution_clock::now();

    for (UINT i = 0; i < 100; i++) {
        encoder->bindBuffer(buffer, i % 8);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Expected: Similar performance (descriptor table vs root CBV)
    // Benefit: Root signature extensibility, not raw performance
}
```

---

## Definition of Done

- [ ] Root CBVs replaced with descriptor table (16 DWORDs → 1 DWORD)
- [ ] Push constants reduced to 16 DWORDs (or justified at 32)
- [ ] Total root signature size ≤32 DWORDs
- [ ] CBV binding updated to use descriptor table
- [ ] Unit tests pass (descriptor table binding)
- [ ] PIX shows optimized root signature layout
- [ ] All render sessions pass without regression
- [ ] Documentation updated with new root parameter layout
- [ ] User confirmation received before proceeding to next task

---

## Notes

- **DO NOT** reduce push constants below application requirements
- **MUST** ensure CBV descriptors are allocated from per-frame heap
- Root descriptors are faster for per-draw constants, but limit extensibility
- Descriptor tables enable bindless rendering and ray tracing (future features)
- MiniEngine uses ~20-28 DWORDs for full feature set

---

## Related Issues

- **DX12-COD-002**: Root signature v1.0 only (should use v1.1 for descriptor flags)
- **H-007**: No descriptor bounds check (descriptor table allocations need validation)
- **H-012**: Descriptor creation redundancy (affects CBV descriptor allocation)
