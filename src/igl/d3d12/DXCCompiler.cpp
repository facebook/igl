/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/DXCCompiler.h>
#include <igl/d3d12/D3D12Headers.h>
#include <codecvt>
#include <locale>

namespace igl::d3d12 {

DXCCompiler::DXCCompiler() = default;
DXCCompiler::~DXCCompiler() = default;

Result DXCCompiler::initialize() {
  if (initialized_) {
    return Result();
  }

  IGL_D3D12_LOG_VERBOSE("DXCCompiler: Initializing DXC compiler...\n");

  // Create DXC utils
  HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(utils_.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("DXCCompiler: Failed to create DxcUtils: 0x%08X\n", static_cast<unsigned>(hr));
    return Result(Result::Code::RuntimeError, "Failed to create DxcUtils");
  }

  // Create DXC compiler
  hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(compiler_.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR("DXCCompiler: Failed to create DxcCompiler: 0x%08X\n", static_cast<unsigned>(hr));
    return Result(Result::Code::RuntimeError, "Failed to create DxcCompiler");
  }

  // Create default include handler
  hr = utils_->CreateDefaultIncludeHandler(includeHandler_.GetAddressOf());
  if (FAILED(hr)) {
    IGL_LOG_ERROR("DXCCompiler: Failed to create include handler: 0x%08X\n", static_cast<unsigned>(hr));
    return Result(Result::Code::RuntimeError, "Failed to create include handler");
  }

  // Create DXC validator for DXIL signing (optional but highly recommended)
  hr = DxcCreateInstance(CLSID_DxcValidator, IID_PPV_ARGS(validator_.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_D3D12_LOG_VERBOSE("DXCCompiler: Validator not available (0x%08X) - DXIL will be unsigned\n", static_cast<unsigned>(hr));
    // Not a fatal error - continue without validator
  } else {
    IGL_D3D12_LOG_VERBOSE("DXCCompiler: Validator initialized - DXIL signing available\n");
  }

  initialized_ = true;
  IGL_D3D12_LOG_VERBOSE("DXCCompiler: Initialization successful (Shader Model 6.0+ enabled)\n");

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

  if (!initialized_) {
    return Result(Result::Code::InvalidOperation, "DXC compiler not initialized");
  }

  IGL_D3D12_LOG_VERBOSE("DXCCompiler: Compiling shader '%s' with target '%s' (%zu bytes source)\n",
               debugName ? debugName : "unnamed",
               target,
               sourceLength);

  // Create source blob
  Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
  HRESULT hr = utils_->CreateBlob(source, static_cast<UINT32>(sourceLength), CP_UTF8, sourceBlob.GetAddressOf());
  if (FAILED(hr)) {
    IGL_LOG_ERROR("DXCCompiler: Failed to create source blob: 0x%08X\n", static_cast<unsigned>(hr));
    return Result(Result::Code::RuntimeError, "Failed to create source blob");
  }

  // Convert strings to wide char for DXC API
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  std::wstring wEntryPoint = converter.from_bytes(entryPoint);
  std::wstring wTarget = converter.from_bytes(target);

  // Build compilation arguments
  std::vector<LPCWSTR> arguments;

  // Entry point
  arguments.push_back(L"-E");
  arguments.push_back(wEntryPoint.c_str());

  // Target profile
  arguments.push_back(L"-T");
  arguments.push_back(wTarget.c_str());

  // Debug info and optimization
  if (flags & D3DCOMPILE_DEBUG) {
    IGL_D3D12_LOG_VERBOSE("  DXC: Debug mode enabled\n");
    arguments.push_back(L"-Zi");              // Debug info
    arguments.push_back(L"-Qembed_debug");    // Embed debug info in shader
    arguments.push_back(L"-Od");              // Disable optimizations
  } else {
    IGL_D3D12_LOG_VERBOSE("  DXC: Release mode - maximum optimization\n");
    arguments.push_back(L"-O3");              // Maximum optimization
  }

  // Skip optimization flag
  if (flags & D3DCOMPILE_SKIP_OPTIMIZATION) {
    arguments.push_back(L"-Od");
  }

  // Warnings as errors
  if (flags & D3DCOMPILE_WARNINGS_ARE_ERRORS) {
    IGL_D3D12_LOG_VERBOSE("  DXC: Treating warnings as errors\n");
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
      IID_PPV_ARGS(result.GetAddressOf())
  );

  if (FAILED(hr)) {
    IGL_LOG_ERROR("DXCCompiler: Compilation invocation failed: 0x%08X\n", static_cast<unsigned>(hr));
    return Result(Result::Code::RuntimeError, "DXC compilation invocation failed");
  }

  // Check compilation status
  HRESULT compileStatus;
  result->GetStatus(&compileStatus);

  // Get errors/warnings
  Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
  Microsoft::WRL::ComPtr<IDxcBlobWide> errorsName;
  result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.GetAddressOf()), errorsName.GetAddressOf());
  if (errors.Get() && errors->GetStringLength() > 0) {
    outErrors = std::string(errors->GetStringPointer(), errors->GetStringLength());
  }

  if (FAILED(compileStatus)) {
    IGL_LOG_ERROR("DXCCompiler: Shader compilation failed\n");
    if (!outErrors.empty()) {
      IGL_LOG_ERROR("%s\n", outErrors.c_str());
    }
    return Result(Result::Code::RuntimeError, "Shader compilation failed: " + outErrors);
  }

  // Log warnings if any
  if (!outErrors.empty()) {
    IGL_D3D12_LOG_VERBOSE("DXCCompiler: Compilation warnings:\n%s\n", outErrors.c_str());
  }

  // Get compiled bytecode (DXIL)
  Microsoft::WRL::ComPtr<IDxcBlob> bytecode;
  Microsoft::WRL::ComPtr<IDxcBlobWide> bytecodeName;
  result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(bytecode.GetAddressOf()), bytecodeName.GetAddressOf());

  if (!bytecode.Get()) {
    IGL_LOG_ERROR("DXCCompiler: No bytecode produced\n");
    return Result(Result::Code::RuntimeError, "No bytecode produced");
  }

  // Validate and sign DXIL if validator is available
  if (validator_.Get()) {
    IGL_D3D12_LOG_VERBOSE("DXCCompiler: Attempting DXIL validation and signing...\n");
    Microsoft::WRL::ComPtr<IDxcOperationResult> validationResult;
    hr = validator_->Validate(bytecode.Get(), DxcValidatorFlags_InPlaceEdit, validationResult.GetAddressOf());

    if (SUCCEEDED(hr)) {
      HRESULT validationStatus;
      validationResult->GetStatus(&validationStatus);
      IGL_D3D12_LOG_VERBOSE("DXCCompiler: Validation status: 0x%08X\n", static_cast<unsigned>(validationStatus));

      if (SUCCEEDED(validationStatus)) {
        // Get the validated (signed) bytecode - this replaces the original
        Microsoft::WRL::ComPtr<IDxcBlob> validatedBlob;
        validationResult->GetResult(validatedBlob.GetAddressOf());

        if (validatedBlob.Get()) {
          IGL_D3D12_LOG_VERBOSE("DXCCompiler: Got validated blob (%zu bytes)\n", validatedBlob->GetBufferSize());
          // Replace bytecode with validated version using move semantics
          bytecode.Reset();
          bytecode = std::move(validatedBlob);
          IGL_D3D12_LOG_VERBOSE("DXCCompiler: DXIL validated and signed successfully\n");
        } else {
          IGL_D3D12_LOG_VERBOSE("DXCCompiler: Validation succeeded but no blob returned\n");
        }
      } else {
        // Validation failed - get error messages
        Microsoft::WRL::ComPtr<IDxcBlobEncoding> validationErrors;
        validationResult->GetErrorBuffer(validationErrors.GetAddressOf());
        if (validationErrors.Get() && validationErrors->GetBufferSize() > 0) {
          std::string errMsg(static_cast<const char*>(validationErrors->GetBufferPointer()),
                           validationErrors->GetBufferSize());
          IGL_D3D12_LOG_VERBOSE("DXCCompiler: DXIL validation failed:\n%s\n", errMsg.c_str());
        }
        IGL_D3D12_LOG_VERBOSE("DXCCompiler: Using unsigned DXIL (may require experimental features)\n");
      }
    } else {
      IGL_D3D12_LOG_VERBOSE("DXCCompiler: DXIL validation skipped (validator error 0x%08X)\n", static_cast<unsigned>(hr));
    }
  } else {
    IGL_D3D12_LOG_VERBOSE("DXCCompiler: Using unsigned DXIL (validator not available)\n");
  }

  // Copy bytecode to output (either signed or unsigned)
  const uint8_t* data = static_cast<const uint8_t*>(bytecode->GetBufferPointer());
  size_t size = bytecode->GetBufferSize();
  outBytecode.assign(data, data + size);

  IGL_D3D12_LOG_VERBOSE("DXCCompiler: Compilation successful (%zu bytes DXIL bytecode)\n", size);

  return Result();
}

} // namespace igl::d3d12
