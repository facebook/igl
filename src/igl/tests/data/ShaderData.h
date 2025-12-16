/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef> // For size_t/
#include <string_view>
#include <igl/Macros.h>
#if IGL_BACKEND_OPENGL
#include <igl/opengl/Config.h>
#endif // IGL_BACKEND_OPENGL

namespace igl::tests::data::shader {

//-----------------------------------------------------------------------------
// Defines names of inputs and functions for the shaders in this file
//-----------------------------------------------------------------------------
constexpr std::string_view kShaderFunc = "main"; // For OGL and VK
constexpr std::string_view kSimpleVertFunc = "vertexShader";
constexpr std::string_view kSimpleFragFunc = "fragmentShader";
constexpr std::string_view kSimplePos = "position_in";
constexpr size_t kSimplePosIndex = 0;
constexpr std::string_view kSimpleUv = "uv_in";
constexpr size_t kSimpleUvIndex = 1;
constexpr std::string_view kSimpleSampler = "inputImage";
constexpr std::string_view kSimpleCubeView = "view";

constexpr std::string_view kSimpleComputeFunc = "doubleKernel";
constexpr std::string_view kSimpleComputeInput = "floatsIn";
constexpr std::string_view kSimpleComputeOutput = "floatsOut";
constexpr size_t kSimpleComputeInputIndex = 0;
constexpr size_t kSimpleComputeOutputIndex = 1;

// clang-format off
//-----------------------------------------------------------------------------
// OGL Shaders
//-----------------------------------------------------------------------------
#if IGL_BACKEND_OPENGL && IGL_OPENGL_ES
#define PROLOG precision mediump float;
#else
#define PROLOG
#endif

#define HASH_LIT #
#define HASH() HASH_LIT
#define VERSION(ver) HASH()version ver\n
#define REQUIRE_EXTENSION(ext) HASH()extension ext : require\n

#if IGL_BACKEND_OPENGL && !IGL_OPENGL_ES
#if IGL_PLATFORM_APPLE
#define LEGACY_VERSION VERSION(100) precision mediump float;
#else
#define LEGACY_VERSION
#endif
#else
#define LEGACY_VERSION
#endif

// Simple OGL Vertex shader
constexpr std::string_view kOglSimpleVertShader =
    IGL_TO_STRING(LEGACY_VERSION attribute vec4 position_in; attribute vec2 uv_in; varying vec2 uv;

               void main() {
                 gl_Position = position_in;
                 gl_PointSize = 1.0; // Testing GL_POINTS draw primitive in Windows environment requires this.
                                     // See https://github.com/khronosgroup/webgl/issues/2818
                 uv = uv_in;
               });

// Simple OGL Fragment shader
constexpr std::string_view kOglSimpleFragShader =
    IGL_TO_STRING(LEGACY_VERSION PROLOG uniform sampler2D inputImage; varying vec2 uv;

               void main() {
                 gl_FragColor = texture2D(inputImage, uv);
                 });

constexpr std::string_view kOglSimpleVertShaderEs3 =
    IGL_TO_STRING(VERSION(300 es)
    in vec4 position_in; in vec2 uv_in; out vec2 uv;

               void main() {
                 gl_Position = position_in;
                 gl_PointSize = 1.0; // Testing GL_POINTS draw primitive in Windows environment requires this.
                                     // See https://github.com/khronosgroup/webgl/issues/2818
                 uv = uv_in;
               });

constexpr std::string_view kOglSimpleFragShaderEs3 =
    IGL_TO_STRING(VERSION(300 es)
    PROLOG uniform sampler2D inputImage; in vec2 uv; out vec4 fragColor;

               void main() {
                 fragColor = texture(inputImage, uv);
                 });

constexpr std::string_view kOglSimpleVertShaderMultiviewEs3 =
    IGL_TO_STRING(VERSION(300 es)
    REQUIRE_EXTENSION(GL_OVR_multiview2)
    layout(num_views = 2) in;

    in vec4 position_in;
    out vec4 color;

    uniform vec4 colors[2];

    void main() {
      gl_Position = position_in;
      color = colors[gl_ViewID_OVR];
    });

constexpr std::string_view kOglSimpleFragShaderMultiviewEs3 =
    IGL_TO_STRING(VERSION(300 es)
    PROLOG
    in vec4 color;
    out vec4 fragColor;

    void main() {
      fragColor = color;
    });

constexpr std::string_view kOglSimpleVertShaderTexArray =
IGL_TO_STRING(VERSION(150)
    in vec4 position_in;
    in vec2 uv_in;
    out vec2 uv;
    flat out uint layer_out;

    uniform int layer;

    void main() {
      gl_Position = position_in;
      uv = uv_in;
      layer_out = uint(layer);
    });

constexpr std::string_view kOglSimpleFragShaderTexArray =
IGL_TO_STRING(VERSION(150)
    PROLOG
    in vec2 uv;
    flat in uint layer_out;
    uniform sampler2DArray inputImage;

    out vec4 fragColor;

    void main() {
      fragColor = texture(inputImage, vec3(uv, layer_out));
    });

constexpr std::string_view kOglSimpleVertShaderTexArrayExt =
IGL_TO_STRING(VERSION(110)
    attribute vec4 position_in;
    attribute vec2 uv_in;
    uniform int layer;
    varying vec2 uv;
    varying float layer_out;

    void main() {
      gl_Position = position_in;
      uv = uv_in;
      layer_out = float(layer);
    });

constexpr std::string_view kOglSimpleFragShaderTexArrayExt =
IGL_TO_STRING(VERSION(110)
    REQUIRE_EXTENSION(GL_EXT_texture_array)
    PROLOG

    varying vec2 uv;
    varying float layer_out;
    uniform sampler2DArray inputImage;

    void main() {
      gl_FragColor = texture2DArray(inputImage, vec3(uv, layer_out));
    });

constexpr std::string_view kOglSimpleVertShaderTexArrayEs3 =
IGL_TO_STRING(VERSION(300 es)
    in vec4 position_in;
    in vec2 uv_in;
    out vec2 uv_vs;
    flat out uint layer_vs;

    uniform int layer;

    void main() {
      gl_Position = position_in;
      uv_vs = uv_in;
      layer_vs = uint(layer);
    });

constexpr std::string_view kOglSimpleFragShaderTexArrayEs3 =
IGL_TO_STRING(VERSION(300 es)
    PROLOG
    in vec2 uv_vs;
    flat in uint layer_vs;
    uniform mediump sampler2DArray inputImage;

    out vec4 fragColor;

    void main() {
      fragColor = texture(inputImage, vec3(uv_vs, layer_vs));
    });

// Simple OGL Vertex shader for textureCube and texture3D
constexpr std::string_view kOglSimpleVertShaderCube =
    IGL_TO_STRING(LEGACY_VERSION attribute vec4 position_in; uniform vec4 view; varying vec3 uv;

               void main() {
                 gl_Position = position_in;
                 uv = view.xyz;
               });

// Simple OGL Fragment shader
constexpr std::string_view kOglSimpleFragShaderCube =
    IGL_TO_STRING(LEGACY_VERSION PROLOG uniform samplerCube inputImage; varying vec3 uv;

               void main() { gl_FragColor = textureCube(inputImage, uv); });

// Simple shader which multiplies each float value in the input buffer by 2 and writes the result to the output buffer
constexpr std::string_view kOglSimpleComputeShader =
  IGL_TO_STRING(VERSION(310 es)
        precision highp float;

        layout (local_size_x = 6, local_size_y = 1, local_size_z = 1) in;
        layout (std430, binding = 0) readonly buffer floatsIn {
          float fIn[];
        };
        layout (std430, binding = 1) writeonly buffer floatsOut {
          float fOut[];
        };

        void main() {
            uint id = gl_LocalInvocationIndex;

            fOut[id] = fIn[id] * 2.0f;
        });

constexpr std::string_view kOglSimpleVertShaderUniformBlocks =
      IGL_TO_STRING(VERSION(300 es)
      in vec4 position_in; out vec3 uv;

      layout (std140) uniform block_without_instance_name {
        float scale;
      };

     layout (std140) uniform block_with_instance_name {
        vec3 view;
        vec4 testArray[2];
      } matrices;

      uniform bool non_uniform_block_bool;

      void main() {
        gl_Position = non_uniform_block_bool ? position_in * scale : position_in;
        uv = matrices.view;
      });

constexpr std::string_view kOglSimpleFragShaderUniformBlocks =
    IGL_TO_STRING(VERSION(300 es)
      PROLOG uniform sampler2D inputImage; in vec3 uv; out vec4 fragColor;

      void main() {
        fragColor = texture(inputImage, uv.xy);
      });

//-----------------------------------------------------------------------------
// Metal Shaders
//-----------------------------------------------------------------------------

// Simple Metal Shader
// The vertext function expects vertex and sampler positions to be bound to
// separate buffers
#define MTL_SIMPLE_SHADER_DEF(returnType, swizzle)                              \
    IGL_TO_STRING(using namespace metal;                                        \
                                                                                \
      typedef struct { float3 color; } UniformBlock;                            \
                                                                                \
      typedef struct {                                                          \
        float4 position [[position]];                                           \
        float pointSize [[point_size]];                                         \
        float2 uv;                                                              \
      } VertexOut;                                                              \
                                                                                \
      vertex VertexOut vertexShader(uint vid [[vertex_id]],                     \
                                    constant float4* position_in [[buffer(0)]], \
                                    constant float2* uv_in [[buffer(1)]]) {     \
        VertexOut out;                                                          \
        out.position = position_in[vid];                                        \
        out.pointSize = 1;                                                      \
        out.uv = uv_in[vid];                                                    \
        return out;                                                             \
      }                                                                         \
                                                                                \
      fragment returnType fragmentShader(VertexOut IN [[stage_in]],             \
                                    texture2d<float> diffuseTex [[texture(0)]], \
                                    sampler linearSampler [[sampler(0)]]) {     \
        float4 tex = diffuseTex.sample(linearSampler, IN.uv);                   \
        returnType ret = returnType(tex.swizzle);                               \
        return ret;                                                             \
      });                                                                       \

constexpr std::string_view kMtlSimpleShaderFloat = MTL_SIMPLE_SHADER_DEF(float, r);
constexpr std::string_view kMtlSimpleShaderFloat2 = MTL_SIMPLE_SHADER_DEF(float2, rg);
constexpr std::string_view kMtlSimpleShaderFloat3 = MTL_SIMPLE_SHADER_DEF(float3, rgb);
constexpr std::string_view kMtlSimpleShaderFloat4 = MTL_SIMPLE_SHADER_DEF(float4, rgba);
constexpr std::string_view kMtlSimpleShaderUshort = MTL_SIMPLE_SHADER_DEF(ushort, r);
constexpr std::string_view kMtlSimpleShaderUshort2 = MTL_SIMPLE_SHADER_DEF(ushort2, rg);
constexpr std::string_view kMtlSimpleShaderUshort4 = MTL_SIMPLE_SHADER_DEF(ushort4, rgba);
constexpr std::string_view kMtlSimpleShaderUint4 = MTL_SIMPLE_SHADER_DEF(uint4, rgba);
constexpr std::string_view kMtlSimpleShaderUint = MTL_SIMPLE_SHADER_DEF(uint4, r);
constexpr std::string_view kMtlSimpleShader = MTL_SIMPLE_SHADER_DEF(float4, rgba);

// Simple Metal Shader for 1D Texture
// The vertext function expects vertex and sampler positions to be bound to
// separate buffers
constexpr std::string_view kMtlSimpleShader1dtex =
    IGL_TO_STRING(using namespace metal;

               typedef struct { float3 color; } UniformBlock;

               typedef struct {
                 float4 position [[position]];
                 float2 uv;
               } VertexOut;

               vertex VertexOut vertexShader(uint vid [[vertex_id]],
                                             constant float4* position_in [[buffer(0)]],
                                             constant float2* uv_in [[buffer(1)]]) {
                 VertexOut out;
                 out.position = position_in[vid];
                 out.uv = uv_in[vid];
                 return out;
               }

               fragment float4 fragmentShader(VertexOut IN [[stage_in]],
                                              texture1d<float> diffuseTex [[texture(0)]],
                                              sampler linearSampler [[sampler(0)]]) {
                 float4 tex = diffuseTex.sample(linearSampler, IN.uv.x);

                 return tex;
               });

// Simple Metal Shader
// The vertext function expects vertex and sampler positions to be bound to
// separate buffers
constexpr std::string_view kMtlSimpleShaderCube =
    IGL_TO_STRING(using namespace metal;

               typedef struct {
                 // float3 color;
                 float4 view;
               } UniformBlock;

               typedef struct {
                 float4 position [[position]];
                 float3 uv;
               } VertexOut;

               vertex VertexOut vertexShader(uint vid [[vertex_id]],
                                             constant UniformBlock& uniforms [[buffer(1)]],
                                             constant float4* position_in [[buffer(0)]],
                                             constant float3* uv_in [[buffer(2)]]) {
                 VertexOut out;
                 out.position = position_in[vid];
                 out.uv = uniforms.view.xyz;
                 return out;
               }

               fragment float4 fragmentShader(VertexOut IN [[stage_in]],
                                              texturecube<float> diffuseTex [[texture(0)]],
                                              sampler linearSampler [[sampler(0)]]) {
                 float4 tex = diffuseTex.sample(linearSampler, IN.uv);

                 return tex;
               });

// Simple shader which multiplies each float value in the input buffer by 2 and writes the result to the output buffer
constexpr std::string_view kMtlSimpleComputeShader =
  IGL_TO_STRING(using namespace metal;

      kernel void doubleKernel(
          device float* floatsIn [[buffer(0)]],
          device float* floatsOut [[buffer(1)]],
          uint2 gid [[thread_position_in_grid]]) {
        floatsOut[gid.x] = floatsIn[gid.x] * 2.0;
      });

constexpr std::string_view kMtlSimpleShaderTxt1dArray =
    IGL_TO_STRING(using namespace metal;

               typedef struct {
                 int layer;
               } UniformBlock;

               typedef struct {
                 float4 position [[position]];
                 float2 uv;
                 uint layer;
               } VertexOut;

               vertex VertexOut vertexShader(uint vid [[vertex_id]],
                                             constant UniformBlock& uniforms [[buffer(2)]],
                                             constant float4* position_in [[buffer(0)]],
                                             constant float2* uv_in [[buffer(1)]]) {
                 VertexOut out;
                 out.position = position_in[vid];
                 out.uv = uv_in[vid];
                 out.layer = uniforms.layer;
                 return out;
               }

               fragment float4 fragmentShader(VertexOut IN [[stage_in]],
                                              texture1d_array<float> diffuseTex [[texture(0)]],
                                              sampler linearSampler [[sampler(0)]]) {
                 float4 tex = diffuseTex.sample(linearSampler, IN.uv.x, IN.layer);

                 return tex;
               });


constexpr std::string_view kMtlSimpleShaderTxt2dArray =
    IGL_TO_STRING(using namespace metal;

               typedef struct {
                 int layer;
               } UniformBlock;

               typedef struct {
                 float4 position [[position]];
                 float2 uv;
                 uint layer;
               } VertexOut;

               vertex VertexOut vertexShader(uint vid [[vertex_id]],
                                             constant UniformBlock& uniforms [[buffer(2)]],
                                             constant float4* position_in [[buffer(0)]],
                                             constant float2* uv_in [[buffer(1)]]) {
                 VertexOut out;
                 out.position = position_in[vid];
                 out.uv = uv_in[vid];
                 out.layer = uniforms.layer;
                 return out;
               }

               fragment float4 fragmentShader(VertexOut IN [[stage_in]],
                                              texture2d_array<float> diffuseTex [[texture(0)]],
                                              sampler linearSampler [[sampler(0)]]) {
                 float4 tex = diffuseTex.sample(linearSampler, IN.uv, IN.layer);

                 return tex;
               });

//-----------------------------------------------------------------------------
// Vulkan Shaders
//-----------------------------------------------------------------------------

// Simple Vulkan Vertex shader
constexpr std::string_view kVulkanSimpleVertShader =
    IGL_TO_STRING(
      layout (location=0) in vec4 position_in;
      layout (location=1) in vec2 uv_in;
      layout (location=0) out vec2 uv;

      void main() {
        gl_Position = position_in;
        gl_PointSize = 1.0; // Testing VK_PRIMITIVE_TOPOLOGY_POINT_LIST drawing requires this
        uv = uv_in;
      });

// Simple Vulkan Fragment shader
#define VULKAN_SIMPLE_FRAG_SHADER_DEF(returnType, swizzle)  \
    IGL_TO_STRING(                                          \
      layout (location=0) in vec2 uv;                       \
      layout (location=0) out returnType out_FragColor;     \
                                                            \
      layout (set = 0, binding = 0) uniform sampler2D uTex; \
                                                            \
      void main() {                                         \
        vec4 tex = texture(uTex, uv);                       \
        out_FragColor = returnType(tex.swizzle);            \
      });

constexpr std::string_view kVulkanSimpleFragShader = VULKAN_SIMPLE_FRAG_SHADER_DEF(vec4, rgba);

constexpr std::string_view kVulkanSimpleFragShaderFloat = VULKAN_SIMPLE_FRAG_SHADER_DEF(float, r);
constexpr std::string_view kVulkanSimpleFragShaderFloat2 = VULKAN_SIMPLE_FRAG_SHADER_DEF(vec2, rg);
constexpr std::string_view kVulkanSimpleFragShaderFloat3 = VULKAN_SIMPLE_FRAG_SHADER_DEF(vec3, rgb);
constexpr std::string_view kVulkanSimpleFragShaderFloat4 = VULKAN_SIMPLE_FRAG_SHADER_DEF(vec4, rgba);
constexpr std::string_view kVulkanSimpleFragShaderUint = VULKAN_SIMPLE_FRAG_SHADER_DEF(uint, r);
constexpr std::string_view kVulkanSimpleFragShaderUint2 = VULKAN_SIMPLE_FRAG_SHADER_DEF(uvec2, rg);
constexpr std::string_view kVulkanSimpleFragShaderUint4 = VULKAN_SIMPLE_FRAG_SHADER_DEF(uvec4, rgba);

constexpr std::string_view kVulkanPushConstantVertShader =
    IGL_TO_STRING(
      layout (location=0) in vec4 position_in;
      layout (location=1) in vec2 uv_in;
      layout (location=0) out vec2 uv;

      void main() {
        gl_Position = position_in;
        gl_PointSize = 1.0;
        uv = uv_in;
      });

constexpr std::string_view kVulkanPushConstantFragShader =
    IGL_TO_STRING(
      layout (location=0) in vec2 uv;
      layout (location=0) out vec4 out_FragColor;
      
      layout (set = 0, binding = 0) uniform sampler2D uTex;
      
      layout (push_constant) uniform PushConstants {
        vec4 colorMultiplier;
      } pushConstants;

      void main() {
        vec4 tex = texture(uTex, uv);
        out_FragColor = tex * pushConstants.colorMultiplier;
      });

// D3D12 HLSL push constant shaders
constexpr const char* kD3D12PushConstantVertShader = R"(
struct VSIn { float4 position_in : POSITION; float2 uv_in : TEXCOORD0; };
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
PSIn main(VSIn i) {
  PSIn o;
  o.position = i.position_in;
  o.uv = i.uv_in;
  return o;
}
)";

constexpr const char* kD3D12PushConstantFragShader = R"(
Texture2D inputImage : register(t0);
SamplerState samp0 : register(s0);

cbuffer PushConstants : register(b2) {
  float4 colorMultiplier;
};

struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
float4 main(PSIn i) : SV_TARGET {
  float4 tex = inputImage.Sample(samp0, i.uv);
  return tex * colorMultiplier;
}
)";

constexpr std::string_view kVulkanSimpleVertShaderTex2dArray =
IGL_TO_STRING(
    layout(location = 0) in vec4 position_in;
    layout(location = 1) in vec2 uv_in;
    layout(location = 0) out vec2 uv_out;
    layout(location = 1) out uint layer_out;

    struct VertexUniforms {
      int layer;
    };

    layout(set = 1, binding = 2, std140) uniform PerFrame {
      VertexUniforms perFrame;
    };

    void main() {
      gl_Position = position_in;
      uv_out = uv_in;
      layer_out = perFrame.layer;
    });

constexpr std::string_view kVulkanSimpleFragShaderTex1darray =
IGL_TO_STRING(
    layout(location = 0) in vec2 uv;
    layout(location = 1) in flat uint layer;
    layout(location = 0) out vec4 out_FragColor;

    layout (set = 0, binding = 0) uniform sampler1DArray uTex;

    void main() {
        out_FragColor = texture(uTex, vec2(uv.x, layer));
    });

constexpr std::string_view kVulkanSimpleFragShaderTex2dArray =
IGL_TO_STRING(
    layout(location = 0) in vec2 uv;
    layout(location = 1) in flat uint layer;
    layout(location = 0) out vec4 out_FragColor;

    layout (set = 0, binding = 0) uniform sampler2DArray uTex;

    void main() {
      out_FragColor = texture(uTex, vec3(uv.xy, layer));
    });

constexpr std::string_view kVulkanSimpleVertShaderCube =
    IGL_TO_STRING(layout(location = 0) in vec4 position_in;
                  layout(location = 0) out vec3 view;

                  struct VertexUniforms {
                    vec4 view;
                  };

                  layout(set = 1, binding = 1, std140) uniform PerFrame {
                     VertexUniforms perFrame;
                  };

               void main() {
                 gl_Position = position_in;
                 view = perFrame.view.xyz;
               });

constexpr std::string_view kVulkanSimpleFragShaderCube =
    IGL_TO_STRING(
               layout(location = 0) in vec3 view;
               layout(location = 0) out vec4 out_FragColor;

               layout (set = 0, binding = 0) uniform samplerCube uTex;

               void main() {
                   out_FragColor = texture(uTex, view);
               });

// Simple Vulkan Vertex shader for multiview
constexpr std::string_view kVulkanSimpleVertShaderMultiview =
    IGL_TO_STRING(\n
      REQUIRE_EXTENSION(GL_EXT_multiview)
      layout (location = 0) in vec4 position_in;
      layout (location = 0) out vec4 color_out;

      layout(set = 1, binding = 1, std140) uniform PerFrame {
        vec4 colors[2];
      };

      void main() {
        gl_Position = position_in;
        color_out = colors[gl_ViewIndex];
      });

// Simple Vulkan Fragment shader for multiview
constexpr std::string_view kVulkanSimpleFragShaderMultiview =
    IGL_TO_STRING(
      layout (location = 0) in vec4 color_in;
      layout (location = 0) out vec4 out_FragColor;

      void main() {
        out_FragColor = color_in;
      });

constexpr std::string_view kVulkanSimpleComputeShader =
  IGL_TO_STRING(
        layout (local_size_x = 6, local_size_y = 1, local_size_z = 1) in;
        layout (std430, binding = 0, set = 1) readonly buffer floatsIn {
          float fIn[];
        };
        layout (std430, binding = 1, set = 1) writeonly buffer floatsOut {
          float fOut[];
        };

        void main() {
            uint id = gl_LocalInvocationIndex;

            fOut[id] = fIn[id] * 2.0f;
        });
// clang-format on
//-----------------------------------------------------------------------------
// D3D12/HLSL Shaders
//-----------------------------------------------------------------------------

// Simple D3D12 Shader with separate vertex and fragment functions
// This is used for ShaderLibrary tests where multiple entry points are in the same source
constexpr std::string_view kD3D12SimpleShader =
    IGL_TO_STRING(
      struct VSIn {
        float4 position_in : POSITION;
        float2 uv_in : TEXCOORD0;
      };

      struct VSOut {
        float4 position : SV_POSITION;
        float2 uv : TEXCOORD0;
      };

      VSOut vertexShader(VSIn input) {
        VSOut output;
        output.position = input.position_in;
        output.uv = input.uv_in;
        return output;
      }

      Texture2D inputImage : register(t0);
      SamplerState linearSampler : register(s0);

      float4 fragmentShader(VSOut input) : SV_TARGET {
        return inputImage.Sample(linearSampler, input.uv);
      }
    );

// Simple D3D12 Vertex shader (standalone)
constexpr std::string_view kD3D12SimpleVertShader =
    IGL_TO_STRING(
      struct VSIn {
        float4 position_in : POSITION;
        float2 uv_in : TEXCOORD0;
      };

      struct VSOut {
        float4 position : SV_POSITION;
        float2 uv : TEXCOORD0;
      };

      VSOut main(VSIn input) {
        VSOut output;
        output.position = input.position_in;
        output.uv = input.uv_in;
        return output;
      }
    );

// Simple D3D12 Fragment shader (standalone)
constexpr std::string_view kD3D12SimpleFragShader =
    IGL_TO_STRING(
      struct PSIn {
        float4 position : SV_POSITION;
        float2 uv : TEXCOORD0;
      };

      Texture2D inputImage : register(t0);
      SamplerState linearSampler : register(s0);

      float4 main(PSIn input) : SV_TARGET {
        return inputImage.Sample(linearSampler, input.uv);
      }
    );

// Simple D3D12 Compute shader
constexpr std::string_view kD3D12SimpleComputeShader =
    IGL_TO_STRING(
      RWStructuredBuffer<float> floatsIn : register(u0);
      RWStructuredBuffer<float> floatsOut : register(u1);

      [numthreads(6, 1, 1)]
      void doubleKernel(uint3 threadID : SV_DispatchThreadID) {
        uint id = threadID.x;
        floatsOut[id] = floatsIn[id] * 2.0;
      }
    );

// D3D12 Texture2DArray Vertex shader
constexpr std::string_view kD3D12SimpleVertShaderTexArray =
    IGL_TO_STRING(
      cbuffer VertexUniforms : register(b2) {
        int layer;
      };

      struct VSIn {
        float4 position_in : POSITION;
        float2 uv_in : TEXCOORD0;
      };

      struct VSOut {
        float4 position : SV_POSITION;
        float2 uv : TEXCOORD0;
        nointerpolation uint layerOut : TEXCOORD1;
      };

      VSOut main(VSIn input) {
        VSOut output;
        output.position = input.position_in;
        output.uv = input.uv_in;
        output.layerOut = layer;
        return output;
      }
    );

// D3D12 Texture2DArray Fragment shader
constexpr std::string_view kD3D12SimpleFragShaderTexArray =
    IGL_TO_STRING(
      Texture2DArray inputImage : register(t0);
      SamplerState inputSampler : register(s0);

      struct PSIn {
        float4 position : SV_POSITION;
        float2 uv : TEXCOORD0;
        nointerpolation uint layerIn : TEXCOORD1;
      };

      float4 main(PSIn input) : SV_TARGET {
        return inputImage.Sample(inputSampler, float3(input.uv, input.layerIn));
      }
    );

// D3D12 TextureCube Vertex shader
constexpr std::string_view kD3D12SimpleVertShaderCube =
    IGL_TO_STRING(
      cbuffer VertexUniforms : register(b1) {
        float4 view;
      };

      struct VSIn {
        float4 position_in : POSITION;
        float2 uv_in : TEXCOORD0;
      };

      struct VSOut {
        float4 position : SV_POSITION;
        float3 viewDir : TEXCOORD0;
      };

      VSOut main(VSIn input) {
        VSOut output;
        output.position = input.position_in;
        output.viewDir = view.xyz;
        return output;
      }
    );

// D3D12 TextureCube Fragment shader
constexpr std::string_view kD3D12SimpleFragShaderCube =
    IGL_TO_STRING(
      TextureCube inputImage : register(t0);
      SamplerState inputSampler : register(s0);

      struct PSIn {
        float4 position : SV_POSITION;
        float3 viewDir : TEXCOORD0;
      };

      float4 main(PSIn input) : SV_TARGET {
        return inputImage.Sample(inputSampler, input.viewDir);
      }
    );

// D3D12 Texture2DArray Vertex shader
constexpr std::string_view kD3D12SimpleVertShaderTex2dArray =
    IGL_TO_STRING(
      cbuffer VertexUniforms : register(b2) {
        int layer;
      };

      struct VSIn {
        float4 position_in : POSITION;
        float2 uv_in : TEXCOORD0;
      };

      struct VSOut {
        float4 position : SV_POSITION;
        float2 uv : TEXCOORD0;
        uint layer : TEXCOORD1;
      };

      VSOut main(VSIn input) {
        VSOut output;
        output.position = input.position_in;
        output.uv = input.uv_in;
        output.layer = uint(layer);
        return output;
      }
    );

// D3D12 Texture2DArray Fragment shader
constexpr std::string_view kD3D12SimpleFragShaderTex2dArray =
    IGL_TO_STRING(
      Texture2DArray<float4> inputImage : register(t0);
      SamplerState inputSampler : register(s0);

      struct PSIn {
        float4 position : SV_POSITION;
        float2 uv : TEXCOORD0;
        uint layer : TEXCOORD1;
      };

      float4 main(PSIn input) : SV_TARGET {
        return inputImage.Sample(inputSampler, float3(input.uv, input.layer));
      }
    );

// clang-format on
} // namespace igl::tests::data::shader
