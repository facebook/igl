# E-011: Shader Debug Names Not Preserved

**Priority:** P1-Medium
**Category:** Enhancement - Debugging
**Status:** Open
**Effort:** Small (1-2 days)

---

## 1. Problem Statement

The D3D12 backend does not preserve or propagate shader debug names through the compilation and pipeline creation process. When debugging with PIX, RenderDoc, or Visual Studio Graphics Debugger, shaders appear as unnamed objects, making it difficult to identify which shader is executing or causing problems.

### The Danger

**Debugging complexity multiplied** when analyzing GPU captures:
- PIX shows "Unnamed Pixel Shader #7" instead of "TerrainShader"
- RenderDoc displays "PSO 0x000001A4B5C8F2E0" instead of meaningful pipeline names
- GPU crash dumps reference memory addresses, not shader names
- No way to correlate shader errors with source files
- Performance analysis can't attribute costs to specific shaders

**Real-world scenario:** Application crashes in production with D3D12 error:
```
D3D12 ERROR: ID3D12CommandList::DrawInstanced: Shader (0x000001A4B5C8F2E0)
encountered an error. [ EXECUTION ERROR #1234: CORRUPTED_PARAMETER]
```

Without debug names:
- **30-60 minutes** to figure out which shader has the bug
- Need to cross-reference memory addresses with dump files
- Reproduce issue multiple times to narrow down suspect shaders

With debug names:
- Error shows "Shader: WaterReflection.hlsl (Pixel Shader)"
- **Immediate identification** of problematic shader
- Fix applied in minutes instead of hours

---

## 2. Root Cause Analysis

The D3D12 backend creates shader objects without setting D3D12 debug names:

**Current Implementation:**
```cpp
// In ShaderModule.cpp - creates shader but doesn't name it
ComPtr<ID3DBlob> shaderBlob;
HRESULT hr = D3DCompile(source.c_str(), ...);

// Shader blob created but never named
// When used in pipeline, pipeline also unnamed

// In RenderPipelineState.cpp
ComPtr<ID3D12PipelineState> pso;
device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
// ← PSO created but SetName() never called!
```

**Missing Propagation Chain:**

1. **ShaderModuleDesc → D3DBlob:** Debug name not attached to blob
2. **ShaderModule → Pipeline:** Name not passed during pipeline creation
3. **Pipeline → D3D12 Object:** SetName() never called on ID3D12PipelineState
4. **Shader reflection → Metadata:** Name not stored for later retrieval

**D3D12 Object Naming API (Not Used):**
```cpp
// Available but unused:
ID3D12Object::SetName(const wchar_t* name)
ID3D12Object::SetPrivateData(GUID, data)  // Could store shader metadata
```

---

## 3. Official Documentation References

- **ID3D12Object::SetName:**
  https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12object-setname

- **PIX for Windows Markers and Names:**
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/user-mode-diagnostics

- **Direct3D 12 Debugging Tools:**
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/understanding-the-d3d12-debug-layer

- **Graphics Debugging with Visual Studio:**
  https://learn.microsoft.com/en-us/visualstudio/debugger/graphics/overview-of-visual-studio-graphics-diagnostics

---

## 4. Code Location Strategy

**Primary Files to Modify:**

1. **Shader module creation:**
   ```
   Search for: "class ShaderModule" or "ShaderModule::create"
   In files: src/igl/d3d12/ShaderModule.h, src/igl/d3d12/ShaderModule.cpp
   Pattern: Look for shader blob creation and storage
   ```

2. **Pipeline state creation:**
   ```
   Search for: "CreateGraphicsPipelineState" or "CreateComputePipelineState"
   In files: src/igl/d3d12/RenderPipelineState.cpp, src/igl/d3d12/ComputePipelineState.cpp
   Pattern: Look for ID3D12PipelineState creation
   ```

3. **Shader module descriptor:**
   ```
   Search for: "struct ShaderModuleDesc" or "debugName"
   In files: include/igl/Shader.h
   Pattern: Look for existing debugName field (cross-platform)
   ```

4. **Pipeline descriptor:**
   ```
   Search for: "struct RenderPipelineDesc" or "ComputePipelineDesc"
   In files: include/igl/RenderPipelineState.h, include/igl/ComputePipelineState.h
   Pattern: Look for debug name or metadata fields
   ```

---

## 5. Detection Strategy

### Reproduction Steps:

1. **Create shader with debug name:**
   ```cpp
   ShaderModuleDesc desc;
   desc.source = vertexShaderSource;
   desc.entryPoint = "main";
   desc.stage = ShaderStage::Vertex;
   desc.debugName = "MyVertexShader";  // ← Currently ignored!

   auto shaderModule = device->createShaderModule(desc);
   ```

2. **Create pipeline and capture with PIX:**
   ```bash
   # Run application under PIX
   "C:\Program Files\Microsoft PIX\PIX.exe" /captureOnStartup /targetExecutable:./build/Debug/Session/TinyMeshSession.exe
   ```

3. **Inspect pipeline in PIX:**
   ```
   Current: Pipeline State Object shows "0x000001A4..."
   Expected: Pipeline State Object shows "MyVertexShader + MyPixelShader"
   ```

4. **Check D3D12 debug layer output:**
   ```bash
   # With D3D12 debug layer enabled
   set D3D12_DEBUG=1
   ./build/Debug/Session/TinyMeshSession.exe

   # Current: Warnings show unnamed objects
   # Expected: Warnings reference shader by name
   ```

### Validation with PIX:

```
1. Capture frame in PIX
2. Navigate to Pipeline section
3. Current: See "Pipeline State 0x..."
4. Expected: See "Pipeline: TerrainVS + TerrainPS"
5. Click on shader
6. Current: No name in shader view
7. Expected: "Shader: TerrainVS (Vertex Shader) from terrain.hlsl"
```

---

## 6. Fix Guidance

### Step 1: Store Debug Name in ShaderModule

```cpp
// In ShaderModule.h
class ShaderModule final : public IShaderModule {
 public:
  ShaderModule(ComPtr<ID3DBlob> blob, const std::string& debugName);

  const std::string& getDebugName() const { return debugName_; }

 private:
  ComPtr<ID3DBlob> blob_;
  std::string debugName_;  // ← Add storage for debug name
  std::string entryPoint_;
  ShaderStage stage_;
};

// In ShaderModule.cpp
ShaderModule::ShaderModule(ComPtr<ID3DBlob> blob, const std::string& debugName)
    : blob_(std::move(blob))
    , debugName_(debugName) {
  // Store debug name for later use
}
```

### Step 2: Set Name on Pipeline State Objects

```cpp
// In RenderPipelineState.cpp
Result<std::shared_ptr<IRenderPipelineState>> Device::createRenderPipeline(
    const RenderPipelineDesc& desc,
    Result* outResult) {

  // ... existing pipeline creation ...

  ComPtr<ID3D12PipelineState> pso;
  HRESULT hr = device_->CreateGraphicsPipelineState(&d3dDesc, IID_PPV_ARGS(&pso));

  if (FAILED(hr)) {
    return Result<...>(Result::Code::RuntimeError, "Failed to create PSO");
  }

  // NEW: Set debug name on PSO
  std::string psoName = buildPipelineName(desc);
  if (!psoName.empty()) {
    std::wstring wideName = stringToWideString(psoName);
    pso->SetName(wideName.c_str());
  }

  // ... continue with pipeline state creation ...
}

// Helper function to build meaningful pipeline name
std::string buildPipelineName(const RenderPipelineDesc& desc) {
  std::ostringstream name;

  // Combine shader names
  if (desc.vertexShader) {
    auto vsModule = static_cast<ShaderModule*>(desc.vertexShader.get());
    name << vsModule->getDebugName();
  }

  if (desc.fragmentShader) {
    auto psModule = static_cast<ShaderModule*>(desc.fragmentShader.get());
    if (name.tellp() > 0) {
      name << " + ";
    }
    name << psModule->getDebugName();
  }

  // Add debug name from pipeline desc if available
  if (!desc.debugName.empty()) {
    if (name.tellp() > 0) {
      name << " (";
    }
    name << desc.debugName;
    if (name.tellp() > 0) {
      name << ")";
    }
  }

  return name.str();
}

// Utility for string conversion
std::wstring stringToWideString(const std::string& str) {
  if (str.empty()) return std::wstring();

  int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
  std::wstring wstr(size, 0);
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
  return wstr;
}
```

### Step 3: Set Names on Compute Pipelines

```cpp
// In ComputePipelineState.cpp
Result<std::shared_ptr<IComputePipelineState>> Device::createComputePipeline(
    const ComputePipelineDesc& desc,
    Result* outResult) {

  // ... existing pipeline creation ...

  ComPtr<ID3D12PipelineState> pso;
  HRESULT hr = device_->CreateComputePipelineState(&d3dDesc, IID_PPV_ARGS(&pso));

  if (FAILED(hr)) {
    return Result<...>(Result::Code::RuntimeError, "Failed to create compute PSO");
  }

  // Set debug name
  std::string psoName;
  if (desc.computeShader) {
    auto csModule = static_cast<ShaderModule*>(desc.computeShader.get());
    psoName = csModule->getDebugName();
  }
  if (!desc.debugName.empty()) {
    if (!psoName.empty()) {
      psoName += " (" + desc.debugName + ")";
    } else {
      psoName = desc.debugName;
    }
  }

  if (!psoName.empty()) {
    std::wstring wideName = stringToWideString(psoName);
    pso->SetName(wideName.c_str());
  }

  // ... continue ...
}
```

### Step 4: Add Debug Name to Pipeline Descriptors

```cpp
// In include/igl/RenderPipelineState.h (cross-platform header)
struct RenderPipelineDesc {
  std::shared_ptr<IShaderModule> vertexShader;
  std::shared_ptr<IShaderModule> fragmentShader;
  // ... existing fields ...

  std::string debugName;  // ← Add if doesn't exist
};

// In include/igl/ComputePipelineState.h
struct ComputePipelineDesc {
  std::shared_ptr<IShaderModule> computeShader;
  // ... existing fields ...

  std::string debugName;  // ← Add if doesn't exist
};
```

### Step 5: Set Names on D3D12 Shader Blobs (Advanced)

```cpp
// In ShaderModule.cpp - after compilation
Result<std::shared_ptr<IShaderModule>> Device::createShaderModule(
    const ShaderModuleDesc& desc,
    Result* outResult) {

  // ... compile shader ...

  ComPtr<ID3DBlob> shaderBlob = /* compiled */;

  // Store metadata with blob using private data (for advanced tooling)
  if (!desc.debugName.empty()) {
    struct ShaderMetadata {
      char name[256];
      uint32_t stage;
    } metadata = {};

    strncpy_s(metadata.name, sizeof(metadata.name),
              desc.debugName.c_str(), _TRUNCATE);
    metadata.stage = static_cast<uint32_t>(desc.stage);

    // Custom GUID for IGL shader metadata
    static const GUID IGL_SHADER_METADATA_GUID = {
      0x12345678, 0x1234, 0x5678,
      {0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF}
    };

    // Note: ID3DBlob doesn't have SetPrivateData, but ID3D12ShaderReflection does
    // This is for future extensibility
  }

  return std::make_shared<ShaderModule>(shaderBlob, desc.debugName);
}
```

### Step 6: Propagate Names to Other D3D12 Objects

```cpp
// Optionally name other objects for better debugging

// Root signatures
void setRootSignatureName(ID3D12RootSignature* rootSig, const std::string& name) {
  if (rootSig && !name.empty()) {
    std::wstring wideName = stringToWideString(name);
    rootSig->SetName(wideName.c_str());
  }
}

// Command lists (if created per-pass with specific names)
void setCommandListName(ID3D12GraphicsCommandList* cmdList, const std::string& name) {
  if (cmdList && !name.empty()) {
    std::wstring wideName = stringToWideString(name);
    cmdList->SetName(wideName.c_str());
  }
}
```

---

## 7. Testing Requirements

### Unit Tests:

```bash
# Run shader module creation tests
./build/Debug/IGLTests.exe --gtest_filter="*ShaderModule*"

# Run pipeline creation tests
./build/Debug/IGLTests.exe --gtest_filter="*Pipeline*"
```

### Test Cases:

```cpp
TEST(ShaderModule, StoresDebugName) {
  ShaderModuleDesc desc;
  desc.debugName = "TestShader";
  desc.source = simpleShaderSource;
  desc.stage = ShaderStage::Vertex;

  auto shader = device->createShaderModule(desc);
  ASSERT_TRUE(shader);

  auto d3dShader = static_cast<ShaderModule*>(shader.get());
  EXPECT_EQ(d3dShader->getDebugName(), "TestShader");
}

TEST(RenderPipeline, SetsNameOnPSO) {
  // Create pipeline with named shaders
  RenderPipelineDesc desc;
  desc.vertexShader = createTestShader("MyVertexShader", ShaderStage::Vertex);
  desc.fragmentShader = createTestShader("MyPixelShader", ShaderStage::Fragment);
  desc.debugName = "TestPipeline";

  auto pipeline = device->createRenderPipeline(desc);
  ASSERT_TRUE(pipeline);

  // Verify name was set (requires accessing underlying D3D12 object)
  auto d3dPipeline = static_cast<RenderPipelineState*>(pipeline.get());
  auto pso = d3dPipeline->getPSO();

  // Can't directly query name, but can verify through PIX or debug layer
  EXPECT_TRUE(pso != nullptr);
}
```

### PIX Validation:

```
1. Build test application with debug names:
   ./build/Debug/Session/TinyMeshSession.exe

2. Capture frame in PIX

3. Verify pipelines show meaningful names:
   - Should see shader names, not memory addresses
   - Pipeline list should show "VS_Main + PS_Main" not "PSO 0x..."

4. Click on shader in pipeline:
   - Shader panel should show debug name
   - Should correlate with source file name
```

### Integration Tests:

```bash
# Test all sessions work with debug names
./test_all_sessions.bat

# Specific sessions with many shaders
./build/Debug/Session/TextureArraySession.exe
./build/Debug/Session/TinyMeshSession.exe
```

### Modification Restrictions:

- **DO NOT modify** cross-platform IGL API semantics
- **DO NOT change** shader compilation behavior
- **DO NOT break** existing pipelines without debug names (graceful fallback)
- **ONLY add** name propagation, no functional changes

---

## 8. Definition of Done

### Validation Checklist:

- [ ] ShaderModule stores and returns debug name
- [ ] Debug name propagated from ShaderModuleDesc to ShaderModule
- [ ] Pipeline state objects have SetName() called with meaningful names
- [ ] Compute pipelines also have debug names set
- [ ] All unit tests pass
- [ ] PIX shows shader names instead of addresses
- [ ] Works with both debug and release builds
- [ ] Graceful handling when debug name is empty (use fallback)

### PIX/RenderDoc Validation:

- [ ] Pipeline names visible in PIX capture
- [ ] Shader names visible in shader views
- [ ] Debug layer messages reference shader by name (if applicable)

### User Confirmation Required:

**STOP - Do NOT proceed to next task until user confirms:**
- [ ] PIX/RenderDoc captures show meaningful shader names
- [ ] Debugging session demonstrates improved identification
- [ ] No regression in shader functionality or performance
- [ ] All render sessions work correctly

---

## 9. Related Issues

### Blocks:
- None - Standalone enhancement

### Related:
- **E-010** - Shader error messages unclear (error messages can use debug names)
- **E-007** - Shader reflection incomplete (reflection could include debug name)
- **H-016** - PIX event markers (both improve GPU debugging experience)

### Depends On:
- None - Can be implemented independently

---

## 10. Implementation Priority

**Priority:** P1-Medium

**Effort Estimate:** 1-2 days
- Day 1: Implement debug name storage and propagation
- Day 2: Testing with PIX/RenderDoc, polish

**Risk Assessment:** Very Low
- Changes are purely additive (naming only)
- No impact on shader compilation or pipeline functionality
- Easy to verify with debugging tools
- Zero runtime performance impact

**Impact:** Medium-High
- **Massive improvement in debugging efficiency** (hours → minutes)
- **Better GPU debugging experience** with PIX/RenderDoc/Visual Studio
- **Easier production issue diagnosis** with meaningful crash dumps
- **Improved team collaboration** (clearer what each shader does)
- Industry best practice compliance

---

## 11. References

### Official Documentation:

1. **ID3D12Object::SetName:**
   https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12object-setname

2. **PIX for Windows:**
   https://devblogs.microsoft.com/pix/download/

3. **User Mode Diagnostics:**
   https://learn.microsoft.com/en-us/windows/win32/direct3d12/user-mode-diagnostics

4. **Graphics Diagnostics in Visual Studio:**
   https://learn.microsoft.com/en-us/visualstudio/debugger/graphics/overview-of-visual-studio-graphics-diagnostics

5. **D3D12 Debug Layer:**
   https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer

### Additional Resources:

6. **RenderDoc D3D12 Support:**
   https://renderdoc.org/docs/how/how_d3d12_marker.html

7. **DirectX Graphics Samples:**
   https://github.com/microsoft/DirectX-Graphics-Samples

---

**Last Updated:** 2025-11-12
**Assignee:** Unassigned
**Estimated Completion:** 1-2 days after start
