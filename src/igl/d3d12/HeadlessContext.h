/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

/*
 * Minimal headless D3D12 context for unit tests (no swapchain / no HWND).
 */

#pragma once

#include <memory>
#include <igl/d3d12/D3D12Context.h>
#include <igl/d3d12/DescriptorHeapManager.h>

namespace igl::d3d12 {

class HeadlessD3D12Context final : public D3D12Context {
 public:
  HeadlessD3D12Context() = default;
  ~HeadlessD3D12Context();

  // Initialize a headless context with default dimensions used only for fallback viewports
  // Accepts optional D3D12ContextConfig for configurable sizes.
  // NOTE: Headless mode currently uses environment variable overrides and internal defaults
  // for descriptor heap sizes. Config parameter is stored for base-class consistency and
  // future extension but is not fully wired to all heap creation paths yet.
  Result initializeHeadless(uint32_t width = 256,
                            uint32_t height = 256,
                            const D3D12ContextConfig& config = D3D12ContextConfig::defaultConfig());

  // Access to descriptor heap manager for tests (may be null on failure)
  [[nodiscard]] DescriptorHeapManager* getDescriptorHeapManager() const {
    return descriptorHeaps_.get();
  }

 private:
  std::unique_ptr<DescriptorHeapManager> descriptorHeaps_;
};

} // namespace igl::d3d12
