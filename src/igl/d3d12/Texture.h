/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <vector>
#include <igl/Texture.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class Texture final : public ITexture {
 public:
  Texture() : ITexture(TextureFormat::Invalid), format_(TextureFormat::Invalid) {}
  explicit Texture(TextureFormat format) : ITexture(format), format_(format) {}

  // B-001: Explicit destructor to free descriptor heap slots
  ~Texture() override;

  // Factory method to create texture from existing D3D12 resource
  static std::shared_ptr<Texture> createFromResource(
      ID3D12Resource* resource,
      TextureFormat format,
      const TextureDesc& desc,
      ID3D12Device* device = nullptr,
      ID3D12CommandQueue* queue = nullptr,
      D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON,
      class Device* iglDevice = nullptr);

  // Factory method to create texture view from parent texture
  static std::shared_ptr<Texture> createTextureView(
      std::shared_ptr<Texture> parent,
      const TextureViewDesc& desc);

  // D3D12-specific upload methods (not part of ITexture interface)
  Result upload(const TextureRangeDesc& range,
                const void* data,
                size_t bytesPerRow = 0) const;
  Result uploadCube(const TextureRangeDesc& range,
                   TextureCubeFace face,
                   const void* data,
                   size_t bytesPerRow = 0) const;

  Dimensions getDimensions() const override;
  uint32_t getNumLayers() const override;
  TextureType getType() const override;
  TextureDesc::TextureUsage getUsage() const override;
  uint32_t getSamples() const override;
  uint32_t getNumMipLevels() const override;
  uint64_t getTextureId() const override;
  bool isRequiredGenerateMipmap() const override;

  void generateMipmap(ICommandQueue& cmdQueue,
                      const TextureRangeDesc* IGL_NULLABLE range = nullptr) const override;
  void generateMipmap(ICommandBuffer& cmdBuffer,
                      const TextureRangeDesc* IGL_NULLABLE range = nullptr) const override;

  // D3D12-specific accessors (not part of ITexture interface)
  TextureFormat getFormat() const;
  ID3D12Resource* getResource() const { return resource_.Get(); }
  void transitionTo(ID3D12GraphicsCommandList* commandList,
                    D3D12_RESOURCE_STATES newState,
                    uint32_t mipLevel = 0,
                    uint32_t layer = 0) const;
  void transitionAll(ID3D12GraphicsCommandList* commandList,
                     D3D12_RESOURCE_STATES newState) const;
  D3D12_RESOURCE_STATES getSubresourceState(uint32_t mipLevel = 0,
                                            uint32_t layer = 0) const;

  // Texture view support
  bool isView() const { return isView_; }
  uint32_t getMipLevelOffset() const { return mipLevelOffset_; }
  uint32_t getNumMipLevelsInView() const { return numMipLevelsInView_; }
  uint32_t getArraySliceOffset() const { return arraySliceOffset_; }
  uint32_t getNumArraySlicesInView() const { return numArraySlicesInView_; }

  // Subresource calculation helper
  uint32_t calcSubresourceIndex(uint32_t mipLevel, uint32_t layer) const;

 protected:
  // Override the base class upload method
  Result uploadInternal(TextureType type,
                        const TextureRangeDesc& range,
                        const void* data,
                        size_t bytesPerRow = 0,
                        const uint32_t* mipLevelBytes = nullptr) const override;

 private:
  Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
  ID3D12Device* device_ = nullptr; // Non-owning pointer
  ID3D12CommandQueue* queue_ = nullptr; // Non-owning pointer
  class Device* iglDevice_ = nullptr; // Non-owning pointer to igl::d3d12::Device for upload operations (P0_DX12-005)
  TextureFormat format_;
  Dimensions dimensions_{0, 0, 0};
  TextureType type_ = TextureType::TwoD;
  size_t numLayers_ = 1;
  size_t numMipLevels_ = 1;
  size_t samples_ = 1;
  TextureDesc::TextureUsage usage_ = 0;
  void initializeStateTracking(D3D12_RESOURCE_STATES initialState) const;
  void ensureStateStorage() const;

  // B-006: Optimized state tracking with fast path for uniform states
  mutable bool hasUniformState_ = true;
  mutable D3D12_RESOURCE_STATES uniformState_ = D3D12_RESOURCE_STATE_COMMON;
  mutable std::vector<D3D12_RESOURCE_STATES> subresourceStates_;
  mutable D3D12_RESOURCE_STATES defaultState_ = D3D12_RESOURCE_STATE_COMMON;

  void promoteToHeterogeneousState() const;

  // Texture view support
  bool isView_ = false;
  std::shared_ptr<Texture> parentTexture_;  // For views, reference to parent
  uint32_t mipLevelOffset_ = 0;  // MostDetailedMip for SRV
  uint32_t numMipLevelsInView_ = 0;  // MipLevels for SRV
  uint32_t arraySliceOffset_ = 0;  // FirstArraySlice for SRV
  uint32_t numArraySlicesInView_ = 0;  // ArraySize for SRV

  // B-001: Descriptor indices for cleanup in destructor
  // These descriptors are allocated from DescriptorHeapManager and must be freed
  std::vector<uint32_t> rtvIndices_;  // RTV descriptors (one per mip level)
  std::vector<uint32_t> dsvIndices_;  // DSV descriptors (for depth/stencil textures)
  uint32_t srvIndex_ = UINT32_MAX;    // SRV descriptor (UINT32_MAX = not allocated)
};

} // namespace igl::d3d12
