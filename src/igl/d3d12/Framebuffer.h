/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <limits>
#include <vector>
#include <igl/Framebuffer.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class Framebuffer final : public IFramebuffer {
 public:
  Framebuffer(const FramebufferDesc& desc);
  ~Framebuffer() override;

  std::vector<size_t> getColorAttachmentIndices() const override;
  std::shared_ptr<ITexture> getColorAttachment(size_t index) const override;
  std::shared_ptr<ITexture> getResolveColorAttachment(size_t index) const override;
  std::shared_ptr<ITexture> getDepthAttachment() const override;
  std::shared_ptr<ITexture> getResolveDepthAttachment() const override;
  std::shared_ptr<ITexture> getStencilAttachment() const override;
  FramebufferMode getMode() const override;
  bool isSwapchainBound() const override;

  void copyBytesColorAttachment(ICommandQueue& cmdQueue,
                                 size_t index,
                                 void* pixelBytes,
                                 const TextureRangeDesc& range,
                                 size_t bytesPerRow) const override;
  void copyBytesDepthAttachment(ICommandQueue& cmdQueue,
                                void* pixelBytes,
                                const TextureRangeDesc& range,
                                size_t bytesPerRow) const override;
  void copyBytesStencilAttachment(ICommandQueue& cmdQueue,
                                  void* pixelBytes,
                                  const TextureRangeDesc& range,
                                  size_t bytesPerRow) const override;
  void copyTextureColorAttachment(ICommandQueue& cmdQueue,
                                  size_t index,
                                  std::shared_ptr<ITexture> destTexture,
                                  const TextureRangeDesc& range) const override;
  void updateDrawable(std::shared_ptr<ITexture> texture) override;
  void updateDrawable(SurfaceTextures surfaceTextures) override;
  void updateResolveAttachment(std::shared_ptr<ITexture> texture) override;

 private:
  struct ReadbackResources {
    Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer;
    uint64_t readbackBufferSize = 0;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    HANDLE fenceEvent = nullptr;
    uint64_t lastFenceValue = 0;
    std::vector<uint8_t> cachedData;
    uint32_t cachedWidth = 0;
    uint32_t cachedHeight = 0;
    uint32_t cachedMipLevel = 0;
    uint32_t cachedLayer = 0;
    uint64_t cachedRowPitch = 0;
    size_t cachedBytesPerPixel = 0;
    UINT64 cachedFrameFenceValue = std::numeric_limits<UINT64>::max();
    bool cacheValid = false;
  };

  mutable std::array<ReadbackResources, IGL_COLOR_ATTACHMENTS_MAX> readbackCache_{};
  FramebufferDesc desc_;
};

} // namespace igl::d3d12
