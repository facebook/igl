# Task E-012: Domain/Hull Shader Implementation

## 1. Problem Statement

### Current Behavior
The D3D12 backend currently implements only vertex, pixel, and compute shaders. Domain and hull shaders (tessellation shaders) are not implemented, preventing the use of hardware tessellation features available in Direct3D 12.

### The Danger
Without domain/hull shader support:
- **Feature Gap**: Cannot leverage GPU tessellation for dynamic geometry generation
- **Performance Loss**: Applications requiring tessellation must implement workarounds or downgrade to CPU-based solutions
- **Scalability Limitation**: Cannot efficiently handle high-polygon meshes with LOD tessellation
- **Competitive Disadvantage**: Fails to match capabilities of other graphics APIs (Vulkan supports tessellation)
- **Non-Compliance**: Incomplete D3D12 shader stage support

### Impact
- Complex geometry cannot benefit from hardware tessellation
- Games and rendering applications cannot use displacement mapping efficiently
- Adaptive tessellation features unavailable
- API incompleteness reduces adoption

---

## 2. Root Cause Analysis

### Current Implementation Limitations

**File Location**: Search for: `ShaderStage` enum and shader creation

The current shader support:
```cpp
// Current implementation - limited shader stages
enum class ShaderStage {
    Vertex,      // VS - Supported
    Fragment,    // PS - Supported (called Fragment in cross-platform, PS in D3D12)
    Compute,     // CS - Supported
    // Missing:
    // Domain,    // DS - Not supported
    // Hull,      // HS - Not supported
};

// Shader creation only handles above stages
HRESULT Device::createShaderModule(const ShaderModuleDesc& desc) {
    switch (desc.shaderStage) {
        case ShaderStage::Vertex:
            // Compile and create vertex shader
            break;
        case ShaderStage::Fragment:
            // Compile and create pixel shader
            break;
        case ShaderStage::Compute:
            // Compile and create compute shader
            break;
        // No cases for Domain/Hull
    }
}
```

### Why It's Not Implemented

1. **Low Priority**: Tessellation is niche use case, lower demand than basic shaders
2. **Additional Complexity**: Requires pipeline state object changes
3. **Patch Control Point Configuration**: Must handle variable-sized input patches
4. **No Cross-Platform Mapping**: SPIR-V doesn't directly map to tessellation

### Relevant Code Locations
- Search for: `enum ShaderStage` - Shader stage definition
- Search for: `class ShaderModule` - Shader stage handling
- Search for: `D3D12_GRAPHICS_PIPELINE_STATE_DESC` - PSO creation (needs tessellation params)
- Search for: `createShaderModule` - Shader compilation entry point
- Search for: `ShaderModuleDesc` - Shader parameters struct

---

## 3. Official Documentation References

### Microsoft Direct3D 12 Documentation

1. **Tessellation Overview**
   - https://learn.microsoft.com/en-us/windows/win32/direct3d11/direct3d-11-advanced-stages-tessellation
   - Focus: Hull shader, domain shader, tessellation stage

2. **Pipeline State Object Creation**
   - https://learn.microsoft.com/en-us/windows/win32/direct3d12/managing-graphics-pipeline-state-in-direct3d-12
   - Focus: Graphics PSO with tessellation setup

3. **Patch Control Point Configuration**
   - https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/sm5-attributes-numcontrolpoints
   - Focus: Hull shader input/output patch configuration

4. **Tessellation Factor Specification**
   - https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/sm5-attributes-domain
   - Focus: Domain type (triangular, quad, isoline)

### Sample Code References
- **Microsoft Samples**: Direct3D 12 Tessellation
  - GitHub: https://github.com/microsoft/DirectX-Graphics-Samples
  - File: `Samples/Desktop/D3D12Fullscreen12/src/TessellationShaders.cpp` (if available)

---

## 4. Code Location Strategy

### Pattern-Based File Search

**Search for these patterns to understand current structure:**

1. **Shader Stage Enum Definition**
   - Search for: `enum class ShaderStage` or `enum ShaderStage`
   - File: `include/igl/ShaderTypes.h` or similar
   - What to look for: Current stage values (Vertex, Fragment, Compute)

2. **Shader Module Class**
   - Search for: `class ShaderModule : public IShaderModule`
   - File: `src/igl/d3d12/ShaderModule.h`
   - What to look for: Stage handling, shader object creation

3. **Shader Compilation**
   - Search for: `D3DCompile` or `compileShader`
   - File: `src/igl/d3d12/ShaderCompiler.cpp`
   - What to look for: Compiler target profile selection logic

4. **Pipeline State Object Creation**
   - Search for: `D3D12_GRAPHICS_PIPELINE_STATE_DESC` or `createGraphicsPipeline`
   - File: `src/igl/d3d12/RenderPipeline.cpp` or `Device.cpp`
   - What to look for: PSO setup with shader stages

5. **RenderPass/Command Binding**
   - Search for: `setRenderPipeline` or `commandBuffer->bindPipeline`
   - File: `src/igl/d3d12/CommandBuffer.cpp`
   - What to look for: Pipeline binding with shader stages

---

## 5. Detection Strategy

### How to Identify the Problem

**Reproduction Steps:**
1. Create a simple hull shader:
   ```hlsl
   // hullShader.hlsl - Tessellation hull shader
   [domain("tri")]
   [partitioning("integer")]
   [outputtopology("triangle_cw")]
   [outputcontrolpoints(3)]
   [patchconstantfunc("PatchConstantFunc")]
   HS_OUT main(InputPatch<VS_OUT, 3> patch, uint index : SV_OutputControlPointID) {
       HS_OUT output;
       output.pos = patch[index].pos;
       return output;
   }
   ```

2. Create a simple domain shader:
   ```hlsl
   // domainShader.hlsl - Tessellation domain shader
   [domain("tri")]
   float4 main(HS_CONST_OUTPUT input, OutputPatch<HS_OUT, 3> patch, float3 uvw : SV_DomainLocation) : SV_Position {
       return patch[0].pos * uvw.x + patch[1].pos * uvw.y + patch[2].pos * uvw.z;
   }
   ```

3. Attempt to create shader modules:
   ```cpp
   ShaderModuleDesc hullDesc{ShaderStage::Hull, hullSource};
   IShaderModule* hullModule = nullptr;
   Result res = device->createShaderModule(hullDesc, &hullModule);
   // Expected: Success
   // Actual: Error - ShaderStage::Hull not defined
   ```

4. Test pipeline creation:
   - Run: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Tessellation*"`
   - Look for: No tessellation tests exist

### Success Criteria
- ShaderStage::Domain and ShaderStage::Hull values available
- Hull/Domain shaders compile with D3DCompile using correct profiles
- Pipeline state objects accept hull/domain shaders
- Render passes bind hull/domain shaders correctly
- Patch control point configuration working

---

## 6. Fix Guidance

### Implementation Steps

#### Step 1: Add Shader Stage Enums
```cpp
// In include/igl/ShaderTypes.h - Extend ShaderStage enum
enum class ShaderStage {
    Vertex,
    Fragment,
    Compute,
    Domain,   // NEW: Tessellation domain shader
    Hull,     // NEW: Tessellation hull shader
};

// Add helper functions
constexpr bool isTessellationStage(ShaderStage stage) {
    return stage == ShaderStage::Domain || stage == ShaderStage::Hull;
}

constexpr const char* shaderStageToProfilePrefix(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex: return "vs";
        case ShaderStage::Fragment: return "ps";
        case ShaderStage::Compute: return "cs";
        case ShaderStage::Domain: return "ds";    // NEW
        case ShaderStage::Hull: return "hs";      // NEW
    }
    return nullptr;
}
```

#### Step 2: Update Shader Module Enum
```cpp
// In src/igl/d3d12/ShaderModule.h - Add member variables
class ShaderModule : public IShaderModule {
public:
    // Existing members...

    // NEW: Tessellation-specific data
    struct TessellationInfo {
        D3D12_HS_OUTPUT_PRIMITIVE outputPrimitive;  // D3D12_HS_OUTPUT_PRIMITIVE_LINE, etc.
        uint32_t outputControlPoints;  // Number of control points from hull shader
        uint32_t inputControlPoints;   // Number of input control points
    };

    const TessellationInfo& getTessellationInfo() const { return tessInfo_; }

private:
    TessellationInfo tessInfo_;
};
```

#### Step 3: Extend Shader Compilation
```cpp
// In src/igl/d3d12/ShaderCompiler.cpp - Add compilation profiles
const char* ShaderCompiler::getTargetProfile(ShaderStage stage,
                                             FeatureLevel featureLevel) {
    const char* prefix = shaderStageToProfilePrefix(stage);
    const char* version = (featureLevel >= FeatureLevel::SM60) ? "6_0" : "5_0";

    // Build profile string: "ps_5_0", "hs_5_0", etc.
    static std::string profile;
    profile = std::string(prefix) + "_" + version;
    return profile.c_str();
}

// Implement compilation for tessellation stages
HRESULT ShaderCompiler::compileHullShader(const std::string& source,
                                         const std::string& entryPoint,
                                         ID3DBlob** outBlob) {
    return D3DCompile(
        source.data(), source.size(),
        nullptr, nullptr, nullptr,
        entryPoint.c_str(),
        "hs_5_0",  // Hull shader profile
        D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0,
        outBlob,
        &errorBlob_
    );
}

HRESULT ShaderCompiler::compileDomainShader(const std::string& source,
                                           const std::string& entryPoint,
                                           ID3DBlob** outBlob) {
    return D3DCompile(
        source.data(), source.size(),
        nullptr, nullptr, nullptr,
        entryPoint.c_str(),
        "ds_5_0",  // Domain shader profile
        D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0,
        outBlob,
        &errorBlob_
    );
}
```

#### Step 4: Update Pipeline State Object Creation
```cpp
// In src/igl/d3d12/RenderPipeline.cpp - Add tessellation to PSO
void RenderPipeline::createGraphicsPipeline() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    // ... existing setup ...

    // NEW: Add tessellation shaders if present
    if (vertexModule_) psoDesc.VS = vertexModule_->getShaderBytecode();
    if (hullModule_) psoDesc.HS = hullModule_->getShaderBytecode();
    if (domainModule_) psoDesc.DS = domainModule_->getShaderBytecode();
    if (fragmentModule_) psoDesc.PS = fragmentModule_->getShaderBytecode();

    // NEW: Configure tessellation if hull shader present
    if (hullModule_) {
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
        // Number of control points must match hull shader output
        // This is validated during pipeline creation
    }

    device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
}
```

#### Step 5: Update Command Binding
```cpp
// In src/igl/d3d12/CommandBuffer.cpp - Ensure tessellation support in binding
void CommandBuffer::setRenderPipeline(const IRenderPipeline* pipeline) {
    auto* d3d12Pipeline = static_cast<const RenderPipeline*>(pipeline);

    // Existing binding logic applies to hull/domain shaders automatically
    // through PSO binding
    commandList_->SetPipelineState(d3d12Pipeline->getPipelineState());

    // NEW: Set primitive topology based on tessellation
    D3D12_PRIMITIVE_TOPOLOGY topology = d3d12Pipeline->getPrimitiveTopology();
    commandList_->IASetPrimitiveTopology(topology);
}
```

#### Step 6: Add Tessellation Validation
```cpp
// In src/igl/d3d12/Device.cpp - Validate tessellation capabilities
Result Device::validateTessellationSupport(const RenderPipelineDesc& desc) {
    if (desc.hullShaderModule || desc.domainShaderModule) {
        // Check minimum feature level
        if (featureLevel_ < D3D_FEATURE_LEVEL_11_0) {
            return Result(Result::Code::NotSupported,
                         "Tessellation requires Feature Level 11_0 or higher");
        }

        // Validate shader pairing
        if ((bool)desc.hullShaderModule != (bool)desc.domainShaderModule) {
            return Result(Result::Code::InvalidOperation,
                         "Hull and domain shaders must be paired");
        }

        // Validate control point count
        if (!validateControlPointCount(desc.hullShaderModule)) {
            return Result(Result::Code::InvalidOperation,
                         "Invalid hull shader control point configuration");
        }
    }
    return Result();
}
```

---

## 7. Testing Requirements

### Unit Tests to Create/Modify

**File**: `tests/d3d12/TessellationShaderTests.cpp` (NEW)

1. **Hull Shader Compilation Test**
   ```cpp
   TEST(D3D12Tessellation, CompileHullShader) {
       const std::string hullSource = R"(
           [domain("tri")]
           [partitioning("integer")]
           [outputtopology("triangle_cw")]
           [outputcontrolpoints(3)]
           [patchconstantfunc("PatchFunc")]
           HS_OUT main(InputPatch<VS_OUT, 3> patch, uint index : SV_OutputControlPointID) {
               HS_OUT output;
               output.pos = patch[index].pos;
               return output;
           }
       )";

       ShaderModuleDesc desc{ShaderStage::Hull, hullSource};
       IShaderModule* module = nullptr;
       ASSERT_EQ(device->createShaderModule(desc, &module), Result::Code::Ok);
       ASSERT_NE(module, nullptr);
   }
   ```

2. **Domain Shader Compilation Test**
   ```cpp
   TEST(D3D12Tessellation, CompileDomainShader) {
       const std::string domainSource = R"(
           [domain("tri")]
           float4 main(HS_CONST_OUTPUT input, OutputPatch<HS_OUT, 3> patch,
                      float3 uvw : SV_DomainLocation) : SV_Position {
               return patch[0].pos * uvw.x + patch[1].pos * uvw.y + patch[2].pos * uvw.z;
           }
       )";

       ShaderModuleDesc desc{ShaderStage::Domain, domainSource};
       IShaderModule* module = nullptr;
       ASSERT_EQ(device->createShaderModule(desc, &module), Result::Code::Ok);
       ASSERT_NE(module, nullptr);
   }
   ```

3. **Tessellation Pipeline Creation Test**
   ```cpp
   TEST(D3D12Tessellation, CreateTessellationPipeline) {
       // Create vertex, hull, domain, and fragment shaders
       auto vertexModule = createVertexShader(...);
       auto hullModule = createHullShader(...);
       auto domainModule = createDomainShader(...);
       auto fragmentModule = createFragmentShader(...);

       RenderPipelineDesc pipelineDesc;
       pipelineDesc.vertexShaderModule = vertexModule;
       pipelineDesc.hullShaderModule = hullModule;
       pipelineDesc.domainShaderModule = domainModule;
       pipelineDesc.fragmentShaderModule = fragmentModule;

       IRenderPipeline* pipeline = nullptr;
       ASSERT_EQ(device->createRenderPipeline(pipelineDesc, &pipeline),
                 Result::Code::Ok);
   }
   ```

4. **Tessellation Validation Test**
   ```cpp
   TEST(D3D12Tessellation, ValidateTessellationPairing) {
       // Hull without domain should fail
       RenderPipelineDesc desc;
       desc.hullShaderModule = createHullShader(...);
       desc.domainShaderModule = nullptr;

       IRenderPipeline* pipeline = nullptr;
       ASSERT_NE(device->createRenderPipeline(desc, &pipeline), Result::Code::Ok);
   }
   ```

### Integration Tests

**Command**: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Tessellation*"`

- Run all tessellation tests
- Verify: 100% pass rate

**Command**: `./test_all_sessions.bat`

- Ensure no regressions in existing rendering
- Verify: No crashes or validation errors

### Restrictions
- ❌ DO NOT modify cross-platform shader interface in `include/igl/IShaderModule.h` unless necessary
- ❌ DO NOT change vertex/fragment shader handling
- ❌ DO NOT add tessellation to shader stage enum in cross-platform code without discussion
- ✅ DO focus changes on D3D12-specific `src/igl/d3d12/` files

---

## 8. Definition of Done

### Completion Checklist

- [ ] ShaderStage::Domain and ShaderStage::Hull added to enum
- [ ] Hull shader compilation implemented with correct profile (hs_5_0)
- [ ] Domain shader compilation implemented with correct profile (ds_5_0)
- [ ] RenderPipelineDesc extended to accept hull/domain shader modules
- [ ] Graphics PSO creation updated for tessellation (primitive topology, etc.)
- [ ] Tessellation validation (pairing, control points) implemented
- [ ] All new unit tests pass: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Tessellation*"`
- [ ] No regressions in existing tests
- [ ] Integration tests pass: `./test_all_sessions.bat`
- [ ] Sample tessellation demo created or documented
- [ ] Code reviewed and approved by maintainer

### User Confirmation Required
⚠️ **STOP** - Do NOT proceed to next task until user confirms:
- "✅ I have tested hull and domain shader compilation"
- "✅ Tessellation pipeline creation works correctly"
- "✅ No regressions in existing rendering functionality"

---

## 9. Related Issues

### Blocks
- **E-013**: Advanced shader features (geometry shaders, stream output)
- **E-015**: Geometry rendering optimization (can use tessellation)

### Related
- **E-007**: Shader validation
- **E-008**: Shader include paths
- **C-002**: Shader pipeline management

---

## 10. Implementation Priority

### Severity: **Low** | Priority: **P2-Low**

**Effort**: 50-70 hours
- Design: 10 hours (tessellation pipeline design)
- Implementation: 28-36 hours (shaders + PSO + validation)
- Testing: 8-12 hours (unit + integration tests)
- Documentation: 4-6 hours

**Risk**: **Medium**
- Risk of complex PSO state if not properly configured
- Risk of validation errors with control point counts
- Mitigation: Comprehensive validation and test coverage

**Impact**: **Low-Medium**
- Enables hardware tessellation features
- Supports advanced rendering techniques
- Niche feature with limited immediate adoption

**Blockers**: None

**Dependencies**:
- Requires D3D Feature Level 11_0 or higher
- Requires existing shader compilation infrastructure

---

## 11. References

### Official Microsoft Documentation
1. **Tessellation Overview**: https://learn.microsoft.com/en-us/windows/win32/direct3d11/direct3d-11-advanced-stages-tessellation
2. **Pipeline State Objects**: https://learn.microsoft.com/en-us/windows/win32/direct3d12/managing-graphics-pipeline-state-in-direct3d-12
3. **Hull Shader**: https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/hull-shader
4. **Domain Shader**: https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/domain-shader

### GitHub References
- **Microsoft D3D12 Samples**: https://github.com/microsoft/DirectX-Graphics-Samples
- **IGL Repository**: https://github.com/facebook/igl

### Related Issues
- Task E-007: Shader validation
- Task E-013: Advanced shader features
