# E-007: Shader Reflection Doesn't Validate All Binding Points

**Priority:** P1-Medium
**Category:** Enhancement - Shader System
**Status:** Open
**Effort:** Medium (2-3 days)

---

## 1. Problem Statement

The D3D12 backend's shader reflection system does not validate all binding points against the root signature, leading to potential runtime errors when shaders reference bindings that don't exist in the root signature or when binding indices are out of range.

### The Danger

**Runtime GPU crashes** can occur when shaders access descriptor table entries that were never bound or are out of bounds. This manifests as:
- Device removed errors (DXGI_ERROR_DEVICE_REMOVED)
- Silent rendering failures where draw calls produce no output
- Validation layer errors only in debug mode, but crashes in release
- Mismatched binding indices between shader expectations and root signature layout

Without comprehensive validation, these issues only appear at runtime during specific rendering scenarios, making them difficult to diagnose and often blamed on "driver issues."

---

## 2. Root Cause Analysis

The shader reflection code in D3D12 only partially validates shader bindings:

**Current Limited Validation:**
```cpp
// In ShaderModule.cpp or similar - checks basic type matching
if (reflection->GetResourceBindingDesc(i, &bindDesc) == S_OK) {
  // Only validates that the binding exists, not its range or compatibility
  if (bindDesc.Type == D3D_SIT_CBUFFER) {
    // Adds to root signature without bounds checking
  }
}
```

**Missing Validations:**
1. **Binding range validation** - No check that `register(b5)` exists in descriptor tables
2. **Space validation** - `register(t0, space1)` not validated against root signature spaces
3. **Descriptor count validation** - Array bindings `Texture2D textures[10]` not checked
4. **Static vs dynamic mismatch** - Shader expects root descriptor but gets table slot
5. **UAV counter validation** - UAVs with append/consume not checked for counter support

**Example of Undetected Issue:**
```cpp
// Shader declares:
Texture2D myTexture : register(t5, space0);

// But root signature only has:
CD3DX12_DESCRIPTOR_RANGE ranges[1];
ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0); // Only t0-t2

// No validation error - crashes at draw time
```

---

## 3. Official Documentation References

- **Shader Reflection API:**
  https://learn.microsoft.com/en-us/windows/win32/api/d3d12shader/nn-d3d12shader-id3d12shaderreflection

- **Root Signature Validation:**
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signature-version-1-1

- **Resource Binding Rules:**
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/resource-binding-in-hlsl

- **Debug Layer Validation:**
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer

---

## 4. Code Location Strategy

**Primary Files to Modify:**

1. **Shader reflection implementation:**
   ```
   Search for: "ID3D12ShaderReflection"
   In files: src/igl/d3d12/ShaderModule.cpp, src/igl/d3d12/ShaderReflection.*
   Pattern: Look for GetResourceBindingDesc() calls
   ```

2. **Root signature creation:**
   ```
   Search for: "D3D12_ROOT_SIGNATURE_DESC" or "CD3DX12_ROOT_SIGNATURE_DESC"
   In files: src/igl/d3d12/RenderPipelineState.cpp, src/igl/d3d12/PipelineState.*
   Pattern: Look for root signature initialization and SetRootSignature()
   ```

3. **Binding validation:**
   ```
   Search for: "GetResourceBindingDescByName" or "ShaderStages::"
   In files: src/igl/d3d12/ShaderModule.cpp
   Pattern: Look for shader stage reflection loops
   ```

4. **Descriptor table setup:**
   ```
   Search for: "D3D12_DESCRIPTOR_RANGE" or "InitDescriptorTable"
   In files: src/igl/d3d12/Device.cpp, src/igl/d3d12/RenderCommandEncoder.cpp
   Pattern: Look for descriptor range initialization
   ```

---

## 5. Detection Strategy

### Reproduction Steps:

1. **Create shader with out-of-range binding:**
   ```hlsl
   // test_shader.hlsl
   Texture2D tex0 : register(t0);
   Texture2D tex1 : register(t1);
   Texture2D tex2 : register(t2);
   Texture2D tex99 : register(t99); // Way beyond typical range

   float4 main(float2 uv : TEXCOORD) : SV_Target {
       return tex99.Sample(sampler0, uv);
   }
   ```

2. **Create root signature with limited range:**
   ```cpp
   // In test or application code
   CD3DX12_DESCRIPTOR_RANGE range;
   range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0); // Only t0-t4
   // Shader wants t99 - should be caught during validation
   ```

3. **Run with debug layer enabled:**
   ```
   Set D3D12_DEBUG=1
   ./build/Debug/IGLTests.exe --gtest_filter="*Shader*Reflection*"
   ```

4. **Expected error (currently missing):**
   ```
   IGL_ERROR: Shader references binding register t99 but root signature
   descriptor table only contains range [t0-t4]
   ```

### Test Cases:

```cpp
// Should fail validation:
TEST(ShaderReflection, DetectsOutOfRangeBindings)
TEST(ShaderReflection, DetectsSpaceMismatch)
TEST(ShaderReflection, DetectsDescriptorArrayOverflow)
TEST(ShaderReflection, DetectsUAVCounterMismatch)
```

---

## 6. Fix Guidance

### Step 1: Extend Shader Reflection Data Structure

```cpp
// In ShaderModule.h or ShaderReflection.h
struct ShaderBindingInfo {
  std::string name;
  D3D_SHADER_INPUT_TYPE type;
  uint32_t bindPoint;           // register index (t5 -> 5)
  uint32_t bindSpace;           // space0 -> 0, space1 -> 1
  uint32_t bindCount;           // array size
  uint32_t flags;               // UAV counter, etc.
};

class ShaderReflectionD3D12 {
  std::vector<ShaderBindingInfo> cbvBindings_;
  std::vector<ShaderBindingInfo> srvBindings_;
  std::vector<ShaderBindingInfo> uavBindings_;
  std::vector<ShaderBindingInfo> samplerBindings_;
};
```

### Step 2: Collect Complete Binding Information

```cpp
// In ShaderModule.cpp - enhance existing reflection
Result<void> ShaderModule::reflectShaderBindings(ID3D12ShaderReflection* reflection) {
  D3D12_SHADER_DESC shaderDesc;
  reflection->GetDesc(&shaderDesc);

  for (uint32_t i = 0; i < shaderDesc.BoundResources; ++i) {
    D3D12_SHADER_INPUT_BIND_DESC bindDesc;
    reflection->GetResourceBindingDesc(i, &bindDesc);

    ShaderBindingInfo info;
    info.name = bindDesc.Name;
    info.type = bindDesc.Type;
    info.bindPoint = bindDesc.BindPoint;
    info.bindSpace = bindDesc.Space;        // ← Now capturing space
    info.bindCount = bindDesc.BindCount;    // ← Now capturing array size
    info.flags = bindDesc.uFlags;           // ← Now capturing flags

    // Categorize by type
    switch (bindDesc.Type) {
      case D3D_SIT_CBUFFER:
        cbvBindings_.push_back(info);
        break;
      case D3D_SIT_TEXTURE:
      case D3D_SIT_STRUCTURED:
      case D3D_SIT_BYTEADDRESS:
        srvBindings_.push_back(info);
        break;
      case D3D_SIT_UAV_RWTYPED:
      case D3D_SIT_UAV_RWSTRUCTURED:
      case D3D_SIT_UAV_RWBYTEADDRESS:
      case D3D_SIT_UAV_APPEND_STRUCTURED:
      case D3D_SIT_UAV_CONSUME_STRUCTURED:
        uavBindings_.push_back(info);
        break;
      case D3D_SIT_SAMPLER:
        samplerBindings_.push_back(info);
        break;
    }
  }

  return Result<void>();
}
```

### Step 3: Add Root Signature Validation

```cpp
// In RenderPipelineState.cpp or PipelineState.cpp
Result<void> validateShaderBindingsAgainstRootSignature(
    const ShaderReflectionD3D12& vsReflection,
    const ShaderReflectionD3D12& psReflection,
    const D3D12_ROOT_SIGNATURE_DESC& rootSigDesc) {

  // Collect all shader bindings
  std::vector<ShaderBindingInfo> allBindings;
  allBindings.insert(allBindings.end(),
                     vsReflection.srvBindings_.begin(),
                     vsReflection.srvBindings_.end());
  allBindings.insert(allBindings.end(),
                     psReflection.srvBindings_.begin(),
                     psReflection.srvBindings_.end());
  // ... repeat for CBV, UAV, samplers

  // Validate each binding against root signature
  for (const auto& binding : allBindings) {
    bool found = false;

    for (uint32_t i = 0; i < rootSigDesc.NumParameters; ++i) {
      const auto& param = rootSigDesc.pParameters[i];

      if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
        for (uint32_t j = 0; j < param.DescriptorTable.NumDescriptorRanges; ++j) {
          const auto& range = param.DescriptorTable.pDescriptorRanges[j];

          // Check if binding falls within this descriptor range
          if (range.RegisterSpace == binding.bindSpace) {
            uint32_t rangeEnd = range.BaseShaderRegister + range.NumDescriptors;
            uint32_t bindingEnd = binding.bindPoint + binding.bindCount;

            if (binding.bindPoint >= range.BaseShaderRegister &&
                bindingEnd <= rangeEnd) {
              found = true;
              break;
            }
          }
        }
      } else if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV) {
        // Validate root CBV
        if (param.Descriptor.RegisterSpace == binding.bindSpace &&
            param.Descriptor.ShaderRegister == binding.bindPoint) {
          found = true;
        }
      }
      // ... similar for SRV, UAV root descriptors

      if (found) break;
    }

    if (!found) {
      return Result<void>(Result::Code::ArgumentOutOfRange,
                         "Shader binding not found in root signature: " +
                         binding.name + " at register " +
                         std::to_string(binding.bindPoint) +
                         ", space " + std::to_string(binding.bindSpace));
    }
  }

  return Result<void>();
}
```

### Step 4: Integrate Validation into Pipeline Creation

```cpp
// In RenderPipelineState.cpp - create() or similar
Result<std::shared_ptr<IRenderPipelineState>> Device::createRenderPipeline(...) {
  // ... existing pipeline setup ...

  // NEW: Validate shader bindings against root signature
  if (vertexShader && pixelShader) {
    auto validationResult = validateShaderBindingsAgainstRootSignature(
        vertexShader->getReflection(),
        pixelShader->getReflection(),
        rootSignatureDesc);

    if (!validationResult.isOk()) {
      return Result<std::shared_ptr<IRenderPipelineState>>(
          validationResult.code(), validationResult.message());
    }
  }

  // ... continue with pipeline creation ...
}
```

### Step 5: Add Warning for Common Issues

```cpp
// In ShaderModule.cpp
void warnAboutCommonBindingIssues(const std::vector<ShaderBindingInfo>& bindings) {
  // Warn about large gaps in binding indices
  for (const auto& binding : bindings) {
    if (binding.bindPoint > 16) {
      IGL_LOG_INFO("Shader uses high register index %s at register %u. "
                   "Consider using lower indices for better compatibility.",
                   binding.name.c_str(), binding.bindPoint);
    }

    // Warn about space usage (less common, might be intentional)
    if (binding.bindSpace > 0) {
      IGL_LOG_INFO("Shader uses explicit register space: %s at space%u. "
                   "Ensure root signature matches.",
                   binding.name.c_str(), binding.bindSpace);
    }
  }
}
```

---

## 7. Testing Requirements

### Unit Tests:

```bash
# Run D3D12-specific shader reflection tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Shader*Reflection*"

# Run all D3D12 pipeline creation tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Pipeline*"
```

### Integration Tests:

```bash
# Test all render sessions with validation enabled
set D3D12_DEBUG=1
./test_all_sessions.bat

# Specific shader-heavy sessions
./build/Debug/Session/TinyMeshSession.exe
./build/Debug/Session/Textured3DCubeSession.exe
./build/Debug/Session/TextureArraySession.exe
```

### Expected Test Results:

1. **All existing tests must pass** - No regressions in shader loading
2. **New validation tests must pass:**
   - Detect out-of-range binding indices
   - Detect space mismatches
   - Detect array size overflows
   - Allow valid bindings without false positives

### Modification Restrictions:

- **DO NOT modify** cross-platform shader compilation logic in `src/igl/ShaderCompiler.cpp`
- **DO NOT change** the public IGL shader API in `include/igl/Shader.h`
- **DO NOT break** existing shaders that use valid binding patterns
- **ONLY enhance** D3D12-specific validation in `src/igl/d3d12/` directory

---

## 8. Definition of Done

### Validation Checklist:

- [ ] Shader reflection collects complete binding information (point, space, count)
- [ ] Root signature validation function implemented and tested
- [ ] Validation integrated into pipeline creation (pre-runtime check)
- [ ] All unit tests pass without regression
- [ ] All integration tests (render sessions) pass with validation enabled
- [ ] Warning messages for common binding issues implemented
- [ ] Code reviewed for edge cases (empty shaders, compute shaders, etc.)

### User Confirmation Required:

Before proceeding with implementation:
1. User reviews and approves the validation strategy
2. User confirms which binding patterns are considered valid in their codebase
3. User tests the changes with their production shaders

**STOP - Do NOT proceed to next task until user confirms:**
- [ ] All tests pass (unit + integration)
- [ ] No false positive validation errors on valid shaders
- [ ] Actual binding errors are now caught during pipeline creation

---

## 9. Related Issues

### Blocks:
- None - Standalone enhancement

### Related:
- **E-010** - Shader error messages unclear (validation errors need clear messages)
- **E-011** - Missing shader debug names (helps identify which shader has binding issues)
- **C-007** - Shader reflection incomplete (this addresses the remaining gaps)

### Depends On:
- None - Can be implemented independently

---

## 10. Implementation Priority

**Priority:** P1-Medium

**Effort Estimate:** 2-3 days
- Day 1: Extend shader reflection to collect complete binding info
- Day 2: Implement and test root signature validation logic
- Day 3: Integration, testing, and edge case handling

**Risk Assessment:** Low
- Changes are additive (new validation, no logic changes)
- Validation can be made non-fatal initially (warnings only)
- Easy to test with existing shader suites

**Impact:** Medium-High
- **Prevents runtime GPU crashes** from binding mismatches
- **Improves debugging** by catching errors at pipeline creation
- **Better compatibility** with complex shader setups
- Minimal performance impact (validation only at pipeline creation, not runtime)

---

## 11. References

### Official Documentation:

1. **ID3D12ShaderReflection Interface:**
   https://learn.microsoft.com/en-us/windows/win32/api/d3d12shader/nn-d3d12shader-id3d12shaderreflection

2. **D3D12_SHADER_INPUT_BIND_DESC:**
   https://learn.microsoft.com/en-us/windows/win32/api/d3d12shader/ns-d3d12shader-d3d12_shader_input_bind_desc

3. **Root Signatures:**
   https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signatures

4. **Resource Binding in HLSL:**
   https://learn.microsoft.com/en-us/windows/win32/direct3d12/resource-binding-in-hlsl

5. **DirectX Graphics Samples - Root Signature Example:**
   https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HelloWorld

### Additional Resources:

6. **Debug Layer Best Practices:**
   https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer

7. **Descriptor Heaps and Tables:**
   https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps

---

**Last Updated:** 2025-11-12
**Assignee:** Unassigned
**Estimated Completion:** 2-3 days after start
