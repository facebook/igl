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

  IGL_LOG_INFO("DXCCompiler: Initializing DXC compiler...\n");

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

  initialized_ = true;
  IGL_LOG_INFO("DXCCompiler: Initialization successful (Shader Model 6.0+ enabled)\n");

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

  IGL_LOG_INFO("DXCCompiler: Compiling shader '%s' with target '%s' (%zu bytes source)\n",
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
    IGL_LOG_INFO("  DXC: Debug mode enabled\n");
    arguments.push_back(L"-Zi");              // Debug info
    arguments.push_back(L"-Qembed_debug");    // Embed debug info in shader
    arguments.push_back(L"-Od");              // Disable optimizations
  } else {
    IGL_LOG_INFO("  DXC: Release mode - maximum optimization\n");
    arguments.push_back(L"-O3");              // Maximum optimization
  }

  // Skip optimization flag
  if (flags & D3DCOMPILE_SKIP_OPTIMIZATION) {
    arguments.push_back(L"-Od");
  }

  // Warnings as errors
  if (flags & D3DCOMPILE_WARNINGS_ARE_ERRORS) {
    IGL_LOG_INFO("  DXC: Treating warnings as errors\n");
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
    IGL_LOG_INFO("DXCCompiler: Compilation warnings:\n%s\n", outErrors.c_str());
  }

  // Get compiled bytecode (DXIL)
  Microsoft::WRL::ComPtr<IDxcBlob> bytecode;
  Microsoft::WRL::ComPtr<IDxcBlobWide> bytecodeName;
  result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(bytecode.GetAddressOf()), bytecodeName.GetAddressOf());

  if (!bytecode.Get()) {
    IGL_LOG_ERROR("DXCCompiler: No bytecode produced\n");
    return Result(Result::Code::RuntimeError, "No bytecode produced");
  }

  // Copy bytecode to output
  const uint8_t* data = static_cast<const uint8_t*>(bytecode->GetBufferPointer());
  size_t size = bytecode->GetBufferSize();
  outBytecode.assign(data, data + size);

  IGL_LOG_INFO("DXCCompiler: Compilation successful (%zu bytes DXIL bytecode)\n", size);

  return Result();
}

} // namespace igl::d3d12
