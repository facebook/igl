# IGL Project Structure Analysis

**Date:** 2025-01-20
**Purpose:** Understanding the folder and file structure for DirectX 12 backend integration

---

## Root Directory Structure

```
igl/
├── .git/                          # Git version control
├── .github/                       # GitHub Actions CI/CD workflows
├── build/                         # CMake build output (gitignored)
├── cmake/                         # CMake modules and helpers
├── docs/                          # Documentation
├── IGLU/                          # IGL Utility library
├── samples/                       # Sample applications
├── shell/                         # Platform abstraction and render sessions
├── src/                           # Core IGL library source code
├── third-party/                   # External dependencies
├── CMakeLists.txt                 # Root CMake configuration
├── README.md                      # Project overview
├── getting-started.md             # Build and usage guide
├── DIRECTX12_MIGRATION_PLAN.md    # D3D12 migration strategy
└── DIRECTX12_MIGRATION_PROGRESS.md # D3D12 migration tracking
```

---

## Core Source Structure (`src/igl/`)

### Backend Implementations

```
src/igl/
├── *.cpp, *.h                     # Core IGL interface definitions
│   ├── Device.h                   # Abstract device interface
│   ├── CommandQueue.h             # Command queue interface
│   ├── CommandBuffer.h            # Command buffer interface
│   ├── Buffer.h                   # Buffer resource interface
│   ├── Texture.h                  # Texture resource interface
│   ├── RenderPipelineState.h     # Graphics pipeline interface
│   ├── ComputePipelineState.h    # Compute pipeline interface
│   ├── Framebuffer.h              # Framebuffer interface
│   ├── ShaderModule.h             # Shader module interface
│   └── ...                        # Other core interfaces
│
├── android/                       # Android-specific utilities
├── win/                           # Windows-specific utilities
│   └── LogDefault.cpp/h           # Windows logging implementation
│
├── opengl/                        # OpenGL/GLES backend
│   ├── CMakeLists.txt
│   ├── Device.cpp/h
│   ├── CommandQueue.cpp/h
│   ├── egl/                       # EGL platform integration
│   └── util/                      # OpenGL utilities
│
├── vulkan/                        # Vulkan backend ⭐ Reference for D3D12
│   ├── CMakeLists.txt             # Vulkan build configuration
│   ├── Device.cpp/h               # VkDevice wrapper
│   ├── CommandQueue.cpp/h         # VkQueue wrapper
│   ├── CommandBuffer.cpp/h        # VkCommandBuffer wrapper
│   ├── RenderCommandEncoder.cpp/h # Render pass encoding
│   ├── ComputeCommandEncoder.cpp/h # Compute encoding
│   ├── VulkanContext.cpp/h        # Core Vulkan state (device, queues, swapchain)
│   ├── VulkanBuffer.cpp/h         # Buffer resources
│   ├── VulkanImage.cpp/h          # Image/texture resources
│   ├── VulkanSwapchain.cpp/h      # Swapchain management
│   ├── VulkanHelpers.cpp/h        # Utility functions
│   ├── Buffer.cpp/h               # IGL buffer implementation
│   ├── Texture.cpp/h              # IGL texture implementation
│   ├── RenderPipelineState.cpp/h  # Graphics pipelines
│   ├── ComputePipelineState.cpp/h # Compute pipelines
│   ├── Framebuffer.cpp/h          # Framebuffer implementation
│   ├── ShaderModule.cpp/h         # SPIR-V shader modules
│   ├── android/                   # Android-specific Vulkan code
│   ├── moltenvk/                  # MoltenVK (macOS) specific code
│   └── util/                      # Vulkan utilities
│
├── metal/                         # Metal backend (macOS/iOS)
│   ├── CMakeLists.txt
│   ├── Device.mm/h                # MTLDevice wrapper
│   └── ...                        # Metal implementations
│
├── d3d12/                         # DirectX 12 backend ✅ NEW
│   ├── CMakeLists.txt             # ✅ Created in Phase 0
│   ├── README.md                  # ✅ Created in Phase 0
│   ├── .gitkeep.cpp               # ✅ Placeholder for build
│   └── [To be implemented in Phase 1+]
│       ├── Common.h/cpp
│       ├── D3D12Headers.h
│       ├── Device.h/cpp
│       ├── CommandQueue.h/cpp
│       ├── CommandBuffer.h/cpp
│       ├── RenderCommandEncoder.h/cpp
│       ├── ComputeCommandEncoder.h/cpp
│       ├── D3D12Context.h/cpp
│       ├── RenderPipelineState.h/cpp
│       ├── ComputePipelineState.h/cpp
│       ├── Buffer.h/cpp
│       ├── Texture.h/cpp
│       ├── Sampler.h/cpp
│       ├── Framebuffer.h/cpp
│       ├── ShaderModule.h/cpp
│       ├── DescriptorHeapPool.h/cpp
│       ├── RootSignature.h/cpp
│       ├── DXGISwapchain.h/cpp
│       └── D3D12Helpers.h/cpp
│
├── glslang/                       # GLSL compiler integration
│   ├── CMakeLists.txt
│   └── GLSLCompiler.cpp/h         # Shader compilation utilities
│
└── tests/                         # IGL unit tests
    └── ...
```

---

## Shell Layer (`shell/`)

The shell provides platform abstraction and sample render sessions.

```
shell/
├── shared/                        # Cross-platform utilities
│   ├── renderSession/             # Render session base classes
│   ├── platform/                  # Platform abstraction
│   ├── fileLoader/                # File I/O utilities
│   ├── imageLoader/               # Image loading
│   └── input/                     # Input handling
│
├── windows/                       # Windows platform layer
│   ├── common/
│   │   └── GlfwShell.cpp/h        # GLFW-based window management
│   ├── opengl/
│   │   └── App.cpp                # OpenGL application entry
│   ├── opengles/
│   │   └── App.cpp                # OpenGL ES application entry
│   ├── vulkan/
│   │   └── App.cpp                # Vulkan application entry
│   └── d3d12/                     # ⚠️ TO BE ADDED for D3D12 samples
│       └── App.cpp                # Future: D3D12 application entry
│
├── renderSessions/                # Sample render sessions ⭐ Key for testing
│   ├── EmptySession.cpp/h         # Phase 2 target: Clear screen
│   ├── TinyMeshSession.cpp/h      # Phase 3 target: Triangle rendering
│   ├── HelloWorldSession.cpp/h    # Similar to TinyMesh
│   ├── ColorSession.cpp/h         # Textured quad
│   ├── BasicFramebufferSession.cpp/h
│   ├── ImguiSession.cpp/h
│   ├── Textured3DCubeSession.cpp/h
│   └── ...                        # Many more sessions
│
├── rust/                          # Rust bindings and examples
│   ├── three-cubes/               # Phase 4 target: 3 rotating cubes
│   │   ├── src/
│   │   │   ├── main.rs
│   │   │   └── render_session.rs
│   │   └── Cargo.toml
│   ├── igl-rs/                    # Rust IGL bindings
│   ├── igl-sys/                   # FFI bindings
│   └── igl-c-wrapper/             # C wrapper for Rust
│
├── android/                       # Android platform layer
├── ios/                           # iOS platform layer
├── mac/                           # macOS platform layer
├── openxr/                        # OpenXR VR/AR support
├── resources/                     # Textures, models, shaders
│   ├── images/
│   ├── models/
│   └── shaders/
└── CMakeLists.txt                 # Shell build configuration
```

---

## Samples (`samples/`)

Production-ready sample applications.

```
samples/
├── desktop/                       # Desktop samples (Windows, Linux, macOS)
│   └── Tiny/
│       ├── Tiny.cpp               # Basic triangle
│       ├── Tiny_Mesh.cpp          # Textured meshes + ImGui
│       └── Tiny_MeshLarge.cpp     # Complex Bistro scene
│
├── android/                       # Android samples
│   ├── opengl/
│   └── vulkan/
│
├── ios/                           # iOS samples
└── wasm/                          # WebAssembly samples
```

---

## Third-Party Dependencies (`third-party/`)

```
third-party/
├── bootstrap.py                   # Dependency download script
├── bootstrap-deps.json            # Dependency manifest
├── .gitignore                     # Ignores deps/ and content/
│
├── deps/                          # Downloaded dependencies (gitignored)
│   ├── src/                       # Source code of dependencies
│   │   ├── glslang/               # GLSL to SPIR-V compiler
│   │   ├── SPIRV-Headers/         # SPIR-V header files
│   │   ├── SPIRV-Cross/           # SPIR-V to HLSL/MSL/GLSL transpiler
│   │   ├── volk/                  # Vulkan meta-loader
│   │   ├── vma/                   # Vulkan Memory Allocator
│   │   ├── glfw/                  # Windowing library
│   │   ├── glew/                  # OpenGL extension loader
│   │   ├── stb/                   # STB image libraries
│   │   ├── glm/                   # OpenGL Mathematics
│   │   ├── fmt/                   # C++ formatting library
│   │   ├── imgui/                 # Dear ImGui
│   │   ├── meshoptimizer/         # Mesh optimization
│   │   ├── tinyobjloader/         # OBJ model loader
│   │   ├── bc7enc/                # Texture compression
│   │   ├── taskflow/              # C++ parallel programming
│   │   ├── ktx-software/          # KTX texture format
│   │   ├── gtest/                 # Google Test
│   │   └── openxr-sdk/            # OpenXR SDK
│   │
│   ├── patches/                   # Patches for dependencies
│   └── archives/                  # Downloaded archives
│
└── d3d12/                         # ✅ DirectX 12 resources (NEW)
    ├── README.md                  # ✅ D3D12 SDK documentation
    └── include/                   # ✅ D3D12 helper headers
        ├── d3dx12.h               # ✅ Main helper header
        ├── d3dx12_barriers.h      # ✅ Resource barriers
        ├── d3dx12_core.h          # ✅ Core helpers
        ├── d3dx12_default.h       # ✅ Default descriptors
        ├── d3dx12_pipeline_state_stream.h
        ├── d3dx12_render_pass.h
        ├── d3dx12_resource_helpers.h
        ├── d3dx12_root_signature.h
        ├── d3dx12_property_format_table.h
        ├── d3dx12_state_object.h
        └── d3dx12_check_feature_support.h
```

---

## Build System (`build/`)

The `build/` directory is created by the user and contains CMake-generated files.

### Windows Build Process

```bash
# 1. Deploy dependencies (downloads third-party libraries)
python3 deploy_deps.py
python3 deploy_content.py

# 2. Create build directory
mkdir build
cd build

# 3. Generate Visual Studio solution
cmake .. -G "Visual Studio 17 2022"

# 4. With D3D12 enabled:
cmake .. -G "Visual Studio 17 2022" -DIGL_WITH_D3D12=ON

# 5. Open Visual Studio
# build/IGL.sln will be generated
```

### Build Directory Contents (After CMake)

```
build/
├── IGL.sln                        # Visual Studio solution
├── CMakeCache.txt                 # CMake configuration cache
├── CMakeFiles/                    # CMake metadata
├── src/                           # Generated projects for src/
│   └── igl/
│       ├── IGLLibrary.vcxproj     # Main IGL library project
│       ├── d3d12/
│       │   └── IGLD3D12.vcxproj   # D3D12 backend project
│       ├── vulkan/
│       │   └── IGLVulkan.vcxproj  # Vulkan backend project
│       └── ...
├── shell/                         # Generated shell projects
├── samples/                       # Generated sample projects
├── Debug/                         # Debug build outputs
│   ├── IGLLibrary.lib
│   ├── IGLD3D12.lib
│   ├── IGLVulkan.lib
│   └── *.exe                      # Sample executables
├── Release/                       # Release build outputs
└── ...
```

---

## CMake Build Configuration

### Root CMakeLists.txt Options

```cmake
option(IGL_WITH_SAMPLES   "Enable sample demo apps"     ON)
option(IGL_WITH_OPENGL    "Enable IGL/OpenGL"           ON)
option(IGL_WITH_OPENGLES  "Enable IGL/OpenGL ES"       OFF)
option(IGL_WITH_VULKAN    "Enable IGL/Vulkan"           ON)
option(IGL_WITH_METAL     "Enable IGL/Metal"            ON)
option(IGL_WITH_WEBGL     "Enable IGL/WebGL"           OFF)
option(IGL_WITH_D3D12     "Enable IGL/DirectX 12"      OFF) # ✅ NEW
option(IGL_WITH_IGLU      "Enable IGLU utils"           ON)
option(IGL_WITH_SHELL     "Enable Shell utils"          ON)
option(IGL_WITH_TESTS     "Enable IGL tests"           OFF)
```

### Backend Library Linking

```cmake
# In src/igl/CMakeLists.txt
if(IGL_WITH_VULKAN)
  add_subdirectory(vulkan)
  target_link_libraries(IGLLibrary PUBLIC IGLVulkan)
endif()

if(IGL_WITH_D3D12)
  add_subdirectory(d3d12)                              # ✅ NEW
  target_link_libraries(IGLLibrary PUBLIC IGLD3D12)    # ✅ NEW
endif()
```

### D3D12 Backend CMakeLists.txt

```cmake
# src/igl/d3d12/CMakeLists.txt
project(IGLD3D12 CXX C)

file(GLOB SRC_FILES *.cpp)
file(GLOB HEADER_FILES *.h)

add_library(IGLD3D12 ${SRC_FILES} ${HEADER_FILES})

target_link_libraries(IGLD3D12 PRIVATE IGLLibrary IGLGlslang)

# Link DirectX 12 system libraries
target_link_libraries(IGLD3D12 PUBLIC
  d3d12.lib
  dxgi.lib
  dxguid.lib
  dxcompiler.lib
)

# Include d3dx12 helper headers
target_include_directories(IGLD3D12 PUBLIC
  "${IGL_ROOT_DIR}/third-party/d3d12/include"
)
```

---

## DirectX 12 Integration Points

### 1. Backend Library (`src/igl/d3d12/`)
- Implements IGL interfaces for D3D12
- Compiles to `IGLD3D12.lib`
- Linked into `IGLLibrary`

### 2. Platform Layer (`shell/windows/d3d12/`)
- **To be created** in future phases
- Provides D3D12-specific windowing and initialization
- Similar to `shell/windows/vulkan/App.cpp`

### 3. Sample Applications
- Use IGL interfaces (backend-agnostic)
- Backend selected at runtime via device creation
- RenderSessions work with any backend

### 4. Build Configuration
- CMake option: `-DIGL_WITH_D3D12=ON`
- Preprocessor define: `IGL_BACKEND_ENABLE_D3D12=1`
- Windows-only (enforced in CMake)

---

## Key Files for D3D12 Migration

### Reference Files (Vulkan Backend)
```
src/igl/vulkan/VulkanContext.cpp     # Device, queue, swapchain setup
src/igl/vulkan/Device.cpp            # Device implementation
src/igl/vulkan/CommandBuffer.cpp     # Command recording
src/igl/vulkan/RenderCommandEncoder.cpp # Render pass encoding
src/igl/vulkan/Buffer.cpp            # Buffer resources
src/igl/vulkan/Texture.cpp           # Texture resources
src/igl/vulkan/RenderPipelineState.cpp # Graphics pipelines
shell/windows/vulkan/App.cpp         # Windows platform entry
```

### Target RenderSessions for Testing
```
shell/renderSessions/EmptySession.cpp        # Phase 2: Clear screen
shell/renderSessions/TinyMeshSession.cpp     # Phase 3: Triangle
shell/rust/three-cubes/                      # Phase 4: 3 cubes
```

---

## Phase 0 Completion Status ✅

### Created Files
- [x] `third-party/d3d12/README.md`
- [x] `third-party/d3d12/include/*.h` (11 d3dx12 helper headers)
- [x] `src/igl/d3d12/CMakeLists.txt`
- [x] `src/igl/d3d12/README.md`
- [x] `src/igl/d3d12/.gitkeep.cpp`

### Modified Files
- [x] `CMakeLists.txt` (root)
  - Added `IGL_WITH_D3D12` option
  - Added platform check (Windows only)
  - Added status message output
  - Added backend validation
  - Added compile definition `IGL_BACKEND_ENABLE_D3D12=1`
- [x] `src/igl/CMakeLists.txt`
  - Added D3D12 subdirectory and linking

### Next Steps (Phase 1)
- Create 13 stub classes in `src/igl/d3d12/`
- Each stub returns `Result::Code::Unimplemented`
- Verify project compiles with D3D12 enabled

---

## Summary

The IGL project is well-structured with clear separation:
- **Core interfaces** (`src/igl/*.h`) - Backend-agnostic
- **Backend implementations** (`src/igl/{vulkan,opengl,metal,d3d12}/`) - API-specific
- **Platform layer** (`shell/`) - OS and windowing abstraction
- **Samples** - Demonstration applications
- **Build** - CMake-generated output (Visual Studio .sln on Windows)

The D3D12 backend follows the same pattern as Vulkan, making migration straightforward due to the 87% API similarity.
