/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Post effects for chapter 3 of SlangShowcaseSession, composed at compile time: PostChain is a
// variadic generic over IPostEffect, and each entry point instantiates it with a different list of
// effects, so every chain compiles to straight-line code with no branches between effects.
#include "igl_abi.slangh"

struct PostUniforms {
  float4 params; // x: seconds, y: 1 to gamma-encode for a UNorm surface
};

IGL_UNIFORM_BUFFER(PostUniforms, post, 1);
IGL_TEXTURE2D(scene, 0);

struct PostContext {
  float2 uv;
};

interface IPostEffect {
  float3 apply(float3 color, PostContext context);
};

struct PostChain<each E : IPostEffect> {
  float3 run(float3 color, PostContext context, expand each E effects) {
    float3 c = color;
    expand c = (each effects).apply(c, context);
    return c;
  }
};

struct Exposure : IPostEffect {
  float stops;
  __init(float s) {
    stops = s;
  }
  float3 apply(float3 color, PostContext context) {
    return color * exp2(stops);
  }
};

// Samples the scene again, so it goes first.
struct ChromaticAberration : IPostEffect {
  float strength;
  __init(float s) {
    strength = s;
  }
  float3 apply(float3 color, PostContext context) {
    const float2 offset = (context.uv - 0.5) * strength;
    return float3(IGL_SAMPLE2D_LEVEL(scene, context.uv + offset, 0.0).r,
                  color.g,
                  IGL_SAMPLE2D_LEVEL(scene, context.uv - offset, 0.0).b);
  }
};

// Narkowicz's ACES fit.
struct ACES : IPostEffect {
  float3 apply(float3 color, PostContext context) {
    return saturate(color * (2.51 * color + 0.03) / (color * (2.43 * color + 0.59) + 0.14));
  }
};

struct Vignette : IPostEffect {
  float strength;
  __init(float s) {
    strength = s;
  }
  float3 apply(float3 color, PostContext context) {
    const float2 d = context.uv - 0.5;
    return color * saturate(1.0 - strength * dot(d, d));
  }
};

struct FilmGrain : IPostEffect {
  float amount;
  __init(float a) {
    amount = a;
  }
  float3 apply(float3 color, PostContext context) {
    const float2 p = floor(context.uv * 512.0) + floor(post.params.x * 24.0) * float2(17.0, 59.0);
    const float noise = frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453) - 0.5;
    return saturate(color + amount * noise);
  }
};

struct Monochrome : IPostEffect {
  float3 apply(float3 color, PostContext context) {
    return float3(dot(color, float3(0.2126, 0.7152, 0.0722)));
  }
};

struct Tint : IPostEffect {
  float3 tint;
  __init(float3 t) {
    tint = t;
  }
  float3 apply(float3 color, PostContext context) {
    return color * tint;
  }
};

struct Posterize : IPostEffect {
  float levels;
  __init(float l) {
    levels = l;
  }
  float3 apply(float3 color, PostContext context) {
    return floor(color * levels + 0.5) / levels;
  }
};

// sRGB surfaces encode gamma themselves.
struct Encode : IPostEffect {
  float3 apply(float3 color, PostContext context) {
    return post.params.y > 0.5 ? pow(color, 1.0 / 2.2) : color;
  }
};

struct PostVarying {
  float4 position : SV_Position;
  [[vk::location(0)]] float2 uv : TEXCOORD0;
};

[shader("vertex")]
PostVarying postVertex(uint vertexId: IGL_SV_VERTEX_ID) {
  const float2 uv = float2(float((vertexId << 1u) & 2u), float(vertexId & 2u));
  PostVarying output;
  output.position = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
  output.uv = uv;
  return output;
}

float3 sceneColor(PostVarying input) {
  return IGL_SAMPLE2D_LEVEL(scene, input.uv, 0.0).rgb;
}

[shader("fragment")]
float4 postNeutral(PostVarying input) : SV_Target0 {
  const PostContext context = {input.uv};
  PostChain<Exposure, ACES, Encode> chain;
  return float4(chain.run(sceneColor(input), context, Exposure(0.0), ACES(), Encode()), 1.0);
}

[shader("fragment")]
float4 postFilm(PostVarying input) : SV_Target0 {
  const PostContext context = {input.uv};
  PostChain<ChromaticAberration, Exposure, ACES, Vignette, FilmGrain, Encode> chain;
  return float4(chain.run(sceneColor(input),
                          context,
                          ChromaticAberration(0.012),
                          Exposure(0.3),
                          ACES(),
                          Vignette(1.6),
                          FilmGrain(0.06),
                          Encode()),
                1.0);
}

[shader("fragment")]
float4 postNoir(PostVarying input) : SV_Target0 {
  const PostContext context = {input.uv};
  // Posterize the grey before tinting it, or each channel steps at a different level.
  PostChain<Exposure, ACES, Monochrome, Posterize, Tint, Vignette, Encode> chain;
  return float4(chain.run(sceneColor(input),
                          context,
                          Exposure(0.5),
                          ACES(),
                          Monochrome(),
                          Posterize(6.0),
                          Tint(float3(1.0, 0.93, 0.8)),
                          Vignette(2.2),
                          Encode()),
                1.0);
}
