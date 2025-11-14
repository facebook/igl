# E-006: Missing Geometry/Tessellation Shader Support

**Severity:** Medium
**Category:** Engine Features
**Status:** Open
**Related Issues:** H-009 (Shader Model Support), P0_DX12-002 (Root Signature)

---

## Problem Statement

The D3D12 backend only supports vertex and fragment (pixel) shaders. Geometry shaders, hull (tessellation control) shaders, and domain (tessellation evaluation) shaders are not supported, limiting advanced rendering techniques:
- **Geometry Shaders:** Point sprite expansion, primitive generation, stream output
- **Tessellation:** Adaptive LOD, displacement mapping, smooth surfaces

**Impact Analysis:**
- **Feature Parity:** Missing features available in Metal/Vulkan backends
- **Advanced Rendering:** Cannot implement tessellation-based displacement, geometry-based particle systems
- **Vendor Extensions:** No access to GPU-accelerated tessellation hardware
- **API Completeness:** IGL API may expose these stages, but D3D12 backend silently ignores them

**The Danger:**
- Applications expecting tessellation support will fail silently or produce incorrect output
- No compile-time or runtime warnings when unsupported shader stages are specified
- Users may waste time debugging "why isn't my geometry shader working?"

---

## Root Cause Analysis

### Current Implementation (`Device.cpp:1470-1479`):

```cpp
// Create PSO - zero-initialize all fields
D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
psoDesc.pRootSignature = rootSignature.Get();

// Shader bytecode
psoDesc.VS = {vsBytecode.data(), vsBytecode.size()};
psoDesc.PS = {psBytecode.data(), psBytecode.size()};

// Explicitly zero unused shader stages
psoDesc.DS = {nullptr, 0}; // Domain shader - NOT SUPPORTED
psoDesc.HS = {nullptr, 0}; // Hull shader - NOT SUPPORTED
psoDesc.GS = {nullptr, 0}; // Geometry shader - NOT SUPPORTED
```

**RenderPipelineDesc doesn't expose GS/HS/DS (`igl/RenderPipelineState.h`):**
```cpp
struct RenderPipelineDesc {
  std::shared_ptr<IShaderModule> vertexShader;   // Supported
  std::shared_ptr<IShaderModule> fragmentShader; // Supported
  // NO geometry shader field!
  // NO hull shader field!
  // NO domain shader field!
  // ...
};
```

### Why This Is Wrong:

1. **IGL API Gap:** `RenderPipelineDesc` doesn't have fields for geometry/tessellation shaders
2. **Silent Failure:** If other backends support these stages, D3D12 silently ignores them
3. **No Validation:** No error returned if application tries to use unsupported stages
4. **Hardcoded Zeros:** PSO descriptor explicitly sets these stages to null

---

## Official Documentation References

1. **Geometry Shader Overview**:
   - https://learn.microsoft.com/windows/win32/direct3d11/d3d10-graphics-programming-guide-geometry-shader
   - Use cases: Point sprites, primitive ID generation, stream output
   - Supported on Feature Level 10.0+ (all modern GPUs)

2. **Tessellation Overview**:
   - https://learn.microsoft.com/windows/win32/direct3d11/direct3d-11-advanced-stages-tessellation
   - Hull shader (control): Controls tessellation factors, patch constants
   - Domain shader (evaluation): Generates vertex positions from tessellation domain
   - Supported on Feature Level 11.0+ (all modern GPUs)

3. **Pipeline State Object**:
   - https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_graphics_pipeline_state_desc
   - GS, HS, DS fields in `D3D12_GRAPHICS_PIPELINE_STATE_DESC`

4. **DirectX-Graphics-Samples**:
   - https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HDR
   - Sample uses geometry shaders for fullscreen quads

---

## Code Location Strategy

### Files to Modify:

1. **igl/RenderPipelineState.h** (IGL core API):
   - Search for: `struct RenderPipelineDesc`
   - Context: Pipeline descriptor definition
   - Action: Add optional geometry/hull/domain shader fields

2. **Device.cpp** (`createRenderPipeline` method):
   - Search for: `psoDesc.GS = {nullptr, 0};`
   - Context: PSO descriptor initialization
   - Action: Conditionally set GS/HS/DS if provided in descriptor

3. **ShaderModule.h/cpp** (shader stage enum):
   - Search for: `enum class ShaderStage`
   - Context: Shader stage type definition
   - Action: Add Geometry, Hull, Domain enum values (if not already present)

4. **Device.cpp** (feature validation):
   - Search for: `validateDeviceLimits` method
   - Context: Device capability checking
   - Action: Log geometry/tessellation shader support

---

## Detection Strategy

### How to Reproduce:

Attempt to use geometry shader (will fail silently):
```cpp
// Create geometry shader (hypothetical API)
auto geometryShader = device->createShaderModule({
  .stage = ShaderStage::Geometry,
  .source = geometryShaderCode,
  // ...
});

RenderPipelineDesc desc;
desc.vertexShader = vertexShader;
desc.fragmentShader = fragmentShader;
desc.geometryShader = geometryShader; // Field doesn't exist!

auto pipeline = device->createRenderPipeline(desc, nullptr);
// Expected: Error or warning
// Actual: Silently ignores geometry shader (or compilation error)
```

### Verification After Fix:

1. Create pipeline with geometry shader specified
2. Verify PSO descriptor has non-null GS bytecode
3. Run test that requires geometry shader output
4. Verify rendering is correct

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Extend IGL RenderPipelineDesc

**File:** `include/igl/RenderPipelineState.h` (IGL core API)

**Locate:** Search for `struct RenderPipelineDesc`

**Current (PROBLEM):**
```cpp
struct RenderPipelineDesc {
  std::shared_ptr<IShaderModule> vertexShader;
  std::shared_ptr<IShaderModule> fragmentShader;
  // Missing geometry/tessellation shader fields!

  // ... other fields ...
};
```

**Fixed (SOLUTION):**
```cpp
struct RenderPipelineDesc {
  // Required shader stages
  std::shared_ptr<IShaderModule> vertexShader;
  std::shared_ptr<IShaderModule> fragmentShader;

  // E-006: Optional shader stages for advanced rendering
  // Geometry shader (optional): Used for primitive generation, stream output
  // Supported on D3D12 Feature Level 10.0+, Metal (via Metal Performance Shaders), Vulkan
  std::shared_ptr<IShaderModule> geometryShader = nullptr;

  // Hull shader / Tessellation Control shader (optional): Controls tessellation factors
  // Supported on D3D12 Feature Level 11.0+, Metal tessellation, Vulkan
  std::shared_ptr<IShaderModule> hullShader = nullptr;

  // Domain shader / Tessellation Evaluation shader (optional): Evaluates tessellated vertices
  // Supported on D3D12 Feature Level 11.0+, Metal tessellation, Vulkan
  // NOTE: hullShader and domainShader must be specified together (both or neither)
  std::shared_ptr<IShaderModule> domainShader = nullptr;

  // ... other fields ...
};
```

**Rationale:** Adds optional shader stage fields to IGL core API. Defaults to nullptr for backward compatibility.

---

#### Step 2: Update D3D12 Pipeline Creation to Support Additional Stages

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** Search for `psoDesc.GS = {nullptr, 0};` in `createRenderPipeline`

**Current (PROBLEM):**
```cpp
// Shader bytecode
psoDesc.VS = {vsBytecode.data(), vsBytecode.size()};
psoDesc.PS = {psBytecode.data(), psBytecode.size()};

// Explicitly zero unused shader stages
psoDesc.DS = {nullptr, 0};
psoDesc.HS = {nullptr, 0};
psoDesc.GS = {nullptr, 0};
```

**Fixed (SOLUTION):**
```cpp
// Shader bytecode - Vertex and Pixel (required)
psoDesc.VS = {vsBytecode.data(), vsBytecode.size()};
psoDesc.PS = {psBytecode.data(), psBytecode.size()};

// E-006: Optional shader stages (Geometry, Hull, Domain)
// Geometry shader (optional)
if (desc.geometryShader) {
  auto gsModule = std::static_pointer_cast<ShaderModule>(desc.geometryShader);
  const std::vector<uint8_t>& gsBytecode = gsModule->getBytecode();

  if (gsBytecode.empty()) {
    *outResult = Result(Result::Code::InvalidOperation,
                        "Geometry shader bytecode is empty");
    return nullptr;
  }

  psoDesc.GS = {gsBytecode.data(), gsBytecode.size()};
  IGL_LOG_INFO("createRenderPipeline: Geometry shader enabled (%zu bytes)\n",
               gsBytecode.size());
} else {
  psoDesc.GS = {nullptr, 0};
}

// Hull and Domain shaders (tessellation - must be specified together)
if (desc.hullShader && desc.domainShader) {
  auto hsModule = std::static_pointer_cast<ShaderModule>(desc.hullShader);
  auto dsModule = std::static_pointer_cast<ShaderModule>(desc.domainShader);

  const std::vector<uint8_t>& hsBytecode = hsModule->getBytecode();
  const std::vector<uint8_t>& dsBytecode = dsModule->getBytecode();

  if (hsBytecode.empty() || dsBytecode.empty()) {
    *outResult = Result(Result::Code::InvalidOperation,
                        "Hull or Domain shader bytecode is empty");
    return nullptr;
  }

  psoDesc.HS = {hsBytecode.data(), hsBytecode.size()};
  psoDesc.DS = {dsBytecode.data(), dsBytecode.size()};

  IGL_LOG_INFO("createRenderPipeline: Tessellation enabled (HS=%zu bytes, DS=%zu bytes)\n",
               hsBytecode.size(), dsBytecode.size());
} else if (desc.hullShader || desc.domainShader) {
  // Error: Hull and Domain shaders must be specified together
  *outResult = Result(Result::Code::InvalidOperation,
                      "Hull and Domain shaders must be specified together for tessellation");
  return nullptr;
} else {
  psoDesc.HS = {nullptr, 0};
  psoDesc.DS = {nullptr, 0};
}
```

**Rationale:**
- Conditionally populates GS/HS/DS fields if shaders are provided
- Validates that tessellation shaders are specified together (D3D12 requirement)
- Logs shader stage enablement for diagnostics
- Returns error if validation fails

---

#### Step 3: Update Root Signature for Tessellation

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** Search for `D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT`

**Current (PROBLEM):**
```cpp
D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
rootSigDesc.NumParameters = 7;
rootSigDesc.pParameters = rootParams;
rootSigDesc.NumStaticSamplers = 0;
rootSigDesc.pStaticSamplers = nullptr;
rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
// Missing tessellation flags!
```

**Fixed (SOLUTION):**
```cpp
D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
rootSigDesc.NumParameters = 7;
rootSigDesc.pParameters = rootParams;
rootSigDesc.NumStaticSamplers = 0;
rootSigDesc.pStaticSamplers = nullptr;

// E-006: Configure root signature flags based on shader stages
D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

// Deny shader access for stages not in use (optimization)
if (!desc.hullShader && !desc.domainShader) {
  flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
  flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
}

if (!desc.geometryShader) {
  flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
}

// Note: We don't deny VS/PS access since they're always present

rootSigDesc.Flags = flags;

IGL_LOG_INFO("createRenderPipeline: Root signature flags = 0x%08X\n", flags);
```

**Rationale:**
- Optimizes root signature by denying access to unused shader stages
- D3D12 best practice: Deny access to stages not in pipeline to reduce driver overhead
- Logs flags for diagnostic visibility

---

#### Step 4: Add Primitive Topology Validation for Tessellation

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** Search for `D3D12_PRIMITIVE_TOPOLOGY_TYPE` setup in PSO descriptor

**Current:**
```cpp
// Primitive topology
switch (desc.topology) {
  case PrimitiveType::Point:
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    break;
  case PrimitiveType::Line:
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    break;
  case PrimitiveType::Triangle:
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    break;
  // No patch topology!
}
```

**Fixed (SOLUTION):**
```cpp
// E-006: Primitive topology with tessellation support
if (desc.hullShader && desc.domainShader) {
  // Tessellation requires PATCH topology
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
  IGL_LOG_INFO("createRenderPipeline: Using PATCH topology for tessellation\n");
} else {
  // Standard topology (point/line/triangle)
  switch (desc.topology) {
    case PrimitiveType::Point:
      psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
      break;
    case PrimitiveType::Line:
      psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
      break;
    case PrimitiveType::Triangle:
      psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      break;
    default:
      psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      break;
  }
}
```

**Rationale:** Tessellation pipelines must use `D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH` topology type. Automatically select patch topology when tessellation shaders are present.

---

#### Step 5: Log Feature Support in validateDeviceLimits()

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** Search for `validateDeviceLimits` method

**Add Feature Logging:**
```cpp
void Device::validateDeviceLimits() {
  // ... existing limit validation ...

  // E-006: Log geometry and tessellation shader support
  IGL_LOG_INFO("\n=== Shader Stage Support ===\n");

  // All D3D12 Feature Level 10.0+ devices support geometry shaders
  IGL_LOG_INFO("  Geometry Shaders:  Supported (Feature Level 10.0+)\n");

  // All D3D12 Feature Level 11.0+ devices support tessellation
  IGL_LOG_INFO("  Tessellation (Hull/Domain Shaders): Supported (Feature Level 11.0+)\n");

  // Log feature level for reference
  // (Feature level detection already implemented in D3D12Context)
  IGL_LOG_INFO("  Device Feature Level: %s\n", /* feature level string */);

  IGL_LOG_INFO("========================================\n");
}
```

**Rationale:** Provides visibility into shader stage support during device initialization. All modern D3D12 devices support these stages (FL 10.0+ for GS, FL 11.0+ for tessellation).

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all D3D12 tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*"

# Pipeline creation tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Pipeline*"
```

**Test Modifications Allowed:**
- ✅ Add test for pipeline with geometry shader
- ✅ Add test for pipeline with tessellation shaders
- ✅ Add test for validation (e.g., hull without domain fails)
- ❌ **DO NOT modify cross-platform test logic**

**New Tests to Add:**
```cpp
// Test geometry shader pipeline creation
TEST(D3D12Device, CreatePipelineWithGeometryShader) {
  auto device = createTestDevice();

  auto vertexShader = createTestShader(device, ShaderStage::Vertex);
  auto fragmentShader = createTestShader(device, ShaderStage::Fragment);
  auto geometryShader = createTestShader(device, ShaderStage::Geometry);

  RenderPipelineDesc desc;
  desc.vertexShader = vertexShader;
  desc.fragmentShader = fragmentShader;
  desc.geometryShader = geometryShader;
  // ... other required fields ...

  Result result;
  auto pipeline = device->createRenderPipeline(desc, &result);

  EXPECT_TRUE(result.isOk());
  EXPECT_NE(pipeline, nullptr);
}

// Test tessellation pipeline creation
TEST(D3D12Device, CreatePipelineWithTessellation) {
  auto device = createTestDevice();

  auto vertexShader = createTestShader(device, ShaderStage::Vertex);
  auto fragmentShader = createTestShader(device, ShaderStage::Fragment);
  auto hullShader = createTestShader(device, ShaderStage::Hull);
  auto domainShader = createTestShader(device, ShaderStage::Domain);

  RenderPipelineDesc desc;
  desc.vertexShader = vertexShader;
  desc.fragmentShader = fragmentShader;
  desc.hullShader = hullShader;
  desc.domainShader = domainShader;
  // ... other required fields ...

  Result result;
  auto pipeline = device->createRenderPipeline(desc, &result);

  EXPECT_TRUE(result.isOk());
  EXPECT_NE(pipeline, nullptr);
}

// Test validation: Hull without Domain fails
TEST(D3D12Device, TessellationRequiresBothShaders) {
  auto device = createTestDevice();

  RenderPipelineDesc desc;
  desc.vertexShader = createTestShader(device, ShaderStage::Vertex);
  desc.fragmentShader = createTestShader(device, ShaderStage::Fragment);
  desc.hullShader = createTestShader(device, ShaderStage::Hull);
  desc.domainShader = nullptr; // Missing domain shader!

  Result result;
  auto pipeline = device->createRenderPipeline(desc, &result);

  EXPECT_FALSE(result.isOk());
  EXPECT_EQ(pipeline, nullptr);
}
```

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# Existing sessions should still pass (no geometry/tessellation used)
./build/Debug/RenderSessions.exe --session=TQSession
./build/Debug/RenderSessions.exe --session=HelloWorld
./build/Debug/RenderSessions.exe --session=Textured3DCube
```

**New Session (Optional):**
- Create test session with geometry shader (e.g., point sprite expansion)
- Create test session with tessellation (e.g., displacement mapped plane)

**Modifications Allowed:**
- ✅ Add new test sessions for geometry/tessellation
- ❌ **DO NOT modify existing session logic**

---

## Definition of Done

### Completion Criteria:

- [ ] All unit tests pass (existing + new geometry/tessellation tests)
- [ ] `RenderPipelineDesc` has geometry/hull/domain shader fields
- [ ] D3D12 pipeline creation populates GS/HS/DS in PSO descriptor
- [ ] Validation: Hull and Domain shaders must be specified together
- [ ] Root signature flags optimized based on active shader stages
- [ ] Patch topology automatically selected for tessellation pipelines
- [ ] Feature support logged in `validateDeviceLimits()`
- [ ] All existing render sessions pass
- [ ] Optional: New test sessions demonstrate geometry/tessellation
- [ ] Code review approved

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. Pipeline creation with geometry shader succeeds
2. Pipeline creation with tessellation shaders succeeds
3. Validation catches invalid configurations (e.g., hull without domain)
4. All existing render sessions pass

**Post in chat:**
```
E-006 Fix Complete - Ready for Review

Geometry/Tessellation Shader Support:
- IGL API: Added geometryShader, hullShader, domainShader to RenderPipelineDesc
- D3D12 Backend: PSO descriptor populates GS/HS/DS bytecode
- Validation: Hull+Domain must be specified together
- Root Signature: Optimized flags deny unused stages
- Topology: Automatic PATCH selection for tessellation

Testing Results:
- Unit tests: PASS (all D3D12 tests)
- Geometry shader test: PASS
- Tessellation test: PASS
- Validation test: PASS (rejects invalid configs)
- Render sessions: PASS (existing sessions unchanged)

Awaiting confirmation to proceed.
```

---

## Related Issues

### Blocks:
- None

### Related:
- **H-009** - Shader model support (geometry/tessellation require SM 5.0+)
- **P0_DX12-002** - Root signature caching (optimized root signatures for new stages)

---

## Implementation Priority

**Priority:** P1 - Medium (Engine Features)
**Estimated Effort:** 4-6 hours
**Risk:** Medium (Changes IGL core API; requires cross-backend testing)
**Impact:** Enables advanced rendering techniques; improves feature parity with other backends

**Notes:**
- Geometry shaders supported on Feature Level 10.0+ (all modern D3D12 devices)
- Tessellation supported on Feature Level 11.0+ (all modern D3D12 devices)
- Changing IGL core API (`RenderPipelineDesc`) requires coordination with other backends
- Other backends (Metal, Vulkan) may need updates to support new API fields
- Consider making this a cross-backend effort to ensure API consistency

**Implementation Strategy:**
1. Start with D3D12 backend implementation (Steps 2-5)
2. Test thoroughly with D3D12-specific tests
3. Update IGL core API (`RenderPipelineDesc`) last to avoid breaking other backends
4. Coordinate with Metal/Vulkan backend maintainers for parallel implementation

---

## References

- https://learn.microsoft.com/windows/win32/direct3d11/d3d10-graphics-programming-guide-geometry-shader
- https://learn.microsoft.com/windows/win32/direct3d11/direct3d-11-advanced-stages-tessellation
- https://learn.microsoft.com/windows/win32/api/d3d12/ns-d3d12-d3d12_graphics_pipeline_state_desc
- https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HDR
- https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_root_signature_flags
- https://learn.microsoft.com/windows/win32/api/d3d12/ne-d3d12-d3d12_primitive_topology_type

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
