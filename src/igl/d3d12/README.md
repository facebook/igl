# IGL DirectX 12 Backend

This directory contains the DirectX 12 backend implementation for IGL (Intermediate Graphics Library).

## Status

**Current Phase:** Phase 0 - CMake Setup Complete
**Next Phase:** Phase 1 - Stub Infrastructure

## Architecture

The D3D12 backend follows the same architectural pattern as the Vulkan backend, leveraging the 87% similarity between the two APIs.

### Core Components (To Be Implemented)

```
d3d12/
├── Common.h/cpp                  - Common types, constants, utilities
├── D3D12Headers.h                - D3D12/DXGI includes wrapper
├── Device.h/cpp                  - ID3D12Device wrapper
├── CommandQueue.h/cpp            - ID3D12CommandQueue wrapper
├── CommandBuffer.h/cpp           - ID3D12GraphicsCommandList wrapper
├── RenderCommandEncoder.h/cpp    - Render command encoding
├── ComputeCommandEncoder.h/cpp   - Compute command encoding
├── D3D12Context.h/cpp            - Core D3D12 state management
├── RenderPipelineState.h/cpp     - ID3D12PipelineState wrapper
├── ComputePipelineState.h/cpp    - Compute pipeline state
├── Buffer.h/cpp                  - ID3D12Resource (buffers)
├── Texture.h/cpp                 - ID3D12Resource (textures)
├── Sampler.h/cpp                 - D3D12_SAMPLER_DESC
├── Framebuffer.h/cpp             - RTV + DSV collection
├── ShaderModule.h/cpp            - DXIL/DXBC bytecode
├── ShaderStages.h/cpp            - Shader stage management
├── DescriptorHeapPool.h/cpp      - Descriptor heap management
├── RootSignature.h/cpp           - Root signature cache
├── DXGISwapchain.h/cpp           - DXGI swapchain wrapper
└── D3D12Helpers.h/cpp            - Utility functions
```

## Build Instructions

### Prerequisites

- Windows 10 1909+ or Windows 11
- Visual Studio 2019 or later
- Windows SDK (10.0.19041.0 or later)
- DirectX Shader Compiler (DXC) - included with Windows SDK 10.0.20348.0+

### CMake Configuration

```bash
cmake -DIGL_WITH_D3D12=ON -DIGL_WITH_VULKAN=OFF -DIGL_WITH_OPENGL=OFF ..
```

Or with other backends enabled:

```bash
cmake -DIGL_WITH_D3D12=ON ..
```

### Build

```bash
cmake --build . --config Release
```

## Implementation Plan

See [DIRECTX12_MIGRATION_PLAN.md](../../../DIRECTX12_MIGRATION_PLAN.md) for the complete migration plan.

### Progress

- [x] Phase 0: CMake Setup
  - [x] DirectX 12 Agility SDK headers
  - [x] CMake configuration
- [ ] Phase 1: Stub Infrastructure (13 stub classes)
- [ ] Phase 2: EmptySession (Clear screen)
- [ ] Phase 3: TinyMeshSession (Triangle rendering)
- [ ] Phase 4: three-cubes (Full demo)

## References

- [DirectX 12 Programming Guide](https://learn.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-guide)
- [DirectX Shader Compiler](https://github.com/microsoft/DirectXShaderCompiler)
- [DirectX-Headers](https://github.com/microsoft/DirectX-Headers)

## License

Licensed under the MIT License. See [LICENSE](../../../LICENSE.md) for details.
