# Slang in IGL

IGL can take shaders written once in [Slang](https://shader-slang.org) and run them on every
backend. Slang is an option next to the existing GLSL, MSL, HLSL and SparkSL shaders, not a
replacement for them, and `slangc` is a build-time tool only: generated C++ headers are checked in,
so building IGL never runs it.

## How it works

`generate_slang.py` reads a manifest (`programs.json`) and compiles each program to:

| Backend | Code in the header | Produced by |
|---|---|---|
| Vulkan | SPIR-V words, one module per entry point | `slangc -target spirv` |
| Metal | MSL source with every entry point | `slangc -target metal` |
| D3D12 | HLSL source per entry point, compiled by DXC at runtime | `slangc -target hlsl` |
| WebGPU | WGSL source | `slangc -target wgsl` |
| OpenGL | GLSL 410/430 and GLSL ES 300/310, plus resource names | SPIR-V through SPIRV-Cross |

Each program becomes `generated/<Name>Slang.h` next to the manifest. Sessions and tests load it
through `IGLU/slang/SlangProgram.h`: `createRenderStages()` and `createComputeStages()` pick the
code for the device's backend, and `addGlResourceNames()` fills the OpenGL binding maps.

A program uses one of two profiles:
- `portable`: every backend. SPIR-V 1.3, HLSL `sm_6_0`, GLSL ES 3.0 for graphics and 3.1 for
  compute.
- `highend`: Vulkan and Metal only. SPIR-V 1.5, so mesh shaders, `half`, buffer pointers and dynamic
  dispatch are available.

## Writing a program

Declare resources with the macros in `igl_abi.slangh`, which place each one at the same IGL index on
every backend:

| Macro | IGL binding |
|---|---|
| `IGL_UNIFORM_BUFFER(Type, name, i)` | `bindBuffer(i)` |
| `IGL_STORAGE_BUFFER(Type, name, i)` | `bindBuffer(i)`, read-write |
| `IGL_READONLY_STORAGE_BUFFER(Type, name, i)` | `bindBuffer(i)`, read-only (`highend`) |
| `IGL_TEXTURE2D(name, u)` with `IGL_SAMPLE2D(name, uv)` | `bindTexture(u)` and `bindSamplerState(u)` |
| `IGL_SUBPASS_INPUT(name, i)` with `name.SubpassLoad()` | color attachment `i` as an input attachment of a subpass (`highend`) |
| `IGL_SV_VERTEX_ID`, `IGL_SV_INSTANCE_ID` | vertex and instance index |

Rules the generator enforces or relies on:
- Matrices: programs compile with `-matrix-layout-row-major`, so a column-major (glm) matrix reads as
  its transpose. Transform with `mul(v, M)`.
- On Metal, buffers share one index table with vertex buffers. Keep vertex streams at low indices
  and other buffers above them.
- Portable compute entry points read their parameters from storage buffers, not uniform buffers:
  OpenGL compute pipelines bind storage blocks only.
- Portable programs keep compute storage buffers dense from index 0 (as D3D12 requires), use at most
  5 vertex attributes with semantics `POSITION`, `TEXCOORD`, `COLOR`, `NORMAL` and `TANGENT`, and
  draw with base vertex and base instance 0.
- Never name an entry point `main`: Metal reserves it.
- Fragment outputs that skip color attachments, as in a subpass, declare `[[vk::location(i)]]` with
  `SV_Target<i>`: Metal takes the attachment from `SV_Target`, but SPIR-V numbers outputs from 0.

Dynamic dispatch needs the conformance table: list `"conformances": ["Type:IInterface=id", ...]` in
the manifest entry, using typealiases for generic types.

## Regenerating

```sh
python3 generate_slang.py --manifest <dir>/programs.json --slangc <slangc> \
    --spirv-cross <spirv-cross> [--spirv-val <spirv-val>] [--dxc <dxc>] [--metal]
```

The optional tools validate every output: `spirv-val` checks the SPIR-V, `dxc` compiles the HLSL,
and `--metal` compiles the MSL with `xcrun metal`. `--check` regenerates in a temporary directory
and fails if the checked-in headers differ.

## Existing GLSL

`glsl2slang.py` converts GLSL shaders into a Slang program that follows these rules:

```sh
python3 glsl2slang.py --vertex shader.vert --fragment shader.frag -o Program.slang \
    [--texture NAME=UNIT]... [--buffer NAME=INDEX]... [--uniforms-index N]
```

It keeps statements, comments and preprocessor lines, renames types and built-ins, rewrites matrix
products by the matrix rule (`M * v` becomes `mul(v, M)`), turns samplers, uniform blocks, loose
uniforms and storage buffers into the macros above, and wraps each stage's inputs and outputs in an
entry point. Bindings come from the GLSL layout qualifiers unless `--texture` or `--buffer` moves
them; a buffer at index 0 draws a warning, since Metal keeps the vertex stream there. Whatever it
cannot convert is reported with its line.

`--verify` compiles the GLSL with glslangValidator and the Slang output with slangc, and compares
their stage inputs and outputs, uniform block sizes, storage buffers and samplers (pass the tools
with `--glslang`, `--slangc` and `--spirv-cross`). It needs GLSL that compiles for Vulkan; for
GLSL ES 1.0, check the output by rendering it.

`slangc` also reads GLSL directly (`-lang glsl`), which compiles a Vulkan GLSL shader to MSL, HLSL
or WGSL without converting it.
