# Task 1.6: DXC Compiler Migration Guide

**Status:** Documented - Ready for Implementation
**Estimated Effort:** 1-2 weeks
**Impact:** +10-20% shader performance + SM 6.0+ features
**Priority:** HIGH (FXC is deprecated by Microsoft)

---

## Executive Summary

Migrate from FXC (legacy D3DCompile) to DXC (modern DxcCompile) compiler for:
- **10-20% shader performance improvement**
- **Shader Model 6.0+ support** (raytracing, wave intrinsics, mesh shaders)
- **Future-proof** (FXC deprecated, no new features)
- **Better optimization** (modern compiler backend)

---

## Current State Analysis

### FXC Usage Locations

**Primary Compilation (Device.cpp:1486)**
```cpp
HRESULT hr = D3DCompile(
    desc.input.source,              // HLSL source
    sourceLength,
    desc.debugName.c_str(),
    nullptr,                        // Defines
    nullptr,                        // Include handler
    desc.entryPoint.c_str(),
    target.c_str(),                 // "vs_5_0", "ps_5_0", "cs_5_0"
    compileFlags,
    0,
    bytecode.GetAddressOf(),
    errorBlob.GetAddressOf()
);
```

**Texture Operations (Texture.cpp:501, 700)**
- Mipmap generation shaders
- Texture copy shaders

---

## Migration Strategy

### Phase 1: Add DXC Infrastructure (2-3 days)

#### 1.1 Add DXC Headers and Library

**D3D12Headers.h:**
```cpp
// Add DXC includes
#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")
```

**Build System (CMakeLists.txt):**
```cmake
# Link DXC compiler library
target_link_libraries(IGLD3D12 PRIVATE dxcompiler.lib)
```

#### 1.2 Create DXC Compiler Wrapper

**New File: src/igl/d3d12/DXCCompiler.h**
```cpp
#pragma once

#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12Headers.h>
#include <vector>

namespace igl::d3d12 {

class DXCCompiler {
 public:
  DXCCompiler();
  ~DXCCompiler();

  // Initialize DXC compiler
  Result initialize();

  // Compile HLSL to DXIL (SM 6.0+)
  Result compile(
      const char* source,
      size_t sourceLength,
      const char* entryPoint,
      const char* target,         // "vs_6_0", "ps_6_0", "cs_6_0"
      const char* debugName,
      uint32_t flags,
      std::vector<uint8_t>& outBytecode,
      std::string& outErrors
  );

 private:
  Microsoft::WRL::ComPtr<IDxcUtils> utils_;
  Microsoft::WRL::ComPtr<IDxcCompiler3> compiler_;
  Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
};

} // namespace igl::d3d12
```

**Implementation: src/igl/d3d12/DXCCompiler.cpp**
```cpp
#include <igl/d3d12/DXCCompiler.h>

namespace igl::d3d12 {

DXCCompiler::DXCCompiler() = default;
DXCCompiler::~DXCCompiler() = default;

Result DXCCompiler::initialize() {
  // Create DXC utils
  HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils_));
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create DxcUtils");
  }

  // Create DXC compiler
  hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_));
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create DxcCompiler");
  }

  // Create default include handler
  hr = utils_->CreateDefaultIncludeHandler(&includeHandler_);
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create include handler");
  }

  return Result();
}

Result DXCCompiler::compile(
    const char* source,
    size_t sourceLength,
    const char* entryPoint,
    const char* target,
    const char* debugName,
    uint32_t flags,
    std::vector<uint8_t>& outBytecode,
    std::string& outErrors) {

  // Create source blob
  Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
  HRESULT hr = utils_->CreateBlob(source, sourceLength, CP_UTF8, &sourceBlob);
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create source blob");
  }

  // Build arguments
  std::vector<LPCWSTR> arguments;

  // Entry point
  std::wstring wEntryPoint = std::wstring(entryPoint, entryPoint + strlen(entryPoint));
  arguments.push_back(L"-E");
  arguments.push_back(wEntryPoint.c_str());

  // Target profile
  std::wstring wTarget = std::wstring(target, target + strlen(target));
  arguments.push_back(L"-T");
  arguments.push_back(wTarget.c_str());

  // Debug info
  if (flags & D3DCOMPILE_DEBUG) {
    arguments.push_back(L"-Zi");     // Debug info
    arguments.push_back(L"-Qembed_debug"); // Embed debug info
    arguments.push_back(L"-Od");     // Disable optimizations
  }

  // Optimization level
  if (!(flags & D3DCOMPILE_SKIP_OPTIMIZATION)) {
    arguments.push_back(L"-O3");     // Maximum optimization
  }

  // Warnings as errors
  if (flags & D3DCOMPILE_WARNINGS_ARE_ERRORS) {
    arguments.push_back(L"-WX");
  }

  // Compile
  DxcBuffer sourceBuffer = {};
  sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
  sourceBuffer.Size = sourceBlob->GetBufferSize();
  sourceBuffer.Encoding = CP_UTF8;

  Microsoft::WRL::ComPtr<IDxcResult> result;
  hr = compiler_->Compile(
      &sourceBuffer,
      arguments.data(),
      static_cast<UINT32>(arguments.size()),
      includeHandler_.Get(),
      IID_PPV_ARGS(&result)
  );

  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "DXC compilation failed");
  }

  // Check compilation status
  HRESULT compileStatus;
  result->GetStatus(&compileStatus);

  // Get errors/warnings
  Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
  result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
  if (errors && errors->GetStringLength() > 0) {
    outErrors = std::string(errors->GetStringPointer(), errors->GetStringLength());
  }

  if (FAILED(compileStatus)) {
    return Result(Result::Code::RuntimeError, "Shader compilation failed: " + outErrors);
  }

  // Get compiled bytecode
  Microsoft::WRL::ComPtr<IDxcBlob> bytecode;
  result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&bytecode), nullptr);

  if (!bytecode) {
    return Result(Result::Code::RuntimeError, "No bytecode produced");
  }

  // Copy bytecode to output
  const uint8_t* data = static_cast<const uint8_t*>(bytecode->GetBufferPointer());
  size_t size = bytecode->GetBufferSize();
  outBytecode.assign(data, data + size);

  return Result();
}

} // namespace igl::d3d12
```

### Phase 2: Update Device.cpp (3-4 days)

**Replace D3DCompile with DXC:**

**Device.cpp - createShaderModule():**
```cpp
std::unique_ptr<IShaderModule> Device::createShaderModule(
    const ShaderModuleDesc& desc,
    Result* IGL_NULLABLE outResult) const {

  // ... existing validation code ...

  // Initialize DXC compiler (once per device, cache it)
  static DXCCompiler dxcCompiler;
  static bool dxcInitialized = false;
  if (!dxcInitialized) {
    Result initResult = dxcCompiler.initialize();
    if (!initResult.isOk()) {
      IGL_LOG_ERROR("Failed to initialize DXC compiler: %s\n", initResult.message.c_str());
      Result::setResult(outResult, initResult.code, initResult.message);
      return nullptr;
    }
    dxcInitialized = true;
  }

  // Determine shader target (SM 6.0+)
  std::string target;
  switch (desc.info.stage) {
    case ShaderStage::Vertex:   target = "vs_6_0"; break;
    case ShaderStage::Fragment: target = "ps_6_0"; break;
    case ShaderStage::Compute:  target = "cs_6_0"; break;
    default:
      Result::setResult(outResult, Result::Code::Unsupported, "Unsupported shader stage");
      return nullptr;
  }

  // Set compile flags
  uint32_t compileFlags = 0;
#ifndef NDEBUG
  compileFlags |= D3DCOMPILE_DEBUG;
  compileFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#else
  // Release: enable optimizations
#endif

  if (treatWarningsAsErrors) {
    compileFlags |= D3DCOMPILE_WARNINGS_ARE_ERRORS;
  }

  // Compile with DXC
  std::vector<uint8_t> bytecode;
  std::string errors;
  Result compileResult = dxcCompiler.compile(
      desc.input.source,
      sourceLength,
      desc.entryPoint.c_str(),
      target.c_str(),
      desc.debugName.c_str(),
      compileFlags,
      bytecode,
      errors
  );

  if (!compileResult.isOk()) {
    IGL_LOG_ERROR("DXC compilation failed: %s\n", errors.c_str());
    Result::setResult(outResult, compileResult.code, errors);
    return nullptr;
  }

  if (!errors.empty()) {
    IGL_LOG_INFO("DXC compilation warnings:\n%s\n", errors.c_str());
  }

  IGL_LOG_INFO("  Shader compiled successfully (%zu bytes DXIL bytecode)\n", bytecode.size());

  Result::setOk(outResult);
  return std::make_unique<ShaderModule>(desc.info, std::move(bytecode));
}
```

### Phase 3: Update Texture.cpp (1-2 days)

**Replace D3DCompile calls in mipmap/copy shaders:**

```cpp
// Example for Texture.cpp:501
Microsoft::WRL::ComPtr<ID3DBlob> vs, ps, errs;

// OLD (FXC):
// D3DCompile(kVS, strlen(kVS), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, vs.GetAddressOf(), errs.GetAddressOf());

// NEW (DXC):
std::vector<uint8_t> vsBytecode, psBytecode;
std::string vsErrors, psErrors;

dxcCompiler.compile(kVS, strlen(kVS), "main", "vs_6_0", "MipmapVS", 0, vsBytecode, vsErrors);
dxcCompiler.compile(kPS, strlen(kPS), "main", "ps_6_0", "MipmapPS", 0, psBytecode, psErrors);

// Wrap in ID3DBlob for compatibility
D3DCreateBlob(vsBytecode.size(), vs.GetAddressOf());
memcpy(vs->GetBufferPointer(), vsBytecode.data(), vsBytecode.size());

D3DCreateBlob(psBytecode.size(), ps.GetAddressOf());
memcpy(ps->GetBufferPointer(), psBytecode.data(), psBytecode.size());
```

### Phase 4: Testing (2-3 days)

**Test Plan:**
1. Rebuild all sessions
2. Verify all 9 sessions still pass
3. Check shader compilation logs for "DXIL" instead of "DXBC"
4. Performance testing (measure 10-20% improvement)
5. Regression testing (no functionality changes)

---

## Benefits After Migration

### Immediate

1. **10-20% Shader Performance**
   - Better register allocation
   - Improved instruction scheduling
   - Modern optimization passes

2. **Shader Model 6.0 Support**
   - Wave intrinsics (cross-lane operations)
   - Improved math intrinsics
   - Better constant folding

### Future (Unlocks)

3. **Shader Model 6.1+**
   - Barycentric coordinates
   - ViewID for multi-view rendering

4. **Shader Model 6.4+**
   - Integer dot product
   - Wave intrinsics improvements

5. **Shader Model 6.5+**
   - DXR (DirectX Raytracing)
   - Mesh shaders
   - Sampler feedback

6. **Shader Model 6.6+**
   - Work graphs
   - Advanced raytracing features

---

## Risks and Mitigation

### Risk 1: DXC Not Available

**Mitigation:** Fallback to FXC if DXC fails to initialize
```cpp
if (!dxcCompiler.initialize().isOk()) {
  IGL_LOG_WARNING("DXC unavailable, falling back to FXC\n");
  useFallbackFXC = true;
}
```

### Risk 2: Shader Compatibility

**Mitigation:**
- SM 6.0 is backward compatible with SM 5.0
- Test all existing shaders with DXC
- Keep FXC path for one release cycle

### Risk 3: Build Dependencies

**Mitigation:**
- dxcompiler.dll ships with Windows 10 1803+
- Bundle DXC redistributable for older systems
- Document runtime requirements

---

## Implementation Checklist

### Prerequisites
- [ ] Install Windows SDK with DXC support (10.0.20348.0+)
- [ ] Verify dxcompiler.lib available
- [ ] Update CMakeLists.txt to link dxcompiler.lib

### Phase 1: Infrastructure (2-3 days)
- [ ] Create DXCCompiler.h
- [ ] Create DXCCompiler.cpp
- [ ] Add DXC headers to D3D12Headers.h
- [ ] Test DXC initialization

### Phase 2: Device.cpp (3-4 days)
- [ ] Replace D3DCompile with DXC in createShaderModule()
- [ ] Update shader targets (vs_6_0, ps_6_0, cs_6_0)
- [ ] Add fallback to FXC if DXC unavailable
- [ ] Test shader compilation

### Phase 3: Texture.cpp (1-2 days)
- [ ] Replace D3DCompile calls in mipmap generation
- [ ] Replace D3DCompile calls in texture copy
- [ ] Test texture operations

### Phase 4: Testing (2-3 days)
- [ ] Run full test suite (9 sessions)
- [ ] Verify DXIL bytecode generation
- [ ] Performance benchmarking
- [ ] Regression testing

### Phase 5: Documentation (1 day)
- [ ] Update build instructions
- [ ] Document DXC requirements
- [ ] Add migration notes

---

## Performance Expectations

### Shader Compilation
- **Time:** 10-30% slower (more optimization passes)
- **Quality:** 10-20% faster runtime execution

### Runtime Performance
- **GPU Bound:** +10-20% FPS improvement
- **CPU Bound:** Minimal change
- **Shader Heavy:** Up to +25% in complex shaders

### Example Improvements
```
Simple VS/PS (HelloWorld):     +5-10% FPS
Complex Textured Scene:        +15-20% FPS
Compute Shaders:               +20-25% throughput
```

---

## References

### Microsoft Documentation
- [DXC Compiler Overview](https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dxc)
- [Shader Model 6.0](https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/hlsl-shader-model-6-0-features-for-direct3d-12)
- [DXC API Reference](https://github.com/microsoft/DirectXShaderCompiler/blob/main/include/dxc/dxcapi.h)

### Code Examples
- DirectX samples: https://github.com/microsoft/DirectX-Graphics-Samples
- DXC integration examples: https://github.com/microsoft/DirectXShaderCompiler/wiki

---

## Timeline

**Total Estimated Time:** 9-13 days (1.5 - 2.5 weeks)

- **Week 1:** Infrastructure + Device.cpp migration
- **Week 2:** Texture.cpp + Testing + Documentation

**Fast Track (if urgent):** 1 week with focused effort

---

## Success Criteria

✅ All 9 sessions compile and run with DXC
✅ DXIL bytecode generated (SM 6.0)
✅ 10-20% shader performance improvement measured
✅ Zero regressions in functionality
✅ Fallback to FXC if DXC unavailable

---

**Status:** DOCUMENTED - Ready for implementation
**Priority:** HIGH (next sprint)
**Prepared By:** Claude Code Agent
**Date:** November 1, 2025
