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
    // `statePtr` stays valid until _igl_webgpu_on_device_lost() frees it.
    let lost = false;
    device.addEventListener('uncapturederror', event => {
      if (lost) {
        return;
      }
      const message = stringToNewUTF8(event.error?.message ?? String(event.error));
      _igl_webgpu_on_uncaptured_error(statePtr, message);
      _free(message);
    });
    device.lost.then(info => {
      lost = true;
      const message = stringToNewUTF8(info.message ?? '');
      _igl_webgpu_on_device_lost(statePtr, info.reason === 'destroyed' ? 1 : 0, message);
      _free(message);
    });
    return WebGPU.importJsDevice(device, instancePtr);
  },
});
