/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */
#pragma once
#include <string>

namespace iglu::nanovg {

static std::string opengl_410_vertex_shader_header = R"(#version 410
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 tcoord;

out vec2 fpos;
out vec2 ftcoord;

layout(std140) uniform VertexUniformBlock {
 vec2 viewSize;
}uniforms;
)";

static std::string opengl_460_vertex_shader_header = R"(#version 460
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 tcoord;

layout (location=0) out vec2 fpos;
layout (location=1) out vec2 ftcoord;

layout(set = 1, binding = 1, std140) uniform VertexUniformBlock {
 vec2 viewSize;
}uniforms;
)";

static std::string opengl_vertex_shader_body = R"(
void main() {
  ftcoord = tcoord;
  fpos = pos;
  gl_Position = vec4(2.0 * pos.x / uniforms.viewSize.x - 1.0,
                     1.0 - 2.0 * pos.y / uniforms.viewSize.y,
                   0, 1);
}
)";

static std::string opengl_410_fragment_shader_header = R"(#version 410
precision highp int; 
precision highp float;

in vec2 fpos;
in vec2 ftcoord;

layout (location=0) out vec4 FragColor;

uniform lowp sampler2D textureUnit;

layout(std140) uniform FragmentUniformBlock {
  mat3 scissorMat;
  mat3 paintMat;
  vec4 innerCol;
  vec4 outerCol;
  vec2 scissorExt;
  vec2 scissorScale;
  vec2 extent;
  float radius;
  float feather;
  float strokeMult;
  float strokeThr;
  int texType;
  int type;
}uniforms;

)";

static std::string opengl_460_fragment_shader_header = R"(#version 460
precision highp int; 
precision highp float;

layout (location=0) in vec2 fpos;
layout (location=1) in vec2 ftcoord;

layout (location=0) out vec4 FragColor;

layout(set = 0, binding = 0)  uniform lowp sampler2D textureUnit;

layout(set = 1, binding = 2, std140) uniform FragmentUniformBlock {
  mat3 scissorMat;
  mat3 paintMat;
  vec4 innerCol;
  vec4 outerCol;
  vec2 scissorExt;
  vec2 scissorScale;
  vec2 extent;
  float radius;
  float feather;
  float strokeMult;
  float strokeThr;
  int texType;
  int type;
}uniforms;
)";

static std::string opengl_fragment_shader_body = R"(
float scissorMask(vec2 p) {
  vec2 sc = (abs((uniforms.scissorMat * vec3(p, 1.0f)).xy)
                  - uniforms.scissorExt)  * uniforms.scissorScale;
  sc = clamp(vec2(0.5f) - sc, 0.0, 1.0);
  return sc.x * sc.y;
}

float sdroundrect(vec2 pt) {
  vec2 ext2 = uniforms.extent - vec2(uniforms.radius);
  vec2 d = abs(pt) - ext2;
  return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - uniforms.radius;
}

float strokeMask(vec2 ftcoord) {
  return min(1.0, (1.0 - abs(ftcoord.x * 2.0 - 1.0)) * uniforms.strokeMult) * min(1.0, ftcoord.y);
}

// Fragment function (No AA)
vec4 main2() {
  float scissor = scissorMask(fpos);
  if (scissor == 0.0)
    return vec4(0);

  if (uniforms.type == 0) {  // MNVG_SHADER_FILLGRAD
    vec2 pt = (uniforms.paintMat * vec3(fpos, 1.0)).xy;
    float d = clamp((uniforms.feather * 0.5 + sdroundrect(pt))
                       / uniforms.feather, 0.0, 1.0);
    vec4 color = mix(uniforms.innerCol, uniforms.outerCol, d);
    return color * scissor;
  } else if (uniforms.type == 1) {  // MNVG_SHADER_FILLIMG
    vec2 pt = (uniforms.paintMat * vec3(fpos, 1.0)).xy / uniforms.extent;
    vec4 color = texture(textureUnit, pt);
    if (uniforms.texType == 1)
      color = vec4(color.xyz * color.w, color.w);
    else if (uniforms.texType == 2)
      color = vec4(color.x);
    color *= scissor;
    return color * uniforms.innerCol;
  } else {  // MNVG_SHADER_IMG
    vec4 color = texture(textureUnit, ftcoord);
    if (uniforms.texType == 1)
      color = vec4(color.xyz * color.w, color.w);
    else if (uniforms.texType == 2)
      color = vec4(color.x);
    color *= scissor;
    return color * uniforms.innerCol;
  }
}

void main(){
    FragColor = main2();
}

)";


} // namespace iglu::nanovg
