/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#ifndef IGL_D3D12_D3D12HEADERS_H
#define IGL_D3D12_D3D12HEADERS_H

// Windows headers
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Don't use WIN32_LEAN_AND_MEAN - it excludes wrl/client.h
#include <windows.h>

// DirectX 12 headers
#include <d3d12.h>
#include <dxgi1_6.h>

// DirectX Shader Compiler
#include <dxcapi.h>
#include <d3dcompiler.h>  // For D3DCompile (legacy HLSL compiler)

// D3DX12 helper library (header-only)
// Disable buggy helper classes that have preprocessor issues or require newer SDK
#define D3DX12_NO_STATE_OBJECT_HELPERS
#define D3DX12_NO_CHECK_FEATURE_SUPPORT_CLASS

// Manually include only the d3dx12 headers we need (excluding incompatible ones)
#include <d3d12.h>
#include <d3dx12_barriers.h>
#include <d3dx12_core.h>
#include <d3dx12_default.h>
#include <d3dx12_pipeline_state_stream.h>
#include <d3dx12_render_pass.h>
// Excluded: d3dx12_resource_helpers.h (requires property_format_table.h which needs newer SDK)
// Excluded: d3dx12_property_format_table.h (requires newer SDK)
#include <d3dx12_root_signature.h>

// ComPtr for COM object management
// NOTE: We cannot use <wrl/client.h> with the traditional MSVC preprocessor
// because it causes namespace parsing errors. For Phase 1 stubs, we'll use
// raw pointers and implement our own simple ComPtr wrapper later.
namespace Microsoft {
namespace WRL {
  template<typename T>
  class ComPtr {
   public:
    ComPtr() noexcept : ptr_(nullptr) {}
    ComPtr(T* ptr) noexcept : ptr_(ptr) {}
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    ComPtr& operator=(ComPtr&& other) noexcept {
      if (this != &other) {
        if (ptr_) ptr_->Release();
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
      }
      return *this;
    }
    ~ComPtr() { if (ptr_) ptr_->Release(); }

    T* Get() const noexcept { return ptr_; }
    T** GetAddressOf() noexcept { return &ptr_; }
    T* operator->() const noexcept { return ptr_; }

    void Reset() noexcept {
      if (ptr_) {
        ptr_->Release();
        ptr_ = nullptr;
      }
    }

   private:
    T* ptr_;
  };
} // namespace WRL
} // namespace Microsoft

// Link required libraries
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "d3dcompiler.lib")  // For D3DCompile (legacy HLSL compiler)

#endif // IGL_D3D12_D3D12HEADERS_H
