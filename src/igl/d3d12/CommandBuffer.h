/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/CommandBuffer.h>
#include <igl/ComputeCommandEncoder.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12Context.h>

namespace igl::d3d12 {

class Device;

class CommandBuffer final : public ICommandBuffer {
 public:
  CommandBuffer(Device& device, const CommandBufferDesc& desc);
  ~CommandBuffer() override = default;

  std::unique_ptr<IRenderCommandEncoder> createRenderCommandEncoder(
      const RenderPassDesc& renderPass,
      const std::shared_ptr<IFramebuffer>& framebuffer,
      const Dependencies& dependencies,
      Result* IGL_NULLABLE outResult) override;

  std::unique_ptr<IComputeCommandEncoder> createComputeCommandEncoder() override;

  void present(const std::shared_ptr<ITexture>& surface) const override;

  void waitUntilScheduled() override;
  void waitUntilCompleted() override;

  void pushDebugGroupLabel(const char* label, const igl::Color& color) const override;
  void popDebugGroupLabel() const override;

  void begin();
  void end();

  ID3D12GraphicsCommandList* getCommandList() const { return commandList_.Get(); }
  D3D12Context& getContext();
  const D3D12Context& getContext() const;

 private:
  Device& device_;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
};

} // namespace igl::d3d12
