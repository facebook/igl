/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <tuple>
#include <igl/Texture.h>
#include <igl/webgpu/Common.h>

namespace igl::webgpu {

class DeviceFeatureSet;
class WebGPUContext;

/// @brief Where a deferred texture's WGPUTexture comes from (see Texture::createDeferred()).
struct DeferredTextureSource {
  /// Called at most once, on first use; may return null (the texture then fails its uses).
  std::function<Handle<WGPUTexture>()> acquire;
  /// Called once when a command buffer that presents the acquired texture is submitted.
  std::function<void()> present;
};

/// @brief Implements the igl::ITexture interface for WebGPU.
///
/// A texture created by createTextureView() shares its parent's WGPUTexture and addresses the
/// parent's subresources through a base mip level and base layer.
class Texture final : public ITexture {
 public:
  [[nodiscard]] static std::shared_ptr<Texture> create(WebGPUContext& ctx,
                                                       const DeviceFeatureSet& features,
                                                       const TextureDesc& desc,
                                                       Result* IGL_NULLABLE outResult);
  [[nodiscard]] static std::shared_ptr<Texture> createView(std::shared_ptr<Texture> parent,
                                                           const TextureViewDesc& desc,
                                                           Result* IGL_NULLABLE outResult);
  /// A texture whose WGPUTexture comes from `source` on first use (as an attachment or copy
  /// source), not at creation. Surfaces use it to acquire their texture after the frame's last
  /// suspension point. The WGPUTexture is released, never destroyed, and cannot be sampled.
  [[nodiscard]] static std::shared_ptr<Texture> createDeferred(WebGPUContext& ctx,
                                                               const DeviceFeatureSet& features,
                                                               const TextureDesc& desc,
                                                               DeferredTextureSource source,
                                                               Result* IGL_NULLABLE outResult);
  ~Texture() override;

  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;
  Texture(Texture&&) = delete;
  Texture& operator=(Texture&&) = delete;

  [[nodiscard]] Dimensions getDimensions() const override;
  [[nodiscard]] uint32_t getNumLayers() const override;
  [[nodiscard]] TextureType getType() const override;
  [[nodiscard]] TextureDesc::TextureUsage getUsage() const override;
  [[nodiscard]] uint32_t getSamples() const override;
  [[nodiscard]] uint32_t getNumMipLevels() const override;
  void generateMipmap(ICommandQueue& cmdQueue,
                      const TextureRangeDesc* IGL_NULLABLE range = nullptr) const override;
  void generateMipmap(ICommandBuffer& cmdBuffer,
                      const TextureRangeDesc* IGL_NULLABLE range = nullptr) const override;
  [[nodiscard]] bool isRequiredGenerateMipmap() const override;
  [[nodiscard]] bool supportsUpload() const override;
  [[nodiscard]] uint64_t getTextureId() const override;

  [[nodiscard]] void* IGL_NULLABLE getNativeImage() const override;
  [[nodiscard]] void* IGL_NULLABLE getNativeImageView() const override;
  [[nodiscard]] const base::AttachmentInteropDesc& getDesc() const override;

  [[nodiscard]] WGPUTexture IGL_NULLABLE getWGPUTexture() const noexcept;
  [[nodiscard]] WGPUTextureFormat getWGPUFormat() const noexcept {
    return wgpuFormat_;
  }
  /// First mip level and layer of the parent texture that this texture covers (0 unless a view).
  [[nodiscard]] uint32_t getBaseMipLevel() const noexcept {
    return baseMipLevel_;
  }
  [[nodiscard]] uint32_t getBaseLayer() const noexcept {
    return baseLayer_;
  }
  /// View for sampling all of this texture's mips and layers. Depth/stencil formats expose only
  /// the depth aspect (the stencil aspect for stencil-only formats), because WebGPU cannot sample
  /// a view that has both.
  [[nodiscard]] WGPUTextureView IGL_NULLABLE getSampledView() const noexcept {
    return sampledView_.get();
  }
  /// Single-subresource view of `mipLevel` and `layer` (relative to this texture) for render
  /// pass attachments; a 3D texture uses the whole mip level. Views are created on first use.
  [[nodiscard]] WGPUTextureView IGL_NULLABLE getAttachmentView(uint32_t mipLevel,
                                                               uint32_t layer) const;
  /// Single-mip view of the base mip level for storage bindings of `dimension`, or null when the
  /// texture cannot be viewed that way. Views are created on first use.
  [[nodiscard]] WGPUTextureView IGL_NULLABLE
  getStorageView(WGPUTextureViewDimension dimension) const;
  /// Index of `face` of `layer` among the WGPU array layers (cube faces are layers in WebGPU).
  [[nodiscard]] uint32_t getWGPULayer(uint32_t layer, uint32_t face) const noexcept;

  [[nodiscard]] bool isDeferred() const noexcept;
  /// Whether the WGPUTexture exists; deferred textures acquire it on first use.
  [[nodiscard]] bool isAcquired() const noexcept;
  /// Presents a deferred texture that was acquired; a no-op otherwise.
  void present() const;

  /// Records that the command buffer with `serial` uses this texture.
  void recordUse(uint64_t serial) const noexcept;

  /// Reads back one mip level and layer region through the command queue's device. Rows are
  /// stored bottom-up, as with every IGL readback, and channels are not swizzled.
  [[nodiscard]] Result getBytes(const TextureRangeDesc& range,
                                WGPUTextureAspect aspect,
                                void* IGL_NONNULL outData,
                                size_t bytesPerRow,
                                bool flipVertically = true) const;

 protected:
  [[nodiscard]] Result uploadInternal(TextureType type,
                                      const TextureRangeDesc& range,
                                      const void* IGL_NULLABLE data,
                                      size_t bytesPerRow,
                                      const uint32_t* IGL_NULLABLE mipLevelBytes) const override;

 private:
  struct Storage;

  Texture(std::shared_ptr<Storage> storage,
          const TextureDesc& desc,
          WGPUTextureFormat wgpuFormat,
          uint32_t baseMipLevel,
          uint32_t baseLayer);

  [[nodiscard]] Result createSampledView();
  [[nodiscard]] Result checkMipmapSupport() const;
  [[nodiscard]] Result encodeMipmaps(WGPUCommandEncoder IGL_NONNULL encoder,
                                     const TextureRangeDesc* IGL_NULLABLE range) const;
  // Encodes the mipmaps in a new command buffer and submits it.
  [[nodiscard]] Result submitMipmaps(const TextureRangeDesc* IGL_NULLABLE range) const;
  [[nodiscard]] Result uploadDepth(const TextureRangeDesc& range,
                                   const void* IGL_NONNULL data,
                                   size_t bytesPerRow) const;

  std::shared_ptr<Storage> storage_;
  const TextureDesc desc_;
  const WGPUTextureFormat wgpuFormat_;
  const uint32_t baseMipLevel_;
  const uint32_t baseLayer_;
  const uint64_t textureId_;
  Handle<WGPUTextureView> sampledView_;
  mutable std::map<std::tuple<uint32_t, uint32_t>, Handle<WGPUTextureView>> attachmentViews_;
  mutable std::map<WGPUTextureViewDimension, Handle<WGPUTextureView>> storageViews_;
  mutable base::AttachmentInteropDesc attachmentDesc_;
};

/// Bytes per texel of one aspect of `format` in texture-buffer copies, or 0 if that aspect cannot
/// be copied (the depth aspect of depth24plus formats) or the format is block-compressed.
[[nodiscard]] uint32_t getCopyBytesPerTexel(WGPUTextureFormat format, WGPUTextureAspect aspect);

} // namespace igl::webgpu
