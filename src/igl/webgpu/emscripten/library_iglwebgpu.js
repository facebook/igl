/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Emscripten JS library for IGL's WebGPU backend (link with --js-library). The page creates the
// GPUDevice and stores it in Module.iglWebGPUDevice (or Module.preinitializedWebGPUDevice);
// igl::webgpu::HWDevice::createWithJsDevice() imports it with WebGPU.importJsDevice(), parented to
// the backend's WGPUInstance so that futures of the device can be waited on.

addToLibrary({
  igl_webgpu_import_js_device__deps: [
    '$WebGPU',
    '$stringToNewUTF8',
    'free',
    'igl_webgpu_on_device_lost',
    'igl_webgpu_on_uncaptured_error',
  ],
  igl_webgpu_import_js_device: (instancePtr, statePtr) => {
    const device = Module['iglWebGPUDevice'] ?? Module['preinitializedWebGPUDevice'];
    if (!device) {
      return 0;
    }
    // An import stays registered until the device is lost or igl_webgpu_release_js_device() is
    // called, whichever comes first; that one ends it (and the device-lost path frees `statePtr`).
    const imports = (Module['iglWebGPUImports'] ??= new Map());
    const entry = {device, listener: null};
    entry.listener = event => {
      const message = stringToNewUTF8(event.error?.message ?? String(event.error));
      _igl_webgpu_on_uncaptured_error(statePtr, message);
      _free(message);
    };
    device.addEventListener('uncapturederror', entry.listener);
    imports.set(statePtr, entry);
    device.lost.then(info => {
      if (imports.get(statePtr) !== entry) {
        return; // Released.
      }
      imports.delete(statePtr);
      device.removeEventListener('uncapturederror', entry.listener);
      const message = stringToNewUTF8(info.message ?? '');
      _igl_webgpu_on_device_lost(statePtr, info.reason === 'destroyed' ? 1 : 0, message);
      _free(message);
    });
    return WebGPU.importJsDevice(device, instancePtr);
  },
  // Ends an import when the IGL context goes away while the page keeps the device. Returns 1 when
  // the caller still owns `statePtr` (the device was not lost yet) and must free it.
  igl_webgpu_release_js_device: statePtr => {
    const imports = Module['iglWebGPUImports'];
    const entry = imports?.get(statePtr);
    if (!entry) {
      return 0;
    }
    imports.delete(statePtr);
    entry.device.removeEventListener('uncapturederror', entry.listener);
    return 1;
  },
});
