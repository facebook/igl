# E-010: Shader Compilation Error Messages Unclear

**Priority:** P1-Medium
**Category:** Enhancement - Developer Experience
**Status:** Open
**Effort:** Small (1-2 days)

---

## 1. Problem Statement

The D3D12 backend's shader compilation error messages are difficult to understand and lack context, making debugging shader issues frustrating and time-consuming. Raw HLSL compiler errors are passed through without additional context about which shader failed, what stage it was for, or what the source was.

### The Danger

**Developer productivity loss** and **increased debugging time:**
- Generic error like "syntax error at line 42" without knowing which of 50 shaders failed
- No indication of shader stage (vertex, pixel, compute) that failed
- No source file path shown when error occurs
- Line numbers meaningless when shaders are generated or concatenated
- Cryptic HLSL compiler messages without explanation of common issues
- No suggestions for fixing common mistakes

**Real-world scenario:** Developer sees "error X3000: syntax error: unexpected token 'register'" but doesn't know:
1. Which shader file/function caused it
2. What shader stage (VS/PS/CS) was being compiled
3. What the actual source looks like around that line
4. That the real issue might be a missing semicolon 5 lines earlier

This leads to 30+ minute debugging sessions for issues that should take 30 seconds to identify.

---

## 2. Root Cause Analysis

The current implementation blindly forwards D3DCompile error output:

**Current Error Reporting:**
```cpp
// In Device.cpp or ShaderModule.cpp
HRESULT hr = D3DCompile(
    source.c_str(),
    source.size(),
    nullptr,  // ← No source name provided!
    nullptr,
    nullptr,
    entryPoint.c_str(),
    target.c_str(),
    compileFlags,
    0,
    &shaderBlob,
    &errorBlob);

if (FAILED(hr)) {
  if (errorBlob) {
    // Just dump raw error - no context!
    const char* errorMsg = static_cast<const char*>(errorBlob->GetBufferPointer());
    IGL_LOG_ERROR("%s", errorMsg);  // ← Unhelpful!
  }
  return Result<ComPtr<ID3DBlob>>(Result::Code::RuntimeError, "Compilation failed");
}
```

**What's Missing:**

1. **No shader identification:**
   ```
   Current:  "error X3000: syntax error"
   Needed:   "MyShader.hlsl (Pixel Shader 'main'): error X3000: syntax error"
   ```

2. **No source context:**
   ```
   Current:  Shows line number but not the actual line
   Needed:   Shows surrounding source lines with error highlighted
   ```

3. **No common error explanations:**
   ```
   Current:  "error X3004: undeclared identifier 'g_texture'"
   Needed:   "error X3004: undeclared identifier 'g_texture'
             Hint: Did you forget to declare this texture? Add: Texture2D g_texture : register(t0);"
   ```

4. **No shader stage context:**
   ```
   Current:  Generic error message
   Needed:   "[Vertex Shader] error in 'vertexMain'..."
   ```

---

## 3. Official Documentation References

- **D3DCompile Error Codes:**
  https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-errors

- **HLSL Compiler Messages:**
  https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-appendix-pre-pragma

- **ID3DBlob Interface (Error Messages):**
  https://learn.microsoft.com/en-us/windows/win32/api/d3dcommon/nn-d3dcommon-id3dblob

- **Shader Debugging Best Practices:**
  https://learn.microsoft.com/en-us/windows/win32/direct3d12/shader-debugging

---

## 4. Code Location Strategy

**Primary Files to Modify:**

1. **Shader compilation error handling:**
   ```
   Search for: "D3DCompile(" and check for errorBlob handling
   In files: src/igl/d3d12/ShaderModule.cpp, src/igl/d3d12/Device.cpp
   Pattern: Look for FAILED(hr) checks after D3DCompile
   ```

2. **Shader module creation:**
   ```
   Search for: "createShaderModule" or "ShaderModuleDesc"
   In files: src/igl/d3d12/Device.cpp
   Pattern: Look for shader creation entry points where we have shader metadata
   ```

3. **Error logging:**
   ```
   Search for: "IGL_LOG_ERROR" in shader compilation context
   In files: src/igl/d3d12/ShaderModule.cpp
   Pattern: Look for existing error reporting that needs enhancement
   ```

**New Files to Create:**

4. **Shader error formatter (optional helper):**
   ```
   Create: src/igl/d3d12/ShaderErrorFormatter.h
   Create: src/igl/d3d12/ShaderErrorFormatter.cpp
   Purpose: Format compilation errors with context and hints
   ```

---

## 5. Detection Strategy

### Reproduction Steps:

1. **Create shader with intentional syntax error:**
   ```hlsl
   // test_error_shader.hlsl
   Texture2D myTexture : register(t0)  // ← Missing semicolon

   float4 main(float2 uv : TEXCOORD) : SV_Target {
       return myTexture.Sample(sampler0, uv);
   }
   ```

2. **Compile and observe current error:**
   ```
   Current output:
   "error X3000: syntax error: unexpected token 'float4'"
   (No indication this is test_error_shader.hlsl pixel shader)
   ```

3. **Expected improved error:**
   ```
   Improved output:
   ========================================
   Shader Compilation Error
   ========================================
   Shader: test_error_shader.hlsl
   Stage:  Pixel Shader
   Entry:  main
   Target: ps_5_0

   test_error_shader.hlsl(1,42): error X3000: syntax error: unexpected token 'float4'

   Source context:
   >   1 | Texture2D myTexture : register(t0)  // ← Missing semicolon
       2 |
       3 | float4 main(float2 uv : TEXCOORD) : SV_Target {

   Hint: Check for missing semicolon on line 1
   ========================================
   ```

### Test Cases:

```cpp
TEST(ShaderErrors, UndeclaredIdentifier)
TEST(ShaderErrors, SyntaxError)
TEST(ShaderErrors, TypeMismatch)
TEST(ShaderErrors, InvalidRegisterIndex)
```

---

## 6. Fix Guidance

### Step 1: Enhance Shader Compilation Context

```cpp
// In ShaderModule.h or Device.h
struct ShaderCompilationContext {
  std::string sourceName;        // "MyShader.hlsl" or "Generated_VS_123"
  std::string entryPoint;        // "main", "vertexMain", etc.
  std::string shaderStage;       // "Vertex Shader", "Pixel Shader", "Compute Shader"
  std::string target;            // "vs_5_0", "ps_5_0", etc.
  std::string fullSource;        // Complete source code for context
  uint32_t compileFlags;
};
```

### Step 2: Create Error Formatter

```cpp
// In ShaderErrorFormatter.h
class ShaderErrorFormatter {
 public:
  static std::string formatError(
      const ShaderCompilationContext& context,
      const std::string& rawError);

 private:
  static std::string extractSourceContext(
      const std::string& source,
      int errorLine,
      int contextLines = 3);

  static std::string addHintForCommonError(const std::string& error);
  static std::string getShaderStageDisplayName(const std::string& target);
};

// In ShaderErrorFormatter.cpp
#include <sstream>
#include <regex>

std::string ShaderErrorFormatter::formatError(
    const ShaderCompilationContext& context,
    const std::string& rawError) {

  std::ostringstream formatted;

  // Header
  formatted << "\n========================================\n";
  formatted << "Shader Compilation Error\n";
  formatted << "========================================\n";

  // Shader identification
  formatted << "Shader: " << context.sourceName << "\n";
  formatted << "Stage:  " << getShaderStageDisplayName(context.target) << "\n";
  formatted << "Entry:  " << context.entryPoint << "\n";
  formatted << "Target: " << context.target << "\n\n";

  // Raw error message
  formatted << rawError << "\n";

  // Try to extract line number from error
  std::regex linePattern(R"((\d+),(\d+))");
  std::smatch match;
  if (std::regex_search(rawError, match, linePattern)) {
    int lineNumber = std::stoi(match[1].str());

    // Add source context
    formatted << "\nSource context:\n";
    formatted << extractSourceContext(context.fullSource, lineNumber, 3);
    formatted << "\n";
  }

  // Add hints for common errors
  std::string hint = addHintForCommonError(rawError);
  if (!hint.empty()) {
    formatted << "Hint: " << hint << "\n";
  }

  formatted << "========================================\n";

  return formatted.str();
}

std::string ShaderErrorFormatter::extractSourceContext(
    const std::string& source,
    int errorLine,
    int contextLines) {

  std::istringstream sourceStream(source);
  std::ostringstream context;

  int currentLine = 1;
  std::string line;

  int startLine = std::max(1, errorLine - contextLines);
  int endLine = errorLine + contextLines;

  while (std::getline(sourceStream, line)) {
    if (currentLine >= startLine && currentLine <= endLine) {
      // Mark error line with '>'
      if (currentLine == errorLine) {
        context << " > ";
      } else {
        context << "   ";
      }

      // Line number and source
      context << std::setw(4) << currentLine << " | " << line << "\n";
    }

    currentLine++;
    if (currentLine > endLine) {
      break;
    }
  }

  return context.str();
}

std::string ShaderErrorFormatter::addHintForCommonError(const std::string& error) {
  // Check for common error patterns and provide hints

  if (error.find("undeclared identifier") != std::string::npos) {
    return "Check that all variables and resources are declared before use. "
           "For textures, add: Texture2D name : register(t#);";
  }

  if (error.find("syntax error") != std::string::npos &&
      error.find("unexpected token") != std::string::npos) {
    return "Check for missing semicolons, parentheses, or braces on previous lines.";
  }

  if (error.find("type mismatch") != std::string::npos) {
    return "Ensure variable types match. Use explicit casts if needed: (float3)value";
  }

  if (error.find("register") != std::string::npos) {
    return "Check that register indices match your root signature descriptor tables. "
           "Valid ranges: b0-b13 (CBV), t0-t127 (SRV), u0-u63 (UAV), s0-s15 (Sampler)";
  }

  if (error.find("undefined") != std::string::npos) {
    return "Ensure all functions are declared before use. Check spelling and case sensitivity.";
  }

  if (error.find("semantic") != std::string::npos) {
    return "All shader inputs/outputs need semantics. Add: SV_Position, TEXCOORD0, COLOR0, etc.";
  }

  return "";  // No specific hint
}

std::string ShaderErrorFormatter::getShaderStageDisplayName(const std::string& target) {
  if (target.find("vs_") == 0) return "Vertex Shader";
  if (target.find("ps_") == 0) return "Pixel Shader";
  if (target.find("gs_") == 0) return "Geometry Shader";
  if (target.find("hs_") == 0) return "Hull Shader";
  if (target.find("ds_") == 0) return "Domain Shader";
  if (target.find("cs_") == 0) return "Compute Shader";
  return "Unknown Shader Stage";
}
```

### Step 3: Integrate into Compilation

```cpp
// In Device.cpp or ShaderModule.cpp
Result<ComPtr<ID3DBlob>> Device::compileShader(
    const ShaderCompilationContext& context) {

  ComPtr<ID3DBlob> shaderBlob;
  ComPtr<ID3DBlob> errorBlob;

  // Provide source name to D3DCompile for better error messages
  const char* sourceName = context.sourceName.empty() ? nullptr : context.sourceName.c_str();

  HRESULT hr = D3DCompile(
      context.fullSource.c_str(),
      context.fullSource.size(),
      sourceName,  // ← Now provides source name!
      nullptr,
      nullptr,
      context.entryPoint.c_str(),
      context.target.c_str(),
      context.compileFlags,
      0,
      &shaderBlob,
      &errorBlob);

  if (FAILED(hr)) {
    std::string rawError;
    if (errorBlob) {
      rawError = std::string(
          static_cast<const char*>(errorBlob->GetBufferPointer()),
          errorBlob->GetBufferSize());
    } else {
      rawError = "Unknown compilation error (no error blob)";
    }

    // Format error with context
    std::string formattedError = ShaderErrorFormatter::formatError(context, rawError);

    // Log to console
    IGL_LOG_ERROR("%s", formattedError.c_str());

    // Return as Result error message
    return Result<ComPtr<ID3DBlob>>(Result::Code::RuntimeError, formattedError);
  }

  return shaderBlob;
}
```

### Step 4: Update Public API Usage

```cpp
// In Device.cpp - createShaderModule
Result<std::shared_ptr<IShaderModule>> Device::createShaderModule(
    const ShaderModuleDesc& desc,
    Result* outResult) {

  // Build compilation context with all available metadata
  ShaderCompilationContext context;
  context.sourceName = desc.debugName.empty() ? "UnnamedShader" : desc.debugName;
  context.entryPoint = desc.entryPoint;
  context.fullSource = desc.source;
  context.compileFlags = getCompileFlags(desc.stage);

  // Determine shader stage and target
  switch (desc.stage) {
    case ShaderStage::Vertex:
      context.shaderStage = "Vertex Shader";
      context.target = "vs_5_0";
      break;
    case ShaderStage::Fragment:
      context.shaderStage = "Pixel Shader";
      context.target = "ps_5_0";
      break;
    case ShaderStage::Compute:
      context.shaderStage = "Compute Shader";
      context.target = "cs_5_0";
      break;
    default:
      context.shaderStage = "Unknown";
      context.target = "unknown";
  }

  // Compile with enhanced error reporting
  auto compileResult = compileShader(context);

  if (!compileResult.isOk()) {
    // Error already formatted and logged
    return Result<std::shared_ptr<IShaderModule>>(
        compileResult.code(),
        compileResult.message());
  }

  // ... continue with shader module creation ...
}
```

### Step 5: Add Warning for Missing Debug Names

```cpp
// Encourage developers to provide debug names
if (desc.debugName.empty()) {
  IGL_LOG_WARNING("Shader created without debug name. "
                  "Consider setting ShaderModuleDesc::debugName for better error messages.");
}
```

---

## 7. Testing Requirements

### Unit Tests:

```bash
# Run shader error formatting tests
./build/Debug/IGLTests.exe --gtest_filter="*ShaderError*"

# Run all D3D12 shader compilation tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*Shader*"
```

### Test Cases:

```cpp
TEST(ShaderErrorFormatter, FormatsSyntaxError) {
  ShaderCompilationContext context;
  context.sourceName = "test.hlsl";
  context.shaderStage = "Pixel Shader";
  context.entryPoint = "main";
  context.target = "ps_5_0";
  context.fullSource = "Texture2D tex\nfloat4 main() { return float4(0); }";

  std::string rawError = "test.hlsl(1,15): error X3000: syntax error: unexpected token 'float4'";
  std::string formatted = ShaderErrorFormatter::formatError(context, rawError);

  EXPECT_NE(formatted.find("test.hlsl"), std::string::npos);
  EXPECT_NE(formatted.find("Pixel Shader"), std::string::npos);
  EXPECT_NE(formatted.find("Source context:"), std::string::npos);
}

TEST(ShaderErrorFormatter, AddsHintForUndeclaredIdentifier) {
  std::string error = "error X3004: undeclared identifier 'myTexture'";
  std::string hint = ShaderErrorFormatter::addHintForCommonError(error);
  EXPECT_NE(hint.find("declared before use"), std::string::npos);
}

TEST(ShaderCompilation, ReportsErrorWithContext) {
  // Intentionally broken shader
  ShaderCompilationContext context;
  context.sourceName = "broken.hlsl";
  context.fullSource = "float4 main() { return undefinedVar; }";
  context.entryPoint = "main";
  context.target = "ps_5_0";

  auto result = device->compileShader(context);
  EXPECT_FALSE(result.isOk());
  EXPECT_NE(result.message().find("broken.hlsl"), std::string::npos);
  EXPECT_NE(result.message().find("Source context:"), std::string::npos);
}
```

### Integration Tests:

```bash
# Test error messages in real scenarios
./build/Debug/Session/TinyMeshSession.exe
# Temporarily break shader, verify error is clear

# Test all sessions still work
./test_all_sessions.bat
```

### Expected Results:

1. **Clear shader identification** in all error messages
2. **Source context shown** for errors with line numbers
3. **Helpful hints** for common errors (at least 5 patterns)
4. **No regression** in successful shader compilation

### Modification Restrictions:

- **DO NOT modify** D3DCompile behavior or output
- **DO NOT change** cross-platform error handling in `src/igl/`
- **DO NOT break** existing error codes or Result semantics
- **ONLY enhance** error message formatting and context

---

## 8. Definition of Done

### Validation Checklist:

- [ ] All shader compilation errors include shader name and stage
- [ ] Source context shown for errors with line numbers (at least 3 lines before/after)
- [ ] At least 5 common error patterns have helpful hints
- [ ] Shader stage derived from target profile (vs_5_0 → "Vertex Shader")
- [ ] All unit tests pass
- [ ] All integration tests pass
- [ ] Error messages tested with intentionally broken shaders
- [ ] Debug names encouraged (warning if missing)

### User Experience Validation:

- [ ] Error messages are easy to understand for common issues
- [ ] Source context helps locate the problem quickly
- [ ] Hints reduce debugging time for typical mistakes

### User Confirmation Required:

**STOP - Do NOT proceed to next task until user confirms:**
- [ ] Error messages are significantly clearer than before
- [ ] Debugging time reduced (informal test: break shader, measure time to fix)
- [ ] No false positives or confusing output
- [ ] All render sessions still work correctly

---

## 9. Related Issues

### Blocks:
- None - Standalone enhancement

### Related:
- **E-007** - Shader reflection incomplete (validation errors need good formatting)
- **E-009** - Shader compilation cache (cache errors need good messages)
- **E-011** - Shader debug names (debug names improve error messages)

### Depends On:
- None - Can be implemented independently

---

## 10. Implementation Priority

**Priority:** P1-Medium

**Effort Estimate:** 1-2 days
- Day 1: Implement ShaderErrorFormatter and integration
- Day 2: Testing, add more hints, polish output

**Risk Assessment:** Very Low
- Changes are purely additive (formatting only)
- No impact on shader compilation logic
- Easy to rollback if issues arise
- Can be tested exhaustively with broken shaders

**Impact:** Medium-High
- **Significant improvement in developer productivity**
- **Reduces debugging time** from 30+ minutes to 30 seconds for common errors
- **Better developer experience** especially for shader novices
- **Encourages use of debug names** (indirect benefit)
- No runtime performance impact

---

## 11. References

### Official Documentation:

1. **HLSL Error Codes:**
   https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-errors

2. **D3DCompile Function:**
   https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3dcompile

3. **Shader Debugging:**
   https://learn.microsoft.com/en-us/windows/win32/direct3d12/shader-debugging

4. **HLSL Compiler Messages:**
   https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-appendix-pre-pragma

5. **ID3DBlob (Error Output):**
   https://learn.microsoft.com/en-us/windows/win32/api/d3dcommon/nn-d3dcommon-id3dblob

### Additional Resources:

6. **HLSL Semantics Reference:**
   https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics

7. **Resource Binding in HLSL:**
   https://learn.microsoft.com/en-us/windows/win32/direct3d12/resource-binding-in-hlsl

---

**Last Updated:** 2025-11-12
**Assignee:** Unassigned
**Estimated Completion:** 1-2 days after start
