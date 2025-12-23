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
#include <d3dcompiler.h> // For D3DCompile (legacy HLSL compiler)
#include <dxcapi.h>

// D3DX12 helper library (header-only)
// Disable buggy helper classes that have preprocessor issues or require newer SDK
#define D3DX12_NO_STATE_OBJECT_HELPERS
#define D3DX12_NO_CHECK_FEATURE_SUPPORT_CLASS

// Manually include only the d3dx12 headers we need (excluding incompatible ones)
// These headers are vendored from Microsoft's DirectX-Headers repository
#include <d3d12.h>
#include <d3dx12/d3dx12_barriers.h>
#include <d3dx12/d3dx12_core.h>
#include <d3dx12/d3dx12_default.h>
#include <d3dx12/d3dx12_pipeline_state_stream.h>
#include <d3dx12/d3dx12_render_pass.h>
// Excluded: d3dx12_resource_helpers.h (requires property_format_table.h which needs newer SDK)
// Excluded: d3dx12_property_format_table.h (requires newer SDK)
#include <d3dx12/d3dx12_root_signature.h>

// ComPtr for COM object management
// IGL's minimal ComPtr-like smart pointer implementing the subset of Microsoft::WRL::ComPtr
// used by the D3D12 backend. We keep it custom to avoid preprocessor issues with <wrl/client.h>.
//
// WARNING: Do not include <wrl/client.h> before this header in the same translation unit.
// Use igl::d3d12::ComPtr instead of Microsoft::WRL::ComPtr throughout the D3D12 backend.
// (Including <wrl/client.h> after this header is technically safe but unsupported.)
//
// Supported operations: Get, GetAddressOf, ReleaseAndGetAddressOf, Reset, Attach, Detach,
// As<U>, TryAs<U>, CopyTo, comparison operators, bool conversion, move/copy semantics.
// Operations NOT implemented: ComPtrRef, operator&, CopyTo(REFIID, void**), and other WRL
// internals.

// Compile-time guard: fail if <wrl/client.h> was already included before this header
#if defined(__WRL_CLIENT_H__) || defined(_WRL_CLIENT_H_)
#error \
    "D3D12Headers.h must be included before <wrl/client.h>. The D3D12 backend uses igl::d3d12::ComPtr exclusively."
#endif

namespace igl {
namespace d3d12 {
template<typename T>
class ComPtr {
 public:
  ComPtr() noexcept : ptr_(nullptr) {}
  ComPtr(T* ptr) noexcept : ptr_(ptr) {
    if (ptr_)
      ptr_->AddRef();
  }

  // Copy constructor - AddRef the pointer
  ComPtr(const ComPtr& other) noexcept : ptr_(other.ptr_) {
    if (ptr_)
      ptr_->AddRef();
  }

  // Copy assignment - AddRef new, Release old
  ComPtr& operator=(const ComPtr& other) noexcept {
    if (this != &other) {
      if (other.ptr_)
        other.ptr_->AddRef();
      if (ptr_)
        ptr_->Release();
      ptr_ = other.ptr_;
    }
    return *this;
  }

  // Move constructor
  ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) {
    other.ptr_ = nullptr;
  }

  // Move assignment
  ComPtr& operator=(ComPtr&& other) noexcept {
    if (this != &other) {
      if (ptr_)
        ptr_->Release();
      ptr_ = other.ptr_;
      other.ptr_ = nullptr;
    }
    return *this;
  }

  // Destructor
  ~ComPtr() {
    if (ptr_)
      ptr_->Release();
  }

  // Accessor methods
  T* Get() const noexcept {
    return ptr_;
  }
  T** GetAddressOf() noexcept {
    return &ptr_;
  }
  T* operator->() const noexcept {
    return ptr_;
  }

  // Dereference operator (caller must ensure ptr_ != nullptr)
  T& operator*() const noexcept {
    return *ptr_;
  }

  // Comparison operators (operator< compares raw addresses for use in containers)
  bool operator==(const ComPtr& other) const noexcept {
    return ptr_ == other.ptr_;
  }

  bool operator!=(const ComPtr& other) const noexcept {
    return ptr_ != other.ptr_;
  }

  bool operator<(const ComPtr& other) const noexcept {
    return ptr_ < other.ptr_;
  }

  bool operator==(T* other) const noexcept {
    return ptr_ == other;
  }

  bool operator!=(T* other) const noexcept {
    return ptr_ != other;
  }

  // Boolean conversion for nullptr checks
  explicit operator bool() const noexcept {
    return ptr_ != nullptr;
  }

  // Reset to release current pointer
  void Reset() noexcept {
    if (ptr_) {
      ptr_->Release();
      ptr_ = nullptr;
    }
  }

  // Attach a raw pointer without AddRef
  void Attach(T* ptr) noexcept {
    if (ptr_) {
      ptr_->Release();
    }
    ptr_ = ptr;
  }

  // Detach and return raw pointer without Release
  T* Detach() noexcept {
    T* temp = ptr_;
    ptr_ = nullptr;
    return temp;
  }

  // ReleaseAndGetAddressOf - release current and return address for output
  T** ReleaseAndGetAddressOf() noexcept {
    Reset();
    return &ptr_;
  }

  // As<U>() - QueryInterface to another interface type
  // Note: Unlike WRL's ComPtr::As, this implementation adds null-safety:
  // - Returns E_POINTER if 'other' is null
  // - Returns E_FAIL and resets 'other' if this->ptr_ is null (no object to query)
  // - Otherwise returns HRESULT from QueryInterface (S_OK or E_NOINTERFACE typically)
  // WRL assumes non-null and relies only on QueryInterface return value.
  // Rationale: Explicit null checks give more predictable behavior when pointers may be null.
  // Callers should treat any non-S_OK result uniformly as "interface query failed".
  template<typename U>
  HRESULT As(ComPtr<U>* other) const noexcept {
    if (!other) {
      return E_POINTER;
    }
    if (!ptr_) {
      other->Reset();
      return E_FAIL; // No object to query
    }
    return ptr_->QueryInterface(__uuidof(U),
                                reinterpret_cast<void**>(other->ReleaseAndGetAddressOf()));
  }

  // TryAs<U>() - QueryInterface convenience method that returns ComPtr<U>
  // WARNING: Silently drops HRESULT; returns empty ComPtr on failure.
  // For error-sensitive code, prefer the HRESULT-returning As(ComPtr<U>* other) overload.
  // Use case: Optional interface queries where failure is expected/acceptable and doesn't need
  // diagnosis. In code paths that return igl::Result or log errors, prefer As() so you can
  // propagate the HRESULT.
  template<typename U>
  ComPtr<U> TryAs() const noexcept {
    ComPtr<U> result;
    if (ptr_) {
      ptr_->QueryInterface(__uuidof(U), reinterpret_cast<void**>(result.ReleaseAndGetAddressOf()));
    }
    return result;
  }

  // CopyTo - Copy pointer with AddRef
  // Note: Returns S_OK even if ptr_ is null; *other will be set to nullptr (matches WRL)
  HRESULT CopyTo(T** other) const noexcept {
    if (!other) {
      return E_POINTER;
    }
    *other = ptr_;
    if (ptr_) {
      ptr_->AddRef();
    }
    return S_OK;
  }

 private:
  T* ptr_;
};
} // namespace d3d12
} // namespace igl

// For convenience in D3D12 implementation files, you may add a local using declaration:
//   namespace { template<typename T> using ComPtr = igl::d3d12::ComPtr<T>; }
// This reduces verbosity without polluting the global or igl::d3d12 namespace.

namespace Microsoft {
namespace WRL {
// DO NOT define ComPtr here - it conflicts with <wrl/client.h>
// All D3D12 code should use igl::d3d12::ComPtr directly
} // namespace WRL
} // namespace Microsoft

// Note: Library linking is handled by CMake (see src/igl/d3d12/CMakeLists.txt)
// Required libraries: d3d12.lib, dxgi.lib, dxguid.lib, dxcompiler.lib, d3dcompiler.lib

#endif // IGL_D3D12_D3D12HEADERS_H
