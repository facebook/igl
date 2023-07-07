#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;

struct VertexUniformBlock {
  float4x4 mvpMatrix;
  float scaleZ;
};

struct VertexIn {
  float3 position [[attribute(0)]];
  float3 uvw [[attribute(1)]];
};

struct VertexOut {
  float4 position [[position]];
  float3 uvw;
};

vertex VertexOut vertexShader(VertexIn in [[stage_in]],
       constant VertexUniformBlock &vUniform[[buffer(1)]]) {
  VertexOut out;
  out.position = vUniform.mvpMatrix * float4(in.position, 1.0);
  out.uvw = in.uvw;
  out.uvw = float3(
  out.uvw.x, out.uvw.y, (out.uvw.z - 0.5f)*vUniform.scaleZ + 0.5f);
  return out;
 }

fragment float4 fragmentShader(
      VertexOut in[[stage_in]],
      texture3d<float> diffuseTex [[texture(0)]],
      sampler linearSampler [[sampler(0)]]) {
  constexpr sampler s(s_address::clamp_to_edge,
                      t_address::clamp_to_edge,
                      min_filter::linear,
                      mag_filter::linear);
  float4 tex = diffuseTex.sample(s, in.uvw);
  return tex;
}
