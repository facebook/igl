# D3D12 Shader Debugging Guide

This document describes the shader debugging features implemented in the IGL D3D12 backend.

## Table of Contents
1. [Overview](#overview)
2. [Debug Features](#debug-features)
3. [Environment Variables](#environment-variables)
4. [PIX Integration](#pix-integration)
5. [Shader Compilation Flags](#shader-compilation-flags)
6. [Error Messages](#error-messages)
7. [Performance Impact](#performance-impact)
8. [Best Practices](#best-practices)

## Overview

The D3D12 backend now includes comprehensive shader debugging capabilities to make shader development and debugging easier. These features include:

- Automatic debug information generation
- Shader disassembly output
- Shader reflection and binding validation
- Enhanced error messages
- PIX debugger integration
- D3D12 object naming

## Debug Features

### 1. Shader Debug Information (D3DCOMPILE_DEBUG)

**Status**: ENABLED BY DEFAULT

Shaders are compiled with debug information embedded in the bytecode, enabling source-level debugging in PIX and Visual Studio Graphics Debugger.

**Build Configuration**:
- **Debug builds**: `D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION`
  - Full debug info + unoptimized code for best debugging experience
- **Release builds**: `D3DCOMPILE_DEBUG`
  - Debug info included for PIX captures, but optimizations are still enabled
  - Can be disabled with `IGL_D3D12_DISABLE_SHADER_DEBUG=1`

**Benefits**:
- Source-level shader debugging in PIX
- Variable inspection at runtime
- Breakpoints and single-stepping through shader code
- Call stack visualization

### 2. Shader Disassembly

**Status**: OPTIONAL (Environment variable controlled)

Outputs DXIL/DXBC assembly to logs for performance analysis and understanding shader compilation.

**Enable**: Set environment variable `IGL_D3D12_DISASSEMBLE_SHADERS=1`

**Output Format**:
```
=== SHADER DISASSEMBLY (VERTEX - main) ===
; Input signature:
; Output signature:
; shader debug name: MyShader_vs
; shader hash: 0x1234567890abcdef
...
[Assembly instructions]
...
=========================
```

**Use Cases**:
- Understanding shader performance characteristics
- Verifying compiler optimizations
- Analyzing instruction count and register usage
- Debugging complex shader logic

### 3. Shader Reflection

**Status**: OPTIONAL (Environment variable controlled)

Uses ID3D12ShaderReflection to analyze compiled shaders and validate resource bindings.

**Enable**: Set environment variable `IGL_D3D12_VALIDATE_SHADER_BINDINGS=1`

**Output Format**:
```
=== SHADER REFLECTION (FRAGMENT/PIXEL - main) ===
  Bound Resources: 4
    [0] ConstantBuffer 'UniformsPerFrame' at b0 (space 0)
    [1] ConstantBuffer 'UniformsPerObject' at b1 (space 0)
    [2] Texture 'albedoTexture' at t0 (space 0)
    [3] Sampler 'albedoSampler' at s0 (space 0)
  Constant Buffers: 2
    [0] UniformsPerFrame: 256 bytes, 4 variables
    [1] UniformsPerObject: 128 bytes, 2 variables
================================
```

**Validates**:
- Constant buffer bindings (b# registers)
- Texture bindings (t# registers)
- Sampler bindings (s# registers)
- UAV bindings (u# registers)
- Register space usage
- Constant buffer sizes and layouts

**Use Cases**:
- Verifying shader resource bindings match pipeline expectations
- Debugging binding mismatches
- Understanding shader resource requirements
- Validating constant buffer layouts

### 4. Enhanced Error Messages

**Status**: ALWAYS ENABLED

Provides detailed, contextualized error messages when shader compilation fails.

**Error Message Components**:
1. **Shader Context**:
   - Shader stage (VERTEX, FRAGMENT/PIXEL, COMPUTE)
   - Entry point name
   - Target shader model (vs_5_0, ps_5_0, cs_5_0)
   - Debug name

2. **Compiler Output**:
   - Full error message from D3DCompile
   - Error codes and descriptions
   - Line numbers and column positions

3. **Helpful Hints**:
   - Extracted line number from error
   - Suggestions for common errors

**Example Error**:
```
Shader compilation FAILED
  Stage: VERTEX
  Entry Point: VSMain
  Target: vs_5_0
  Debug Name: MyVertexShader

=== COMPILER ERRORS ===
MyVertexShader(42,15): error X3004: undeclared identifier 'mvpMatrix'
======================

HINT: Check source line 42,15 in the shader
```

### 5. Warnings as Errors

**Status**: OPTIONAL (Environment variable controlled)

Treats shader warnings as compilation errors for stricter validation.

**Enable**: Set environment variable `IGL_D3D12_SHADER_WARNINGS_AS_ERRORS=1`

**Benefits**:
- Catches potential issues early
- Enforces code quality standards
- Prevents subtle bugs from warnings

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `IGL_D3D12_DISABLE_SHADER_DEBUG` | `0` | Disable debug info in release builds (set to `1` to disable) |
| `IGL_D3D12_DISASSEMBLE_SHADERS` | `0` | Output shader disassembly to logs (set to `1` to enable) |
| `IGL_D3D12_VALIDATE_SHADER_BINDINGS` | `0` | Output shader reflection info (set to `1` to enable) |
| `IGL_D3D12_SHADER_WARNINGS_AS_ERRORS` | `0` | Treat warnings as errors (set to `1` to enable) |

**Setting Environment Variables** (Windows):

Command Prompt:
```batch
set IGL_D3D12_DISASSEMBLE_SHADERS=1
set IGL_D3D12_VALIDATE_SHADER_BINDINGS=1
```

PowerShell:
```powershell
$env:IGL_D3D12_DISASSEMBLE_SHADERS="1"
$env:IGL_D3D12_VALIDATE_SHADER_BINDINGS="1"
```

System Environment Variables:
1. Right-click "This PC" → Properties
2. Advanced System Settings → Environment Variables
3. Add new User or System variable

## PIX Integration

### D3D12 Object Naming

All pipeline states and root signatures are automatically named for PIX captures:

**Render Pipelines**:
- Pipeline State Object: `PSO_<debugName>`
- Root Signature: `RootSig_<debugName>`

**Compute Pipelines**:
- Pipeline State Object: `ComputePSO_<debugName>`
- Root Signature: `ComputeRootSig_<debugName>`

**Benefits**:
- Easy identification in PIX captures
- Simplified performance profiling
- Better debugging workflow

### Using PIX with IGL Shaders

1. **Capture a frame** in PIX on Windows
2. **Navigate to Pipeline view** to see named pipeline states
3. **Click on shader stages** to view source code (thanks to D3DCOMPILE_DEBUG)
4. **Set breakpoints** in shader code
5. **Step through shader execution** with full variable inspection

### Shader Source Debugging in PIX

With `D3DCOMPILE_DEBUG` enabled (default), PIX can:
- Display original HLSL source code
- Show variable values at each line
- Support conditional breakpoints
- Display register allocation
- Show optimization decisions (in release builds)

## Shader Compilation Flags

### Debug Builds (_DEBUG defined)

```cpp
D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION
```

**Flags**:
- `D3DCOMPILE_ENABLE_STRICTNESS`: Force strict compliance with shader language
- `D3DCOMPILE_DEBUG`: Embed debug information in shader container
- `D3DCOMPILE_SKIP_OPTIMIZATION`: Disable optimizations for better debugging

**Characteristics**:
- Larger shader bytecode (includes debug info + source)
- Slower shader execution (no optimizations)
- Best debugging experience
- Suitable for development only

### Release Builds (NDEBUG defined)

```cpp
D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG
```

**Flags**:
- `D3DCOMPILE_ENABLE_STRICTNESS`: Force strict compliance
- `D3DCOMPILE_DEBUG`: Embed debug information (can be disabled)

**Characteristics**:
- Larger bytecode (debug info only, no optimizations disabled)
- Full shader optimizations enabled
- Still debuggable in PIX
- Suitable for profiling and testing

### Shipping Builds (IGL_D3D12_DISABLE_SHADER_DEBUG=1)

```cpp
D3DCOMPILE_ENABLE_STRICTNESS
```

**Characteristics**:
- Minimal bytecode size
- Full optimizations
- No debug information
- Not debuggable in PIX
- Best performance

## Error Messages

### Compilation Errors

**Before** (basic error):
```
Shader compilation failed: error X3004: undeclared identifier 'mvpMatrix'
```

**After** (enhanced error):
```
Shader compilation FAILED
  Stage: VERTEX
  Entry Point: VSMain
  Target: vs_5_0
  Debug Name: TransformShader

=== COMPILER ERRORS ===
TransformShader(42,15): error X3004: undeclared identifier 'mvpMatrix'
======================

HINT: Check source line 42,15 in the shader
```

### Common Errors and Solutions

#### 1. Undeclared Identifier
```
error X3004: undeclared identifier 'variableName'
```
**Solution**: Verify variable is declared, check spelling, ensure constant buffer is bound

#### 2. Type Mismatch
```
error X3017: cannot convert from 'float3' to 'float4'
```
**Solution**: Add explicit cast or adjust variable types

#### 3. Missing Semantic
```
error X3501: 'main': entrypoint not found
```
**Solution**: Verify entry point name matches shader code

#### 4. Register Binding Conflict
```
error X4509: register 'b0' already bound
```
**Solution**: Use shader reflection to identify binding conflicts

## Performance Impact

### Debug Information

**Impact**: ~10-30% larger shader bytecode
- Includes source code and symbol information
- Minimal runtime performance impact (metadata only)
- No effect on shader execution speed

**Recommendation**: Enable in debug and release builds, disable only for shipping

### Skip Optimization

**Impact**: 2-10x slower shader execution
- All optimizations disabled
- Significantly larger instruction count
- Better debugging experience

**Recommendation**: Enable only in debug builds

### Shader Disassembly

**Impact**: Minimal (compilation time only)
- Adds ~10-50ms per shader compilation
- No runtime impact
- Output to logs only

**Recommendation**: Enable during shader development, disable for normal builds

### Shader Reflection

**Impact**: Minimal (compilation time only)
- Adds ~5-20ms per shader compilation
- No runtime impact
- Validation during compilation only

**Recommendation**: Enable during development to catch binding issues early

## Best Practices

### Development Workflow

1. **Start with all debug features enabled**:
   ```batch
   set IGL_D3D12_DISASSEMBLE_SHADERS=1
   set IGL_D3D12_VALIDATE_SHADER_BINDINGS=1
   ```

2. **Use PIX for visual debugging**:
   - Capture frames regularly
   - Verify resource bindings
   - Profile shader performance

3. **Enable warnings as errors for new shaders**:
   ```batch
   set IGL_D3D12_SHADER_WARNINGS_AS_ERRORS=1
   ```

4. **Review shader reflection output**:
   - Verify constant buffer sizes
   - Check register bindings
   - Validate resource usage

### Debugging Shader Issues

1. **Compilation Errors**:
   - Read enhanced error message carefully
   - Check line number in source
   - Verify constant buffer layouts match CPU structures

2. **Binding Mismatches**:
   - Enable shader reflection (`IGL_D3D12_VALIDATE_SHADER_BINDINGS=1`)
   - Compare shader reflection output with pipeline setup
   - Verify register assignments (b#, t#, s#, u#)

3. **Performance Issues**:
   - Enable disassembly (`IGL_D3D12_DISASSEMBLE_SHADERS=1`)
   - Count instructions and identify hotspots
   - Use PIX GPU profiling
   - Review optimization opportunities

4. **Visual Artifacts**:
   - Capture frame in PIX
   - Set breakpoints in pixel shader
   - Inspect variable values
   - Verify blend state and render targets

### Testing and Validation

1. **Always test with validation enabled**:
   - Shader reflection catches binding errors
   - D3D12 debug layer provides additional validation
   - Enhanced error messages help identify issues quickly

2. **Profile with release builds + debug info**:
   - Representative of final performance
   - Still debuggable in PIX
   - Identifies optimization opportunities

3. **Verify shipping builds**:
   - Test with `IGL_D3D12_DISABLE_SHADER_DEBUG=1`
   - Ensure no dependencies on debug features
   - Measure final bytecode sizes

## Recommendations for Future Enhancements

1. **PDB File Generation**:
   - Use D3DCompile2 or DXC with `/Fd` flag
   - Generate separate PDB files for shader debugging
   - Support PIX automatic PDB resolution

2. **Shader Cache with Debug Info**:
   - Cache compiled shaders with embedded debug info
   - Faster iteration during development
   - Preserve debug symbols across runs

3. **Advanced Shader Reflection**:
   - Validate constant buffer member alignment
   - Check for unused resources
   - Detect suboptimal binding patterns

4. **Integration with CI/CD**:
   - Compile all shaders with warnings as errors
   - Generate shader reflection reports
   - Track shader bytecode sizes over time

5. **Shader Variant Management**:
   - Debug vs release shader variants
   - Preprocessor define management
   - Automatic variant selection based on build type

6. **Enhanced PIX Integration**:
   - Automatic marker insertion
   - Shader timing markers
   - Resource usage tracking

## Summary

The IGL D3D12 backend now provides comprehensive shader debugging capabilities:

- **Debug information** embedded in all shaders by default
- **Enhanced error messages** with full context and hints
- **Shader disassembly** for performance analysis
- **Shader reflection** for binding validation
- **PIX integration** via D3D12 object naming
- **Flexible control** via environment variables

These features significantly improve the shader development and debugging experience while maintaining excellent performance in release builds.
