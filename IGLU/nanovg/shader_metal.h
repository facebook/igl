#pragma once
#include <string>

namespace igl::nanovg{

static std::string metal_shader = R"(
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

typedef struct {
  float2 pos [[attribute(0)]];
  float2 tcoord [[attribute(1)]];
} Vertex;

typedef struct {
  float4 pos  [[position]];
  float2 fpos;
  float2 ftcoord;
} RasterizerData;

typedef struct  {
  float3x3 scissorMat;
  float3x3 paintMat;
  float4 innerCol;
  float4 outerCol;
  float2 scissorExt;
  float2 scissorScale;
  float2 extent;
  float radius;
  float feather;
  float strokeMult;
  float strokeThr;
  int texType;
  int type;
} Uniforms;

float scissorMask(constant Uniforms& uniforms, float2 p);
float sdroundrect(constant Uniforms& uniforms, float2 pt);
float strokeMask(constant Uniforms& uniforms, float2 ftcoord);

float scissorMask(constant Uniforms& uniforms, float2 p) {
  float2 sc = (abs((uniforms.scissorMat * float3(p, 1.0f)).xy)
                  - uniforms.scissorExt) \
              * uniforms.scissorScale;
  sc = saturate(float2(0.5f) - sc);
  return sc.x * sc.y;
}

float sdroundrect(constant Uniforms& uniforms, float2 pt) {
  float2 ext2 = uniforms.extent - float2(uniforms.radius);
  float2 d = abs(pt) - ext2;
  return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - uniforms.radius;
}

float strokeMask(constant Uniforms& uniforms, float2 ftcoord) {
  return min(1.0, (1.0 - abs(ftcoord.x * 2.0 - 1.0)) * uniforms.strokeMult) \
         * min(1.0, ftcoord.y);
}

// Vertex Function
vertex RasterizerData vertexShader(Vertex vert [[stage_in]],
                                   constant float2& viewSize [[buffer(1)]]) {
  RasterizerData out;
  out.ftcoord = vert.tcoord;
  out.fpos = vert.pos;
  out.pos = float4(2.0 * vert.pos.x / viewSize.x - 1.0,
                   1.0 - 2.0 * vert.pos.y / viewSize.y,
                   0, 1);
  return out;
}

// Fragment function (No AA)
fragment float4 fragmentShader(RasterizerData in [[stage_in]],
                               constant Uniforms& uniforms [[buffer(2)]],
                               texture2d<float> texture [[texture(0)]],
                               sampler sampler [[sampler(0)]]) {
  float scissor = scissorMask(uniforms, in.fpos);
  if (scissor == 0)
    return float4(0);

  if (uniforms.type == 0) {  // MNVG_SHADER_FILLGRAD
    float2 pt = (uniforms.paintMat * float3(in.fpos, 1.0)).xy;
    float d = saturate((uniforms.feather * 0.5 + sdroundrect(uniforms, pt))
                       / uniforms.feather);
    float4 color = mix(uniforms.innerCol, uniforms.outerCol, d);
    return color * scissor;
  } else if (uniforms.type == 1) {  // MNVG_SHADER_FILLIMG
    float2 pt = (uniforms.paintMat * float3(in.fpos, 1.0)).xy / uniforms.extent;
    float4 color = texture.sample(sampler, pt);
    if (uniforms.texType == 1)
      color = float4(color.xyz * color.w, color.w);
    else if (uniforms.texType == 2)
      color = float4(color.x);
    color *= scissor;
    return color * uniforms.innerCol;
  } else {  // MNVG_SHADER_IMG
    float4 color = texture.sample(sampler, in.ftcoord);
    if (uniforms.texType == 1)
      color = float4(color.xyz * color.w, color.w);
    else if (uniforms.texType == 2)
      color = float4(color.x);
    color *= scissor;
    return color * uniforms.innerCol;
  }
}

// Fragment function (AA)
fragment float4 fragmentShaderAA(RasterizerData in [[stage_in]],
                                 constant Uniforms& uniforms [[buffer(2)]],
                                 texture2d<float> texture [[texture(0)]],
                                 sampler sampler [[sampler(0)]]) {
  float scissor = scissorMask(uniforms, in.fpos);
  if (scissor == 0)
    return float4(0);

  if (uniforms.type == 2) {  // MNVG_SHADER_IMG
    float4 color = texture.sample(sampler, in.ftcoord);
    if (uniforms.texType == 1)
      color = float4(color.xyz * color.w, color.w);
    else if (uniforms.texType == 2)
      color = float4(color.x);
    color *= scissor;
    return color * uniforms.innerCol;
  }

  float strokeAlpha = strokeMask(uniforms, in.ftcoord);
  if (strokeAlpha < uniforms.strokeThr) {
    return float4(0);
  }

  if (uniforms.type == 0) {  // MNVG_SHADER_FILLGRAD
    float2 pt = (uniforms.paintMat * float3(in.fpos, 1.0)).xy;
    float d = saturate((uniforms.feather * 0.5 + sdroundrect(uniforms, pt))
                        / uniforms.feather);
    float4 color = mix(uniforms.innerCol, uniforms.outerCol, d);
    color *= scissor;
    color *= strokeAlpha;
    return color;
  } else {  // MNVG_SHADER_FILLIMG
    float2 pt = (uniforms.paintMat * float3(in.fpos, 1.0)).xy / uniforms.extent;
    float4 color = texture.sample(sampler, pt);
    if (uniforms.texType == 1)
      color = float4(color.xyz * color.w, color.w);
    else if (uniforms.texType == 2)
      color = float4(color.x);
    color *= scissor;
    color *= strokeAlpha;
    return color * uniforms.innerCol;
  }
}
)";

}
