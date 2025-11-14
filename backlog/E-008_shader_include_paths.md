# Task E-008: Configurable Shader Include Paths

## 1. Problem Statement

### Current Behavior
The D3D12 shader compilation system currently supports shader compilation but does not provide a configurable mechanism for shader include paths. Include paths are either hardcoded or rely on environment variables, preventing flexible shader code organization and reusable shader libraries.

### The Danger
Without configurable shader include paths:
- **Code Reusability**: Shared shader utilities (lighting models, math functions) cannot be easily included across multiple shader files
- **Project Organization**: Complex shaders must be monolithic rather than modular
- **Development Friction**: Developers cannot organize shader files in logical directory structures
- **Library Distribution**: Third-party shader libraries cannot be properly integrated
- **Non-Compliance**: Fails to match D3D12 compilation best practices and HLSL compiler capabilities

### Impact
- Shader codebases become bloated and difficult to maintain
- Code duplication across shader files increases bugs
- No standardized way to distribute shader libraries
- Development speed decreases for projects with many shaders

---

## 2. Root Cause Analysis

### Current Implementation Limitations

**File Location**: Search for: `ShaderCompiler` class and `D3DCompile` wrapper

The current shader compilation approach:
```cpp
// Current approach - no include path configuration
HRESULT ShaderCompiler::compile(const ShaderStage& stage, const std::string& source) {
    // Fixed compilation without include path support
    return D3DCompile(
        source.data(),
        source.size(),
        nullptr,  // No include paths configured
        nullptr,  // No defines
        nullptr   // No custom include handler
    );
}
```

### Why It's Not Optimal

1. **No Include Handler**: D3D12 compiler not configured with custom include handler
2. **Static Paths Only**: Cannot pass runtime-configurable paths to shader compilation
3. **No Library Support**: No mechanism for shader library includes
4. **Limited Flexibility**: HLSL `/I` flag equivalent not exposed to API users

### Relevant Code Locations
- Search for: `class ShaderCompiler` - Shader compilation wrapper
- Search for: `D3DCompile` call - Direct3D compilation invocation
- Search for: `ShaderDesc` - Shader creation parameters
- Search for: `IIncludeHandler` or similar - Include resolution (may not exist)

---

## 3. Official Documentation References

### Microsoft Direct3D 12 Documentation

1. **HLSL Compiler Reference**
   - https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-reference
   - Focus: Compiler options and include path handling

2. **D3DCompile Function**
   - https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3dcompile
   - Focus: Compiler options, include handler parameter

3. **ID3DInclude Interface**
   - https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nn-d3dcompiler-id3dinclude
   - Focus: Custom include handler implementation

4. **HLSL Shader Compilation Best Practices**
   - https://learn.microsoft.com/en-us/windows/win32/direct3d12/shader-validation
   - Focus: Modular shader organization and compilation

### Sample Code References
- **Microsoft Samples**: D3D12HelloWorld - Shader Management
  - GitHub: https://github.com/microsoft/DirectX-Graphics-Samples
  - File: `Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/HelloTriangle.cpp`

---

## 4. Code Location Strategy

### Pattern-Based File Search

**Search for these patterns to understand current structure:**

1. **Shader Compiler Implementation**
   - Search for: `class ShaderCompiler` or `D3DCompile`
   - File: `src/igl/d3d12/ShaderCompiler.*` (or similar)
   - What to look for: D3DCompile invocation, compiler flags setup

2. **Shader Stage Creation**
   - Search for: `createShaderStage(` or `ShaderStageDesc`
   - File: `src/igl/d3d12/Device.cpp` and `src/igl/d3d12/ShaderModule.*`
   - What to look for: Shader compilation entry point

3. **Device/Context Configuration**
   - Search for: `class Device` constructor
   - File: `src/igl/d3d12/Device.h`
   - What to look for: Shader configuration options in device creation

4. **Include Handler (if exists)**
   - Search for: `ID3DInclude` implementation
   - File: May not exist yet - would be `src/igl/d3d12/ShaderIncludeHandler.*`
   - What to look for: Include file resolution logic

---

## 5. Detection Strategy

### How to Identify the Problem

**Reproduction Steps:**
1. Create a shader include file:
   ```hlsl
   // shaders/common/lighting.hlsl
   float3 computePhongLighting(float3 normal, float3 lightDir) {
       return max(dot(normal, lightDir), 0.0f).xxx;
   }
   ```

2. Create a shader that includes it:
   ```hlsl
   // shaders/pixel_shader.hlsl
   #include "common/lighting.hlsl"
   float4 main(PixelInput input) : SV_TARGET {
       return float4(computePhongLighting(...), 1.0f);
   }
   ```

3. Attempt compilation:
   - Code: `shaderCompiler.compile(pixelShaderSource, "shaders/")`
   - Expected: Shader compiles and includes are resolved
   - Actual: Compilation fails - cannot find include file

4. Verify with current API:
   - Run: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Shader*"`
   - Look for: No tests with include directives

### Success Criteria
- Shader compilation succeeds with `#include` directives
- Include paths configured via `ShaderCompileDesc::includePaths`
- Multiple include directories searchable in order
- Include files relative and absolute path support

---

## 6. Fix Guidance

### Implementation Steps

#### Step 1: Define Include Path Configuration
```cpp
// In ShaderModule.h or Device.h - Add configuration structure
struct ShaderCompileDesc {
    std::string source;
    std::string entryPoint;
    std::string profile;
    std::vector<std::string> includePaths;  // NEW: configurable paths
    std::vector<D3D_SHADER_MACRO> defines;
};
```

#### Step 2: Implement Custom Include Handler
```cpp
// In src/igl/d3d12/ShaderIncludeHandler.h (NEW FILE)
class ShaderIncludeHandler : public ID3DInclude {
public:
    explicit ShaderIncludeHandler(const std::vector<std::string>& paths);

    HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE includeType,
                                   LPCSTR pFileName,
                                   LPCVOID pParentData,
                                   LPCVOID* ppData,
                                   UINT* pBytes) override;

    HRESULT STDMETHODCALLTYPE Close(LPCVOID pData) override;

private:
    std::vector<std::string> includePaths_;
    std::map<std::string, std::vector<char>> loadedFiles_;
};
```

#### Step 3: Implement Include Handler Methods
```cpp
// In ShaderIncludeHandler.cpp - Implement file resolution
HRESULT ShaderIncludeHandler::Open(D3D_INCLUDE_TYPE includeType,
                                    LPCSTR pFileName,
                                    LPCVOID pParentData,
                                    LPCVOID* ppData,
                                    UINT* pBytes) {
    // Search in include paths for file
    for (const auto& basePath : includePaths_) {
        std::string fullPath = basePath + "/" + pFileName;
        std::ifstream file(fullPath, std::ios::binary);
        if (file.good()) {
            // Load file content
            std::vector<char> content((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
            loadedFiles_[fullPath] = content;

            *ppData = loadedFiles_[fullPath].data();
            *pBytes = static_cast<UINT>(loadedFiles_[fullPath].size());
            return S_OK;
        }
    }
    return E_FAIL;  // File not found
}

HRESULT ShaderIncludeHandler::Close(LPCVOID pData) {
    // Memory managed by loadedFiles_ map
    return S_OK;
}
```

#### Step 4: Update Shader Compiler Integration
```cpp
// In ShaderCompiler.cpp - Use include handler
HRESULT ShaderCompiler::compile(const ShaderCompileDesc& desc) {
    ShaderIncludeHandler includeHandler(desc.includePaths);

    return D3DCompile(
        desc.source.data(),
        desc.source.size(),
        desc.entryPoint.c_str(),
        desc.defines.empty() ? nullptr : desc.defines.data(),
        &includeHandler,  // NEW: Custom include handler
        desc.entryPoint.c_str(),
        desc.profile.c_str(),
        D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0,
        &codeBlob_,
        &errorBlob_
    );
}
```

#### Step 5: Expose API in Device
```cpp
// In Device.h - Add shader compilation with include paths
class Device : public IDevice {
public:
    Result createShaderModule(const ShaderModuleDesc& desc,
                            const std::vector<std::string>& includePaths,
                            IShaderModule** outModule);
};
```

---

## 7. Testing Requirements

### Unit Tests to Create/Modify

**File**: `tests/d3d12/ShaderIncludeTests.cpp` (NEW)

1. **Basic Include Resolution Test**
   ```cpp
   TEST(D3D12ShaderInclude, ResolveIncludeFile) {
       ShaderIncludeHandler handler({"shaders/", "shaders/common/"});

       const char* pFileName = "math_lib.hlsl";
       const void* pData = nullptr;
       UINT bytes = 0;

       HRESULT hr = handler.Open(D3D_INCLUDE_LOCAL, pFileName, nullptr, &pData, &bytes);
       ASSERT_EQ(hr, S_OK);
       ASSERT_NE(pData, nullptr);
       ASSERT_GT(bytes, 0);
   }
   ```

2. **Multiple Include Paths Test**
   ```cpp
   TEST(D3D12ShaderInclude, SearchMultiplePaths) {
       std::vector<std::string> paths = {
           "nonexistent/",
           "shaders/",      // File found here
           "shaders/common/"
       };
       ShaderIncludeHandler handler(paths);

       const void* pData = nullptr;
       UINT bytes = 0;
       HRESULT hr = handler.Open(D3D_INCLUDE_LOCAL, "math_lib.hlsl", nullptr, &pData, &bytes);
       ASSERT_EQ(hr, S_OK);  // Should find in second path
   }
   ```

3. **Missing Include Error Test**
   ```cpp
   TEST(D3D12ShaderInclude, HandleMissingInclude) {
       ShaderIncludeHandler handler({"shaders/"});

       const void* pData = nullptr;
       UINT bytes = 0;
       HRESULT hr = handler.Open(D3D_INCLUDE_LOCAL, "missing.hlsl", nullptr, &pData, &bytes);
       ASSERT_EQ(hr, E_FAIL);
   }
   ```

4. **Shader Compilation with Include Test**
   ```cpp
   TEST(D3D12ShaderInclude, CompileWithInclude) {
       const std::string source = R"(
           #include "math_lib.hlsl"
           float4 main() : SV_TARGET {
               return float4(1.0, 0.0, 0.0, 1.0);
           }
       )";

       ShaderCompileDesc desc{
           source,
           "main",
           "ps_5_0",
           {"shaders/", "shaders/common/"}
       };

       ShaderCompiler compiler;
       ASSERT_TRUE(compiler.compile(desc));
   }
   ```

### Integration Tests

**Command**: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Shader*"`

- Run all shader tests to ensure no regression
- Verify: All existing shader compilation tests still pass

**Command**: `./test_all_sessions.bat`

- Ensure shader-dependent sessions still run correctly
- Verify: No crashes or shader compilation failures

### Restrictions
- ❌ DO NOT modify cross-platform shader interface in `include/igl/IShaderModule.h`
- ❌ DO NOT change shader stage enum values
- ❌ DO NOT add mandatory include path requirement to existing code
- ✅ DO focus changes on `src/igl/d3d12/ShaderCompiler.*` and new `ShaderIncludeHandler.*`

---

## 8. Definition of Done

### Completion Checklist

- [ ] ShaderIncludeHandler class implemented with ID3DInclude interface
- [ ] Include path search logic working for multiple directories
- [ ] ShaderCompileDesc extended with includePaths field
- [ ] ShaderCompiler uses custom include handler in D3DCompile
- [ ] Device API provides include path parameter option
- [ ] All new unit tests pass: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*ShaderInclude*"`
- [ ] Existing shader tests pass: `./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Shader*"`
- [ ] Integration tests pass: `./test_all_sessions.bat` (0 crashes)
- [ ] Sample shader library with includes created and documented
- [ ] Code reviewed and approved by maintainer

### User Confirmation Required
⚠️ **STOP** - Do NOT proceed to next task until user confirms:
- "✅ I have tested shader compilation with include directives"
- "✅ Multiple include paths resolve correctly"
- "✅ No regressions in existing shader functionality"

---

## 9. Related Issues

### Blocks
- **E-013**: Shader preprocessor macros (can extend include support)
- **E-015**: Shader library management (depends on include paths)

### Related
- **E-007**: Shader validation
- **E-009**: Shader compilation errors
- **C-001**: Code organization

---

## 10. Implementation Priority

### Severity: **Low** | Priority: **P2-Low**

**Effort**: 30-40 hours
- Design: 6 hours (include path strategy, file search)
- Implementation: 16-20 hours (handler + compiler integration)
- Testing: 6-8 hours (unit + integration tests)
- Documentation: 2-4 hours

**Risk**: **Low**
- Risk of file I/O errors if include handler has bugs
- Risk of path resolution issues on Windows (backslash/forward slash)
- Mitigation: Comprehensive unit testing with various path formats

**Impact**: **Medium**
- Enables modular shader development
- Improves code reusability
- Supports shader library distribution

**Blockers**: None

**Dependencies**:
- Requires existing shader compiler (`D3DCompile` available)
- No external library dependencies

---

## 11. References

### Official Microsoft Documentation
1. **D3DCompile Function**: https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3dcompile
2. **ID3DInclude Interface**: https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nn-d3dcompiler-id3dinclude
3. **HLSL Compiler Reference**: https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-reference
4. **Shader Validation**: https://learn.microsoft.com/en-us/windows/win32/direct3d12/shader-validation

### GitHub References
- **Microsoft D3D12 Samples**: https://github.com/microsoft/DirectX-Graphics-Samples
- **IGL Repository**: https://github.com/facebook/igl

### Related Issues
- Task E-009: Shader compilation error handling
- Task E-013: Shader preprocessor macros
