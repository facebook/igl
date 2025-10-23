/*
 * Minimal headless D3D12 context for unit tests (no swapchain / no HWND).
 */

#pragma once

#include <igl/d3d12/D3D12Context.h>
#include <igl/d3d12/DescriptorHeapManager.h>
#include <memory>

namespace igl::d3d12 {

class HeadlessD3D12Context final : public D3D12Context {
 public:
  HeadlessD3D12Context() = default;
  ~HeadlessD3D12Context();

  // Initialize a headless context with default dimensions used only for fallback viewports
  Result initializeHeadless(uint32_t width = 256, uint32_t height = 256);

  // Access to descriptor heap manager for tests (may be null on failure)
  [[nodiscard]] DescriptorHeapManager* getDescriptorHeapManager() const { return descriptorHeaps_.get(); }

 private:
  std::unique_ptr<DescriptorHeapManager> descriptorHeaps_;
};

} // namespace igl::d3d12
