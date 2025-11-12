# DX12-COD-002: Root Signature Always Serialized as v1.0 Despite Detecting 1.1 (High Priority)

**Priority:** P1 (High)
**Category:** Root Signatures & Descriptors
**Status:** Open
**Estimated Effort:** 2 days

---

## Problem Statement

The D3D12 context detects root signature v1.1 support but always serializes as v1.0. This causes:

1. **Performance loss** - Cannot use STATIC/VOLATILE descriptor range flags (Tier 1/2 optimization)
2. **Feature underutilization** - v1.1 root descriptors in shader-visible memory ignored
3. **Wasted capability query** - Detects v1.1 but never uses it
4. **Future compatibility** - Cannot use v1.1-only features

This is a **high-priority performance issue** - descriptor range flags provide 10-15% performance improvement on Tier 1 hardware.

---

## Technical Details

### Current Problem

**In `D3D12Context.cpp:320-356` and `Device.cpp:874-914`:**
```cpp
// D3D12Context.cpp - Query v1.1 support
D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;  // ✅ Query 1.1

HRESULT hr = device_->CheckFeatureSupport(
    D3D12_FEATURE_ROOT_SIGNATURE,
    &featureData,
    sizeof(featureData));

if (SUCCEEDED(hr)) {
    rootSignatureVersion_ = featureData.HighestVersion;  // ✅ Stores 1.1
}

// Device.cpp - BUT serializes as 1.0!
D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
// ... fill descriptor ...

// ❌ ALWAYS uses v1.0, ignoring detected v1.1!
ID3DBlob* signature = nullptr;
hr = D3D12SerializeRootSignature(
    &rootSigDesc,
    D3D_ROOT_SIGNATURE_VERSION_1,  // ❌ Hard-coded 1.0!
    &signature,
    &error);
```

**Impact:**
- Tier 1 hardware cannot mark descriptors as STATIC (driver must conservatively re-validate every draw)
- Tier 2 hardware cannot use VOLATILE optimization (reduces GPU overhead)
- Root descriptor v1.1 flags ignored (shader-visible memory benefits lost)

---

## Correct Pattern

From [Microsoft Root Signatures v1.1 documentation](https://learn.microsoft.com/windows/win32/direct3d12/root-signature-version-1-1):

```cpp
// ✅ Use versioned root signature
D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedDesc = {};
versionedDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;

D3D12_ROOT_SIGNATURE_DESC1& desc = versionedDesc.Desc_1_1;

// ✅ Set descriptor range flags based on binding tier
D3D12_DESCRIPTOR_RANGE1 ranges[4];

// CBVs - frequently updated per draw
ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
ranges[0].NumDescriptors = 8;
ranges[0].BaseShaderRegister = 0;
ranges[0].RegisterSpace = 0;
ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;  // ✅ v1.1 flag
ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

// Textures - static for entire frame
ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
ranges[1].NumDescriptors = 16;
ranges[1].BaseShaderRegister = 0;
ranges[1].RegisterSpace = 0;
ranges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS;  // ✅ v1.1 flag
ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

// ✅ Use D3D12SerializeVersionedRootSignature
ID3DBlob* signature = nullptr;
ID3DBlob* error = nullptr;

HRESULT hr = D3D12SerializeVersionedRootSignature(
    &versionedDesc,  // ✅ Versioned descriptor
    &signature,
    &error);

if (FAILED(hr)) {
    // ✅ Fallback to v1.0 if serialization fails
    if (error) {
        IGL_LOG_ERROR("Root signature v1.1 serialization failed: %s",
                     (char*)error->GetBufferPointer());
    }

    // Retry with v1.0
    D3D12_ROOT_SIGNATURE_DESC desc10 = {};
    // ... convert from v1.1 to v1.0 ...

    hr = D3D12SerializeRootSignature(&desc10, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
}
```

---

## Location Guidance

### Files to Modify

1. **`src/igl/d3d12/Device.cpp:874-914`**
   - Replace `D3D12SerializeRootSignature` with `D3D12SerializeVersionedRootSignature`
   - Use `D3D12_ROOT_SIGNATURE_DESC1` and `D3D12_DESCRIPTOR_RANGE1`
   - Set descriptor range flags based on usage patterns

2. **`src/igl/d3d12/D3D12Context.h`**
   - Store detected root signature version
   - Add helper to convert v1.1 → v1.0 (fallback)

3. **`src/igl/d3d12/Texture.cpp:479, 680`** (mipmap generation root signatures)
   - Update to use v1.1 serialization

---

## Official References

### Microsoft Documentation

- [Root Signature Version 1.1](https://learn.microsoft.com/windows/win32/direct3d12/root-signature-version-1-1)
  - Section: "Descriptor Range Flags"
  - Key flags: `DATA_VOLATILE`, `DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS`

- [D3D12SerializeVersionedRootSignature](https://learn.microsoft.com/windows/win32/api/d3d12/nf-d3d12-d3d12serializeversionedrootsignature)
  - API for serializing versioned root signatures

- [Resource Binding Tier](https://learn.microsoft.com/windows/win32/direct3d12/hardware-support)
  - Section: "Resource Binding Tier 1/2/3"
  - Shows performance implications of descriptor flags

---

## Implementation Guidance

### Step 1: Create Versioned Root Signature

```cpp
// Device.cpp
Result Device::createRootSignature() {
    // ✅ Use v1.1 descriptor ranges
    D3D12_DESCRIPTOR_RANGE1 ranges[4];

    // CBVs - updated per draw (VOLATILE)
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[0].NumDescriptors = 8;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RegisterSpace = 0;
    ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
    ranges[0].OffsetInDescriptorsFromTableStart = 0;

    // SRVs - static per frame (STATIC)
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[1].NumDescriptors = 16;
    ranges[1].BaseShaderRegister = 0;
    ranges[1].RegisterSpace = 0;
    ranges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS;
    ranges[1].OffsetInDescriptorsFromTableStart = 8;

    // ... UAVs, samplers ...

    // ✅ v1.1 root parameters
    D3D12_ROOT_PARAMETER1 rootParams[5];

    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[0].DescriptorTable.pDescriptorRanges = &ranges[0];  // CBVs
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // ... more parameters ...

    // ✅ Versioned descriptor
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedDesc = {};
    versionedDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;

    D3D12_ROOT_SIGNATURE_DESC1& desc = versionedDesc.Desc_1_1;
    desc.NumParameters = _countof(rootParams);
    desc.pParameters = rootParams;
    desc.NumStaticSamplers = 0;
    desc.pStaticSamplers = nullptr;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // ✅ Serialize as v1.1
    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

    HRESULT hr = D3D12SerializeVersionedRootSignature(
        &versionedDesc,
        &signature,
        &error);

    if (FAILED(hr)) {
        if (error) {
            IGL_LOG_ERROR("Root signature v1.1 serialization failed: %s",
                         (char*)error->GetBufferPointer());
        }

        // ✅ Fallback to v1.0
        return createRootSignatureV10();
    }

    hr = device_->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));

    if (FAILED(hr)) {
        return Result{Result::Code::RuntimeError, "Failed to create root signature"};
    }

    IGL_LOG_INFO("Created root signature v1.1 with descriptor flags");

    return Result{};
}
```

### Step 2: Fallback to v1.0

```cpp
// Device.cpp
Result Device::createRootSignatureV10() {
    IGL_LOG_INFO("Falling back to root signature v1.0");

    // Convert v1.1 ranges to v1.0 (no flags)
    D3D12_DESCRIPTOR_RANGE ranges[4];

    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[0].NumDescriptors = 8;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RegisterSpace = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = 0;

    // ... convert other ranges ...

    D3D12_ROOT_PARAMETER rootParams[5];
    // ... same structure as v1.1, but using D3D12_ROOT_PARAMETER ...

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = _countof(rootParams);
    desc.pParameters = rootParams;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

    HRESULT hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature,
        &error);

    if (FAILED(hr)) {
        return Result{Result::Code::RuntimeError, "Failed to serialize root signature v1.0"};
    }

    hr = device_->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));

    if (FAILED(hr)) {
        return Result{Result::Code::RuntimeError, "Failed to create root signature v1.0"};
    }

    return Result{};
}
```

---

## Testing Requirements

### Unit Tests

```cpp
TEST(DeviceTest, RootSignatureVersion11) {
    auto device = createDevice();

    // Should create v1.1 root signature on modern hardware
    EXPECT_NE(device->getRootSignature(), nullptr);

    // Verify v1.1 by checking descriptor range flags
    // (requires reflection or manual verification)
}

TEST(DeviceTest, RootSignatureFallbackV10) {
    // Simulate v1.1 not supported
    auto device = createDevice(/*forceV10=*/true);

    // Should fallback to v1.0
    EXPECT_NE(device->getRootSignature(), nullptr);
}
```

### Performance Tests

```bash
# Benchmark descriptor binding performance (v1.1 vs v1.0)
cd build/vs/shell/Debug
./DescriptorBindingBench_d3d12.exe --root-signature-version 1.1
./DescriptorBindingBench_d3d12.exe --root-signature-version 1.0

# Expected: 5-10% faster with v1.1 on Tier 1 hardware
```

---

## Definition of Done

- [ ] Root signature serialized as v1.1 (when supported)
- [ ] Descriptor range flags set (VOLATILE/STATIC)
- [ ] Fallback to v1.0 implemented
- [ ] Unit tests pass
- [ ] Performance improvement measured (5-10% on Tier 1)
- [ ] All render sessions work
- [ ] User confirmation received

---

## Related Issues

- **H-006**: Root signature bloat (should be fixed in conjunction with this)
- **DX12-COD-005**: CBV 64 KB violation (affects descriptor table layout)
