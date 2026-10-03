/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Runs the iglWebGPUTriangleWasm sample under Node with `navigator.gpu` from Dawn's Node binding
// (the `webgpu` npm package) and writes its 256x256 RGBA frame. With a reference frame (written by
// the native WebGPUTriangleRenderTest), prints the difference.
//
// usage: node run_triangle_node.mjs <dir with iglWebGPUTriangleWasm{,.wasm}> <out.rgba>
//            [reference.rgba]
// The `webgpu` package is resolved from WEBGPU_NODE_MODULE, else from the module path.

import fs from 'node:fs';
import {createRequire} from 'node:module';
import path from 'node:path';

const [dir, outPath, referencePath] = process.argv.slice(2);
if (!dir || !outPath) {
  console.error('usage: node run_triangle_node.mjs <dir> <out.rgba> [reference.rgba]');
  process.exit(2);
}
const {create, globals} = await import(process.env.WEBGPU_NODE_MODULE ?? 'webgpu');
Object.assign(globalThis, globals);
Object.defineProperty(globalThis, 'navigator', {
  value: {gpu: create([])},
  configurable: true,
});

const require = createRequire(import.meta.url);
const factory = require(path.resolve(dir, 'iglWebGPUTriangleWasm'));
const module = await factory({locateFile: file => path.resolve(dir, file)});

const size = 256;
const bytes = size * size * 4;
const ptr = module._malloc(bytes);
const status = await module._igl_triangle(size, ptr);
if (status !== 0) {
  console.error(`igl_triangle() failed: ${status}`);
  process.exit(1);
}
const frame = module.HEAPU8.slice(ptr, ptr + bytes);
module._free(ptr);
fs.writeFileSync(outPath, frame);
console.log(`wrote ${outPath}`);

if (referencePath) {
  const reference = fs.readFileSync(referencePath);
  let maxAbs = 0;
  let differing = 0;
  for (let i = 0; i < bytes; i += 4) {
    let d = 0;
    for (let c = 0; c < 4; ++c) {
      d = Math.max(d, Math.abs(frame[i + c] - reference[i + c]));
    }
    maxAbs = Math.max(maxAbs, d);
    differing += d > 0 ? 1 : 0;
  }
  console.log(`maxAbs=${maxAbs} differing=${((100 * differing) / (size * size)).toFixed(4)}%`);
}
process.exit(0);
