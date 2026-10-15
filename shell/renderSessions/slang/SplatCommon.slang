/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// The model SlangSplatsSession fits to an image: anisotropic 2D splats with a color and an
// opacity, blended as C = sum(w c) / (sum(w) + epsilon). Rendering and training share every
// function here, so the gradients are those of exactly what is drawn.
module SplatCommon;

public static const uint kImageSize = 256;
public static const uint kSplatCount = 2048;
public static const uint kTileSize = 8;
public static const uint kTilesPerRow = kImageSize / kTileSize;
public static const uint kTileCapacity = 255;
// A tile holds its splat count followed by up to kTileCapacity splat indices.
public static const uint kTileStride = kTileCapacity + 1;
// Splats are cut off at this many standard deviations.
public static const float kExtent = 3.0;
public static const float kWeightEpsilon = 1e-4;

// One splat in a storage or vertex buffer.
public struct SplatRecord {
  public float4 shape; // center (pixels), log standard deviation (pixels)
  public float4 extra; // rotation (radians), opacity logit
  public float4 color; // linear RGB
};

public struct Splat : IDifferentiable {
  public float2 center;
  public float2 logScale;
  public float rotation;
  public float opacityLogit;
  public float3 color;

  public static Splat load(SplatRecord record) {
    Splat splat;
    splat.center = record.shape.xy;
    splat.logScale = record.shape.zw;
    splat.rotation = record.extra.x;
    splat.opacityLogit = record.extra.y;
    splat.color = record.color.rgb;
    return splat;
  }
};

// What the forward pass leaves for the backward pass at each training pixel.
public struct PixelState {
  public float4 blended; // blended color, total weight
  public float4 gradient; // dLoss/dColor, loss
};

public SplatRecord toRecord(Splat.Differential d) {
  SplatRecord record;
  record.shape = float4(d.center, d.logScale);
  record.extra = float4(d.rotation, d.opacityLogit, 0.0, 0.0);
  record.color = float4(d.color, 0.0);
  return record;
}

public SplatRecord addRecords(SplatRecord a, SplatRecord b) {
  SplatRecord sum;
  sum.shape = a.shape + b.shape;
  sum.extra = a.extra + b.extra;
  sum.color = a.color + b.color;
  return sum;
}

// The analytic derivative of the logistic function, instead of differentiating through exp().
[BackwardDerivative(sigmoidBackward)]
[Differentiable]
public float sigmoid(float x) {
  return 1.0 / (1.0 + exp(-x));
}

void sigmoidBackward(inout DifferentialPair<float> x, float dOut) {
  const float s = 1.0 / (1.0 + exp(-x.p));
  x = diffPair(x.p, dOut * s * (1.0 - s));
}

// The falloff of a splat around its center, in units of its standard deviation.
public interface ISplatKernel {
  [Differentiable]
  public static float profile(float2 local);
};

public struct GaussianKernel : ISplatKernel {
  [Differentiable]
  public static float profile(float2 local) {
    return exp(-0.5 * dot(local, local));
  }
};

public struct SoftDiskKernel : ISplatKernel {
  [Differentiable]
  public static float profile(float2 local) {
    return sigmoid(4.0 * (2.0 - sqrt(dot(local, local) + 1e-6)));
  }
};

// How far a predicted color is from the target.
public interface ILoss {
  [Differentiable]
  public static float pixelLoss(float3 predicted, no_diff float3 target);
};

public struct L2Loss : ILoss {
  [Differentiable]
  public static float pixelLoss(float3 predicted, no_diff float3 target) {
    const float3 d = predicted - target;
    return dot(d, d);
  }
};

public struct CharbonnierLoss : ILoss {
  [Differentiable]
  public static float pixelLoss(float3 predicted, no_diff float3 target) {
    const float3 d = predicted - target;
    return sqrt(dot(d, d) + 1e-6);
  }
};

[Differentiable]
public float2 localCoordinates(Splat splat, no_diff float2 pixel) {
  const float2 d = pixel - splat.center;
  const float c = cos(splat.rotation);
  const float s = sin(splat.rotation);
  return float2(c * d.x + s * d.y, c * d.y - s * d.x) / exp(splat.logScale);
}

[Differentiable]
public float splatWeight<K : ISplatKernel>(Splat splat, no_diff float2 pixel) {
  const float2 local = localCoordinates(splat, pixel);
  if (abs(local.x) > kExtent || abs(local.y) > kExtent) {
    return 0.0;
  }
  return sigmoid(splat.opacityLogit) * K.profile(local);
}

// One splat's term of the blend with the blended color C and the total weight D held fixed:
// d/dsplat [w (c - C) / D] equals dC/dsplat, the exact gradient of the normalized blend.
[Differentiable]
public float3 blendTerm<K : ISplatKernel>(Splat splat,
                                          no_diff float2 pixel,
                                          no_diff float3 blended,
                                          no_diff float totalWeight) {
  return splatWeight<K>(splat, pixel) * (splat.color - blended) / totalWeight;
}

// Half extent of the axis-aligned box around a splat's cutoff.
public float2 boundingRadius(Splat splat) {
  const float2 scale = exp(splat.logScale) * kExtent;
  const float c = abs(cos(splat.rotation));
  const float s = abs(sin(splat.rotation));
  return float2(c * scale.x + s * scale.y, s * scale.x + c * scale.y);
}
