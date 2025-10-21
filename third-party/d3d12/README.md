# DirectX 12 Dependencies

This directory contains information about DirectX 12 dependencies for the IGL D3D12 backend.

## Requirements

The DirectX 12 backend requires the following components:

### Windows SDK
- **Minimum Version:** Windows 10 SDK (10.0.19041.0) or later
- **Recommended:** Latest Windows 11 SDK
- **Installation:** Via Visual Studio Installer or standalone Windows SDK installer

### Required Headers
- `d3d12.h` - DirectX 12 API
- `dxgi1_6.h` - DXGI (DirectX Graphics Infrastructure)
- `d3d12sdklayers.h` - Debug layer support
- `dxcapi.h` - DirectX Shader Compiler API

### Required Libraries
- `d3d12.lib` - DirectX 12 runtime
- `dxgi.lib` - DXGI runtime
- `dxguid.lib` - GUID definitions
- `dxcompiler.lib` - DirectX Shader Compiler

### DirectX Shader Compiler (DXC)
- **Source:** https://github.com/microsoft/DirectXShaderCompiler
- **Installation:**
  - Included with Windows SDK (10.0.20348.0+)
  - Or download from GitHub releases
- **Required Files:**
  - `dxcompiler.dll`
  - `dxil.dll`
  - `dxcapi.h`

### D3DX12 Helper Library
- **Source:** https://github.com/microsoft/DirectX-Headers
- **File:** `d3dx12.h` (header-only helper library)
- **Installation:** Included in this directory

## Directory Structure

```
third-party/d3d12/
├── README.md           # This file
├── include/            # Additional headers (d3dx12.h)
└── doc/                # Documentation links
```

## Platform Support

- **Windows 10:** Version 1909 (Build 18363) or later
- **Windows 11:** All versions
- **Architecture:** x64, ARM64

## CMake Integration

The D3D12 backend is optional and controlled by the `IGL_WITH_D3D12` CMake option:

```bash
cmake -DIGL_WITH_D3D12=ON ..
```

When enabled, the build system will:
1. Verify Windows platform
2. Link against system-provided D3D12 libraries
3. Include d3dx12.h helper from this directory

## Verification

To verify your DirectX 12 SDK installation:

```powershell
# Check for dxc.exe (DirectX Shader Compiler)
where dxc

# Check Windows SDK version
reg query "HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots" /v KitsRoot10
```

## References

- [DirectX 12 Programming Guide](https://learn.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-guide)
- [DirectX Shader Compiler](https://github.com/microsoft/DirectXShaderCompiler)
- [DirectX-Headers](https://github.com/microsoft/DirectX-Headers)
- [Agility SDK](https://devblogs.microsoft.com/directx/directx12agility/)
