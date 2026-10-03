/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Runs the TriangleWebGPU wasm samples under Node with `navigator.gpu` from Dawn's Node binding
// (the `webgpu` npm package), compares their frames with the native reference frame written by
// WebGPUTriangleRenderTest, and checks the JSPI rules:
// - export audit: igl_triangle() is the only JSPI export (it returns a Promise); every other
//   export returns synchronously, also while igl_triangle() is suspended;
// - an imported page device renders and reads back without waiting (AsyncTextureReadback);
// - error scopes that do not wait latch their errors;
// - with suspension disallowed waits fail cleanly, and without the guard a wait outside a JSPI
//   export traps with WebAssembly.SuspendError.
//
// usage: node run_triangle_node.mjs <iglWebGPUTriangleWasm dir> <iglWebGPUTriangleWasmAsync dir>
//            <reference.rgba | -> [out dir]
// With `-` instead of the native frame, the first frame (JSPI igl_triangle()) is checked for the
// clear color and a covered center, and every other frame is compared with it.
// The `webgpu` package is resolved from WEBGPU_NODE_MODULE, else from the module path.

import fs from 'node:fs';
import {createRequire} from 'node:module';
import path from 'node:path';

const [jspiDir, asyncDir, referencePath, outDir] = process.argv.slice(2);
if (!jspiDir || !asyncDir || !referencePath) {
  console.error(
    'usage: node run_triangle_node.mjs <jspi dir> <async dir> <reference.rgba | -> [out dir]',
  );
  process.exit(2);
}
const {create, globals} = await import(process.env.WEBGPU_NODE_MODULE ?? 'webgpu');
Object.assign(globalThis, globals);
Object.defineProperty(globalThis, 'navigator', {
  value: {gpu: create([])},
  configurable: true,
});

const kSize = 256;
const kBytes = kSize * kSize * 4;
const kSampleExports = [
  '_igl_canvas_frame',
  '_igl_canvas_init',
  '_igl_import_device',
  '_igl_make_latched_error',
  '_igl_poll_async',
  '_igl_render_async',
  '_igl_take_errors',
  '_igl_triangle',
  '_igl_wait_disallowed',
  '_igl_wait_unguarded',
];
let reference = referencePath === '-' ? null : fs.readFileSync(referencePath);
let failures = 0;

function check(ok, what) {
  console.log(`${ok ? 'PASS' : 'FAIL'} ${what}`);
  failures += ok ? 0 : 1;
}

// TriangleRender's clear color, (0.2, 0.3, 0.4, 1) in 8-bit unorm.
const kClearColor = [51, 77, 102, 255];

// 0.3 * 255 is exactly 76.5, so adapters round green to 76 or 77.
function isClearColor(pixel) {
  return pixel.every((value, i) => Math.abs(value - kClearColor[i]) <= 1);
}

function adoptAsReference(frame, name) {
  const corner = Array.from(frame.subarray(0, 4));
  const centerOffset = ((kSize / 2) * kSize + kSize / 2) * 4;
  const center = Array.from(frame.subarray(centerOffset, centerOffset + 4));
  check(
    isClearColor(corner) && !isClearColor(center),
    `${name}: corner ${corner} is the clear color, center ${center} is covered (reference frame)`,
  );
  reference = Buffer.from(frame);
}

function compare(frame, name) {
  if (reference === null) {
    adoptAsReference(frame, name);
    return;
  }
  let maxAbs = 0;
  let differing = 0;
  for (let i = 0; i < kBytes; i += 4) {
    let d = 0;
    for (let c = 0; c < 4; ++c) {
      d = Math.max(d, Math.abs(frame[i + c] - reference[i + c]));
    }
    maxAbs = Math.max(maxAbs, d);
    differing += d > 0 ? 1 : 0;
  }
  if (outDir) {
    fs.writeFileSync(path.join(outDir, `${name}.rgba`), frame);
  }
  const percent = ((100 * differing) / (kSize * kSize)).toFixed(4);
  const against = referencePath === '-' ? 'the first frame' : 'the native frame';
  check(maxAbs <= 1, `${name}: maxAbs=${maxAbs} differing=${percent}% vs ${against}`);
}

const tick = () => new Promise(resolve => setTimeout(resolve, 1));

async function load(dir, name) {
  const require = createRequire(import.meta.url);
  const factory = require(path.resolve(dir, name));
  return await factory({locateFile: file => path.resolve(dir, file)});
}

// A new export must be added here and classified: JSPI exports return a Promise (checked below);
// every other export must not wait unless suspension is disallowed.
function auditExports(module, flavor) {
  const exported = Object.keys(module)
    .filter(key => key.startsWith('_igl_'))
    .sort();
  check(
    JSON.stringify(exported) === JSON.stringify(kSampleExports),
    `${flavor}: exports are ${exported.join(', ')}`,
  );
}

async function importDevice(module, flavor) {
  const adapter = await navigator.gpu.requestAdapter();
  module.iglWebGPUDevice = await adapter.requestDevice();
  const status = module._igl_import_device();
  check(status === 0, `${flavor}: igl_import_device() returns ${status} synchronously`);
}

async function renderAsync(module, flavor) {
  const ptr = module._malloc(kBytes);
  const started = module._igl_render_async(kSize);
  check(started === 0, `${flavor}: igl_render_async() returns ${started} synchronously`);
  let status = 0;
  for (let i = 0; i < 10000 && status === 0; ++i) {
    await tick();
    status = module._igl_poll_async(ptr);
  }
  check(status === 1, `${flavor}: igl_poll_async() completes (${status})`);
  compare(module.HEAPU8.slice(ptr, ptr + kBytes), `TriangleWebGPU_${flavor}_async`);
  module._free(ptr);
}

async function checkLatchedErrors(module, flavor) {
  module._igl_take_errors();
  const status = module._igl_make_latched_error();
  check(status === 0, `${flavor}: a create call that does not wait returns Ok provisionally`);
  let errors = 0;
  for (let i = 0; i < 1000 && errors === 0; ++i) {
    await tick();
    errors = module._igl_take_errors();
  }
  check(errors === 1, `${flavor}: the error is latched (${errors})`);
}

// JSPI flavor.
{
  const module = await load(jspiDir, 'iglWebGPUTriangleWasm');
  auditExports(module, 'jspi');
  await importDevice(module, 'jspi');

  const ptr = module._malloc(kBytes);
  const pending = module._igl_triangle(kSize, ptr);
  check(pending instanceof Promise, 'jspi: igl_triangle() is a JSPI export (returns a Promise)');
  // Non-suspending exports may run while igl_triangle() is suspended.
  const during = module._igl_take_errors();
  check(typeof during === 'number', 'jspi: a non-JSPI export runs during the suspension');
  const status = await pending;
  check(status === 0, `jspi: igl_triangle() returns ${status}`);
  compare(module.HEAPU8.slice(ptr, ptr + kBytes), 'TriangleWebGPU_jspi');
  module._free(ptr);

  await renderAsync(module, 'jspi');
  await checkLatchedErrors(module, 'jspi');
  const guarded = module._igl_wait_disallowed();
  check(guarded === 0, 'jspi: waits fail cleanly while suspension is disallowed');
  let trapped = null;
  try {
    const result = module._igl_wait_unguarded();
    trapped = result instanceof Promise ? 'returned a Promise' : `returned ${result}`;
  } catch (e) {
    trapped = e?.constructor?.name ?? String(e);
  }
  check(
    trapped === 'SuspendError',
    `jspi: an unguarded wait outside a JSPI export traps (${trapped})`,
  );
}

// Async flavor: nothing can wait.
{
  const module = await load(asyncDir, 'iglWebGPUTriangleWasmAsync');
  auditExports(module, 'async');
  const ptr = module._malloc(kBytes);
  const status = module._igl_triangle(kSize, ptr);
  module._free(ptr);
  check(status !== 0 && typeof status === 'number', `async: igl_triangle() cannot wait (${status})`);
  await importDevice(module, 'async');
  await renderAsync(module, 'async');
  await checkLatchedErrors(module, 'async');
  const guarded = module._igl_wait_disallowed();
  check(guarded === 0, 'async: waits fail cleanly');
}

console.log(failures === 0 ? 'ALL PASSED' : `${failures} FAILED`);
process.exit(failures === 0 ? 0 : 1);
