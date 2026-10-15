/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Physically based materials for SlangShowcaseSession, in the spirit of the glTF PBR extensions.
// Every lobe is generic over the float type, so the same code shades in float and in half, and
// differentiable in the material parameters, so the same code renders and fits parameters to
// images. Material instances conform to IMaterial, so they can be specialized statically or
// dispatched dynamically from a buffer record.
module ShowcaseMaterials;

static const float kPi = 3.14159265;

// What a material sees at a shaded point: normalized world-space vectors.
public struct SurfaceSample {
  public float3 normal;
  public float3 tangent;
  public float3 bitangent;
  public float3 view;
};

// One material's parameters as stored; each model decides what the fields mean.
public struct MaterialData : IDifferentiable {
  public float4 base; // base color, roughness
  public float4 extra;
};

public struct Lighting {
  public float4 lights[3]; // position, intensity
  public float4 lightColors[3];
  public float4 sky;
  public float4 ground;
};

// Reflectance as a function of the material parameters.
public interface IMaterialModel {
  // Reflected radiance per unit of light arriving from direction l: BRDF times cosine.
  [Differentiable]
  public static float3 lightResponse(MaterialData d, SurfaceSample s, no_diff float3 l);
  [Differentiable]
  public static float3 ambient(MaterialData d, SurfaceSample s, no_diff float3 sky, no_diff float3 ground);
};

// Radiance leaving a point toward the viewer under three point lights and a sky.
[Differentiable]
public float3 radiance<M : IMaterialModel>(MaterialData d,
                                           SurfaceSample s,
                                           no_diff float3 position,
                                           Lighting lighting) {
  float3 color = M.ambient(d, s, lighting.sky.rgb, lighting.ground.rgb);
  [ForceUnroll]
  for (int i = 0; i < 3; ++i) {
    const float3 toLight = lighting.lights[i].xyz - position;
    const float distance2 = dot(toLight, toLight);
    color += M.lightResponse(d, s, toLight * rsqrt(distance2)) * lighting.lightColors[i].rgb *
             (lighting.lights[i].w / distance2);
  }
  return color;
}

[anyValueSize(32)]
public interface IMaterial {
  public float3 shade(SurfaceSample s, float3 position, Lighting lighting);
};

// A model with its parameters.
public struct Material<M : IMaterialModel> : IMaterial {
  public MaterialData data;

  public __init(MaterialData d) {
    data = d;
  }
  public float3 shade(SurfaceSample s, float3 position, Lighting lighting) {
    return radiance<M>(data, s, position, lighting);
  }
};

// (alpha / d)^2 rather than alpha^2 / d^2, with d built from sin^2 rather than 1 - cos^2, so narrow
// highlights stay finite in half precision.
[Differentiable]
T distributionGGX<T : __BuiltinFloatingPointType>(T nDotH, T sinH2, T alpha) {
  const T k = alpha / (sinH2 + nDotH * nDotH * alpha * alpha);
  return k * k / T(kPi);
}

// Height-correlated Smith visibility, V = G / (4 n.l n.v).
[Differentiable]
T visibilitySmithGGX<T : __BuiltinFloatingPointType>(T nDotV, T nDotL, T alpha) {
  const T a2 = alpha * alpha;
  const T v = nDotL * sqrt(nDotV * nDotV * (T(1.0) - a2) + a2);
  const T l = nDotV * sqrt(nDotL * nDotL * (T(1.0) - a2) + a2);
  return T(0.5) / max(v + l, T(1e-4));
}

[Differentiable]
vector<T, 3> fresnelSchlick<T : __BuiltinFloatingPointType>(vector<T, 3> f0, T vDotH) {
  const T k = T(1.0) - vDotH;
  const T k2 = k * k;
  return f0 + (vector<T, 3>(T(1.0)) - f0) * (k2 * k2 * k);
}

// Estevez and Kulla's "Charlie" sheen distribution.
[Differentiable]
T distributionCharlie<T : __BuiltinFloatingPointType>(T sinH2, T roughness) {
  const T alpha = max(roughness * roughness, T(0.02));
  const T sin2 = max(sinH2, T(1e-4));
  return (T(2.0) + T(1.0) / alpha) * pow(sin2, T(0.5) / alpha) / T(2.0 * kPi);
}

[Differentiable]
T visibilityNeubelt<T : __BuiltinFloatingPointType>(T nDotV, T nDotL) {
  return T(1.0) / (T(4.0) * max(nDotL + nDotV - nDotL * nDotV, T(1e-4)));
}

[Differentiable]
T distributionGGXAnisotropic<T : __BuiltinFloatingPointType>(T tDotH, T bDotH, T nDotH, T at, T ab) {
  const T a2 = at * ab;
  const vector<T, 3> m = vector<T, 3>(ab * tDotH, at * bDotH, a2 * nDotH);
  const T k = a2 / dot(m, m);
  return a2 * k * k / T(kPi);
}

[Differentiable]
T visibilitySmithGGXAnisotropic<T : __BuiltinFloatingPointType>(T tDotV,
                                                                  T bDotV,
                                                                  T nDotV,
                                                                  T tDotL,
                                                                  T bDotL,
                                                                  T nDotL,
                                                                  T at,
                                                                  T ab) {
  const T v = nDotL * length(vector<T, 3>(at * tDotV, ab * bDotV, nDotV));
  const T l = nDotV * length(vector<T, 3>(at * tDotL, ab * bDotL, nDotL));
  return T(0.5) / max(v + l, T(1e-4));
}

// The dot products every lobe needs, in the material's float type. sinH2 is computed in float:
// 1 - nDotH^2 cancels to zero in half precision near a highlight.
struct Angles<T : __BuiltinFloatingPointType> {
  T nDotL;
  T nDotV;
  T nDotH;
  T sinH2;
  T vDotH;

  __init(SurfaceSample s, float3 l) {
    const float3 h = normalize(s.view + l);
    const float cosH = saturate(dot(s.normal, h));
    nDotL = T(saturate(dot(s.normal, l)));
    nDotV = T(max(dot(s.normal, s.view), 1e-4));
    nDotH = T(cosH);
    sinH2 = T(1.0 - cosH * cosH);
    vDotH = T(saturate(dot(s.view, h)));
  }
};

// The metallic-roughness base layer every material builds on.
[Differentiable]
vector<T, 3> baseLayer<T : __BuiltinFloatingPointType>(MaterialData d, Angles<T> a) {
  const vector<T, 3> baseColor = vector<T, 3>(d.base.rgb);
  const T alpha = max(T(d.base.a * d.base.a), T(1e-3));
  const T metallic = T(d.extra.x);
  const vector<T, 3> f0 = lerp(vector<T, 3>(T(0.04)), baseColor, metallic);
  const vector<T, 3> f = fresnelSchlick(f0, a.vDotH);
  const vector<T, 3> diffuse = (vector<T, 3>(T(1.0)) - f) * baseColor * ((T(1.0) - metallic) / T(kPi));
  const T specular =
      distributionGGX(a.nDotH, a.sinH2, alpha) * visibilitySmithGGX(a.nDotV, a.nDotL, alpha);
  return (diffuse + f * specular) * a.nDotL;
}

[Differentiable]
float3 baseAmbient(MaterialData d, SurfaceSample s, no_diff float3 sky, no_diff float3 ground) {
  const float3 environment = lerp(ground, sky, s.normal.y * 0.5 + 0.5);
  const float3 f0 = lerp(float3(0.04), d.base.rgb, d.extra.x);
  return environment * lerp(d.base.rgb * (1.0 - d.extra.x), f0, 0.5 * (1.0 - d.base.a));
}

// extra.x: metallic.
public struct MetalRough<T : __BuiltinFloatingPointType> : IMaterialModel {
  [Differentiable]
  public static float3 lightResponse(MaterialData d, SurfaceSample s, no_diff float3 l) {
    return float3(baseLayer(d, Angles<T>(s, l)));
  }
  [Differentiable]
  public static float3 ambient(MaterialData d, SurfaceSample s, no_diff float3 sky, no_diff float3 ground) {
    return baseAmbient(d, s, sky, ground);
  }
};

// extra: metallic, clearcoat weight, clearcoat roughness.
public struct Clearcoat<T : __BuiltinFloatingPointType> : IMaterialModel {
  [Differentiable]
  public static float3 lightResponse(MaterialData d, SurfaceSample s, no_diff float3 l) {
    const Angles<T> a = Angles<T>(s, l);
    const vector<T, 3> base = baseLayer(d, a);
    const T alpha = max(T(d.extra.z * d.extra.z), T(1e-3));
    const T coat = T(d.extra.y) * fresnelSchlick(vector<T, 3>(T(0.04)), a.vDotH).x;
    const T specular =
        distributionGGX(a.nDotH, a.sinH2, alpha) * visibilitySmithGGX(a.nDotV, a.nDotL, alpha);
    return float3(base * (T(1.0) - coat) + vector<T, 3>(coat * specular * a.nDotL));
  }
  [Differentiable]
  public static float3 ambient(MaterialData d, SurfaceSample s, no_diff float3 sky, no_diff float3 ground) {
    return baseAmbient(d, s, sky, ground) + 0.04 * d.extra.y * sky;
  }
};

// extra: sheen color, sheen roughness; a dielectric base.
public struct Sheen<T : __BuiltinFloatingPointType> : IMaterialModel {
  [Differentiable]
  public static float3 lightResponse(MaterialData d, SurfaceSample s, no_diff float3 l) {
    const Angles<T> a = Angles<T>(s, l);
    MaterialData dielectric = d;
    dielectric.extra.x = 0.0;
    const vector<T, 3> base = baseLayer(dielectric, a);
    const vector<T, 3> sheenColor = vector<T, 3>(d.extra.rgb);
    const T sheen = distributionCharlie(a.sinH2, T(d.extra.w)) * visibilityNeubelt(a.nDotV, a.nDotL);
    const T scale = T(1.0) - max(sheenColor.x, max(sheenColor.y, sheenColor.z)) * T(0.157);
    return float3(base * scale + sheenColor * (sheen * a.nDotL));
  }
  [Differentiable]
  public static float3 ambient(MaterialData d, SurfaceSample s, no_diff float3 sky, no_diff float3 ground) {
    MaterialData dielectric = d;
    dielectric.extra.x = 0.0;
    const float rim = pow(1.0 - saturate(dot(s.normal, s.view)), 4.0);
    return baseAmbient(dielectric, s, sky, ground) + d.extra.rgb * rim * sky;
  }
};

// extra: metallic, anisotropy (stretches highlights along the tangent).
public struct Anisotropic<T : __BuiltinFloatingPointType> : IMaterialModel {
  [Differentiable]
  public static float3 lightResponse(MaterialData d, SurfaceSample s, no_diff float3 l) {
    const Angles<T> a = Angles<T>(s, l);
    const float3 h = normalize(s.view + l);
    const T alpha = max(T(d.base.a * d.base.a), T(1e-3));
    const T anisotropy = T(d.extra.y);
    const T at = lerp(alpha, T(1.0), anisotropy * anisotropy);
    const T ab = alpha;
    const vector<T, 3> baseColor = vector<T, 3>(d.base.rgb);
    const vector<T, 3> f0 = lerp(vector<T, 3>(T(0.04)), baseColor, T(d.extra.x));
    const vector<T, 3> f = fresnelSchlick(f0, a.vDotH);
    const T distribution = distributionGGXAnisotropic(
        T(dot(s.tangent, h)), T(dot(s.bitangent, h)), a.nDotH, at, ab);
    const T visibility = visibilitySmithGGXAnisotropic(T(dot(s.tangent, s.view)),
                                                       T(dot(s.bitangent, s.view)),
                                                       a.nDotV,
                                                       T(dot(s.tangent, l)),
                                                       T(dot(s.bitangent, l)),
                                                       a.nDotL,
                                                       at,
                                                       ab);
    const vector<T, 3> diffuse = (vector<T, 3>(T(1.0)) - f) * baseColor * ((T(1.0) - T(d.extra.x)) / T(kPi));
    return float3((diffuse + f * (distribution * visibility)) * a.nDotL);
  }
  [Differentiable]
  public static float3 ambient(MaterialData d, SurfaceSample s, no_diff float3 sky, no_diff float3 ground) {
    return baseAmbient(d, s, sky, ground);
  }
};

// The conformances dynamic dispatch can create, by type id.
public typealias MetalRoughMaterial = Material<MetalRough<float>>;
public typealias ClearcoatMaterial = Material<Clearcoat<float>>;
public typealias SheenMaterial = Material<Sheen<float>>;
public typealias AnisotropicMaterial = Material<Anisotropic<float>>;
