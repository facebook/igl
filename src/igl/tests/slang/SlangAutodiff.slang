/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Forward- and reverse-mode derivatives, detach() and a custom backward derivative.
#include "igl_abi.slangh"

IGL_STORAGE_BUFFER(float2, inputs, 0);
IGL_STORAGE_BUFFER(float4, results, 1);

[BackwardDerivative(softplusBackward)]
[Differentiable]
float softplus(float x) {
  return log(1.0 + exp(x));
}

void softplusBackward(inout DifferentialPair<float> x, float dOut) {
  x = diffPair(x.p, dOut / (1.0 + exp(-x.p)));
}

// detach() hides x from the derivative of the last term, but not y.
[Differentiable]
float objective(float x, float y) {
  return sin(x) * y + softplus(x * y) + detach(x) * 0.5 * y;
}

// results[i] = (f, df/dx by forward mode, df/dx and df/dy by reverse mode).
[shader("compute")]
[numthreads(64, 1, 1)]
void evaluateDerivatives(uint3 dispatchId: SV_DispatchThreadID) {
  const float2 xy = inputs[dispatchId.x];
  const DifferentialPair<float> forward =
      fwd_diff(objective)(diffPair(xy.x, 1.0), diffPair(xy.y, 0.0));
  var dx = diffPair(xy.x);
  var dy = diffPair(xy.y);
  bwd_diff(objective)(dx, dy, 1.0);
  results[dispatchId.x] = float4(forward.p, forward.d, dx.d, dy.d);
}
