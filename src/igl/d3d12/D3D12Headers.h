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
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// DirectX 12 headers
#include <d3d12.h>
#include <dxgi1_6.h>

// DirectX Shader Compiler
#include <dxcapi.h>

// D3DX12 helper library (header-only)
#include <d3dx12.h>

// ComPtr for COM object management
#include <wrl/client.h>

// Link required libraries
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

#endif // IGL_D3D12_D3D12HEADERS_H
