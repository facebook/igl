/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class D3D12Context;

/**
 * @brief Manages swapchain presentation with device removal detection
 *
 * Handles:
 * - VSync configuration via environment variable
 * - Present flags (tearing support)
 * - Device removal detection before/after Present
 * - DRED and Info Queue diagnostics on failure
 */
class PresentManager final {
 public:
  explicit PresentManager(D3D12Context& context) : context_(context) {}

  /**
   * @brief Present the current frame with proper error handling
   *
   * Checks device status before and after Present, logs diagnostics on failure.
   * Does not throw - sets device lost flag for application to check.
   *
   * @return true if present succeeded, false if device was removed or present failed
   */
  bool present();

 private:
  /**
   * @brief Check device status and log diagnostics if removed
   */
  bool checkDeviceStatus(const char* context);

  /**
   * @brief Log Info Queue messages for debugging
   */
  void logInfoQueueMessages(ID3D12Device* device);

  /**
   * @brief Log DRED breadcrumbs and page fault info
   */
  void logDredInfo(ID3D12Device* device);

  D3D12Context& context_;
};

} // namespace igl::d3d12
