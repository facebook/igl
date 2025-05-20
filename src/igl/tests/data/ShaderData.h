/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef> // For size_t/
#include <igl/Macros.h>
#if IGL_BACKEND_OPENGL
#include <igl/opengl/Macros.h>
#endif // IGL_BACKEND_OPENGL

namespace igl::tests::data::shader {

//-----------------------------------------------------------------------------
// Defines names of inputs and functions for the shaders in this file
//-----------------------------------------------------------------------------
const char shaderFunc[] = "main"; // For OGL and VK
const char simpleVertFunc[] = "vertexShader";
const char simpleFragFunc[] = "fragmentShader";
const char simplePos[] = "position_in";
const size_t simplePosIndex = 0;
const char simpleUv[] = "uv_in";
const size_t simpleUvIndex = 1;
const char simpleSampler[] = "inputImage";
const char simpleCubeView[] = "view";

const char simpleComputeFunc[] = "doubleKernel";
const char simpleComputeInput[] = "floatsIn";
const char simpleComputeOutput[] = "floatsOut";
const size_t simpleComputeInputIndex = 0;
const size_t simpleComputeOutputIndex = 1;

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
const char OGL_SIMPLE_VERT_SHADER[] =
    IGL_TO_STRING(LEGACY_VERSION attribute vec4 position_in; attribute vec2 uv_in; varying vec2 uv;

               void main() {
                 gl_Position = position_in;
                 gl_PointSize = 1.0; // Testing GL_POINTS draw primitive in Windows environment requires this.
                                     // See https://github.com/khronosgroup/webgl/issues/2818
                 uv = uv_in;
               });

// Simple OGL Fragment shader
const char OGL_SIMPLE_FRAG_SHADER[] =
    IGL_TO_STRING(LEGACY_VERSION PROLOG uniform sampler2D inputImage; varying vec2 uv;

               void main() {
                 gl_FragColor = texture2D(inputImage, uv);
                 });

const char OGL_SIMPLE_VERT_SHADER_ES3[] =
    IGL_TO_STRING(VERSION(300 es)
    in vec4 position_in; in vec2 uv_in; out vec2 uv;

               void main() {
                 gl_Position = position_in;
                 gl_PointSize = 1.0; // Testing GL_POINTS draw primitive in Windows environment requires this.
                                     // See https://github.com/khronosgroup/webgl/issues/2818
                 uv = uv_in;
               });

const char OGL_SIMPLE_FRAG_SHADER_ES3[] =
    IGL_TO_STRING(VERSION(300 es)
    PROLOG uniform sampler2D inputImage; in vec2 uv; out vec4 fragColor;

               void main() {
                 fragColor = texture(inputImage, uv);
                 });

const char OGL_SIMPLE_VERT_SHADER_MULTIVIEW_ES3[] =
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

const char OGL_SIMPLE_FRAG_SHADER_MULTIVIEW_ES3[] =
    IGL_TO_STRING(VERSION(300 es)
    PROLOG
    in vec4 color;
    out vec4 fragColor;

    void main() {
      fragColor = color;
    });

const char OGL_SIMPLE_VERT_SHADER_TEXARRAY[] =
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

const char OGL_SIMPLE_FRAG_SHADER_TEXARRAY[] =
IGL_TO_STRING(VERSION(150)
    PROLOG
    in vec2 uv;
    flat in uint layer_out;
    uniform sampler2DArray inputImage;

    out vec4 fragColor;

    void main() {
      fragColor = texture(inputImage, vec3(uv, layer_out));
    });

const char OGL_SIMPLE_VERT_SHADER_TEXARRAY_EXT[] =
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

const char OGL_SIMPLE_FRAG_SHADER_TEXARRAY_EXT[] =
IGL_TO_STRING(VERSION(110)
    REQUIRE_EXTENSION(GL_EXT_texture_array)
    PROLOG

    varying vec2 uv;
    varying float layer_out;
    uniform sampler2DArray inputImage;

    void main() {
      gl_FragColor = texture2DArray(inputImage, vec3(uv, layer_out));
    });

const char OGL_SIMPLE_VERT_SHADER_TEXARRAY_ES3[] =
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

const char OGL_SIMPLE_FRAG_SHADER_TEXARRAY_ES3[] =
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
const char OGL_SIMPLE_VERT_SHADER_CUBE[] =
    IGL_TO_STRING(LEGACY_VERSION attribute vec4 position_in; uniform vec4 view; varying vec3 uv;

               void main() {
                 gl_Position = position_in;
                 uv = view.xyz;
               });

// Simple OGL Fragment shader
const char OGL_SIMPLE_FRAG_SHADER_CUBE[] =
    IGL_TO_STRING(LEGACY_VERSION PROLOG uniform samplerCube inputImage; varying vec3 uv;

               void main() { gl_FragColor = textureCube(inputImage, uv); });

// Simple shader which multiplies each float value in the input buffer by 2 and writes the result to the output buffer
const char OGL_SIMPLE_COMPUTE_SHADER[] =
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

const char OGL_SIMPLE_VERT_SHADER_UNIFORM_BLOCKS[] =
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

const char OGL_SIMPLE_FRAG_SHADER_UNIFORM_BLOCKS[] =
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

const char MTL_SIMPLE_SHADER_FLOAT[] = MTL_SIMPLE_SHADER_DEF(float, r);
const char MTL_SIMPLE_SHADER_FLOAT2[] = MTL_SIMPLE_SHADER_DEF(float2, rg);
const char MTL_SIMPLE_SHADER_FLOAT3[] = MTL_SIMPLE_SHADER_DEF(float3, rgb);
const char MTL_SIMPLE_SHADER_FLOAT4[] = MTL_SIMPLE_SHADER_DEF(float4, rgba);
const char MTL_SIMPLE_SHADER_USHORT[] = MTL_SIMPLE_SHADER_DEF(ushort, r);
const char MTL_SIMPLE_SHADER_USHORT2[] = MTL_SIMPLE_SHADER_DEF(ushort2, rg);
const char MTL_SIMPLE_SHADER_USHORT4[] = MTL_SIMPLE_SHADER_DEF(ushort4, rgba);
const char MTL_SIMPLE_SHADER_UINT4[] = MTL_SIMPLE_SHADER_DEF(uint4, rgba);
const char MTL_SIMPLE_SHADER_UINT[] = MTL_SIMPLE_SHADER_DEF(uint4, r);
const char MTL_SIMPLE_SHADER[] = MTL_SIMPLE_SHADER_DEF(float4, rgba);

// Simple Metal Shader for 1D Texture
// The vertext function expects vertex and sampler positions to be bound to
// separate buffers
const char MTL_SIMPLE_SHADER_1DTEX[] =
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
const char MTL_SIMPLE_SHADER_CUBE[] =
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
const char MTL_SIMPLE_COMPUTE_SHADER[] =
  IGL_TO_STRING(using namespace metal;

      kernel void doubleKernel(
          device float* floatsIn [[buffer(0)]],
          device float* floatsOut [[buffer(1)]],
          uint2 gid [[thread_position_in_grid]]) {
        floatsOut[gid.x] = floatsIn[gid.x] * 2.0;
      });

const char MTL_SIMPLE_SHADER_TXT_1D_ARRAY[] =
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


const char MTL_SIMPLE_SHADER_TXT_2D_ARRAY[] =
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
const char VULKAN_SIMPLE_VERT_SHADER[] =
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

const char VULKAN_SIMPLE_FRAG_SHADER[] = VULKAN_SIMPLE_FRAG_SHADER_DEF(vec4, rgba);

const char VULKAN_SIMPLE_FRAG_SHADER_FLOAT[] = VULKAN_SIMPLE_FRAG_SHADER_DEF(float, r);
const char VULKAN_SIMPLE_FRAG_SHADER_FLOAT2[] = VULKAN_SIMPLE_FRAG_SHADER_DEF(vec2, rg);
const char VULKAN_SIMPLE_FRAG_SHADER_FLOAT3[] = VULKAN_SIMPLE_FRAG_SHADER_DEF(vec3, rgb);
const char VULKAN_SIMPLE_FRAG_SHADER_FLOAT4[] = VULKAN_SIMPLE_FRAG_SHADER_DEF(vec4, rgba);
const char VULKAN_SIMPLE_FRAG_SHADER_UINT[] = VULKAN_SIMPLE_FRAG_SHADER_DEF(uint, r);
const char VULKAN_SIMPLE_FRAG_SHADER_UINT2[] = VULKAN_SIMPLE_FRAG_SHADER_DEF(uvec2, rg);
const char VULKAN_SIMPLE_FRAG_SHADER_UINT4[] = VULKAN_SIMPLE_FRAG_SHADER_DEF(uvec4, rgba);

const char VULKAN_PUSH_CONSTANT_VERT_SHADER[] =
    IGL_TO_STRING(
      layout (location=0) in vec4 position_in;
      layout (location=1) in vec2 uv_in;
      layout (location=0) out vec2 uv;

      void main() {
        gl_Position = position_in;
        gl_PointSize = 1.0;
        uv = uv_in;
      });

const char VULKAN_PUSH_CONSTANT_FRAG_SHADER[] =
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

const char VULKAN_SIMPLE_VERT_SHADER_TEX_2DARRAY[] =
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

const char VULKAN_SIMPLE_FRAG_SHADER_TEX_1DARRAY[] =
IGL_TO_STRING(
    layout(location = 0) in vec2 uv;
    layout(location = 1) in flat uint layer;
    layout(location = 0) out vec4 out_FragColor;

    layout (set = 0, binding = 0) uniform sampler1DArray uTex;

    void main() {
        out_FragColor = texture(uTex, vec2(uv.x, layer));
    });

const char VULKAN_SIMPLE_FRAG_SHADER_TEX_2DARRAY[] =
IGL_TO_STRING(
    layout(location = 0) in vec2 uv;
    layout(location = 1) in flat uint layer;
    layout(location = 0) out vec4 out_FragColor;

    layout (set = 0, binding = 0) uniform sampler2DArray uTex;

    void main() {
      out_FragColor = texture(uTex, vec3(uv.xy, layer));
    });

const char VULKAN_SIMPLE_VERT_SHADER_CUBE[] =
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

const char VULKAN_SIMPLE_FRAG_SHADER_CUBE[] =
    IGL_TO_STRING(
               layout(location = 0) in vec3 view;
               layout(location = 0) out vec4 out_FragColor;

               layout (set = 0, binding = 0) uniform samplerCube uTex;

               void main() {
                   out_FragColor = texture(uTex, view);
               });

// Simple Vulkan Vertex shader for multiview
const char VULKAN_SIMPLE_VERT_SHADER_MULTIVIEW[] =
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
const char VULKAN_SIMPLE_FRAG_SHADER_MULTIVIEW[] =
    IGL_TO_STRING(
      layout (location = 0) in vec4 color_in;
      layout (location = 0) out vec4 out_FragColor;

      void main() {
        out_FragColor = color_in;
      });

const char VULKAN_SIMPLE_COMPUTE_SHADER[] =
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
} // namespace igl::tests::data::shader
