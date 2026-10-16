Introduction
============

Supported rendering backends
----------------------------

 * Metal 2+
 * OpenGL 2.x (requires [GL_ARB_framebuffer_object](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_framebuffer_object.txt))
 * OpenGL 3.1+
 * OpenGL ES 2.0+
 * Vulkan 1.1 (requires [VK_KHR_buffer_device_address](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_buffer_device_address.html) and [VK_EXT_descriptor_indexing](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_descriptor_indexing.html))
 * DirectX 12
 * WebGPU, on [Dawn](https://dawn.googlesource.com/dawn) natively and in the browser through Emscripten
 * WebGL 2.0

OpenGL ES, DirectX 12, WebGPU and WebGL are opt-in: `IGL_WITH_OPENGLES`, `IGL_WITH_D3D12`,
`IGL_WITH_WEBGPU` and `IGL_WITH_WEBGL` default to `OFF`.

Supported platforms
-------------------

 * Android
 * iOS
 * Linux
 * macOS
 * Windows
 * WebAssembly

Shading languages
-----------------

Shaders can be written in GLSL, MSL, HLSL or SparkSL, and in [Slang](https://shader-slang.org).
A Slang shader is written once and compiled ahead of time to SPIR-V, MSL, HLSL, WGSL and GLSL, so
the same source runs on every backend. The generated C++ headers are checked in, so building IGL
never runs `slangc`.

Samples
-------

Three desktop samples and over thirty render sessions ship with IGL, including two written entirely
in Slang. They are listed in
[getting-started.md](https://github.com/facebook/igl/blob/main/getting-started.md) and built by
default (`IGL_WITH_SAMPLES`).
