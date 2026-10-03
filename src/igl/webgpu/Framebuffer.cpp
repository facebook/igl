/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/Framebuffer.h>

#include <utility>
#include <igl/webgpu/Texture.h>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

namespace {

// Multisampled textures can be neither read back nor copied to single-sampled textures; their
// resolve targets hold the rendered result.
const std::shared_ptr<ITexture>& getCopySource(const FramebufferDesc::AttachmentDesc& attachment) {
  return attachment.texture && attachment.texture->getSamples() > 1 && attachment.resolveTexture
             ? attachment.resolveTexture
             : attachment.texture;
}

} // namespace

Framebuffer::Framebuffer(WebGPUContext& ctx, FramebufferDesc desc) :
  ctx_(ctx), desc_(std::move(desc)) {}

std::vector<size_t> Framebuffer::getColorAttachmentIndices() const {
  std::vector<size_t> indices;
  for (size_t i = 0; i != IGL_COLOR_ATTACHMENTS_MAX; i++) {
    if (desc_.colorAttachments[i].texture || desc_.colorAttachments[i].resolveTexture) {
      indices.push_back(i);
    }
  }
  return indices;
}

std::shared_ptr<ITexture> Framebuffer::getColorAttachment(size_t index) const {
  IGL_DEBUG_ASSERT(index < IGL_COLOR_ATTACHMENTS_MAX);
  return desc_.colorAttachments[index].texture;
}

std::shared_ptr<ITexture> Framebuffer::getResolveColorAttachment(size_t index) const {
  IGL_DEBUG_ASSERT(index < IGL_COLOR_ATTACHMENTS_MAX);
  return desc_.colorAttachments[index].resolveTexture;
}

std::shared_ptr<ITexture> Framebuffer::getDepthAttachment() const {
  return desc_.depthAttachment.texture;
}

std::shared_ptr<ITexture> Framebuffer::getResolveDepthAttachment() const {
  return desc_.depthAttachment.resolveTexture;
}

std::shared_ptr<ITexture> Framebuffer::getStencilAttachment() const {
  return desc_.stencilAttachment.texture;
}

FramebufferMode Framebuffer::getMode() const {
  return desc_.mode;
}

bool Framebuffer::isSwapchainBound() const {
  const auto& texture = desc_.colorAttachments[0].texture;
  return texture && texture->isSwapchainTexture();
}

void Framebuffer::copyBytesColorAttachment(ICommandQueue& /*cmdQueue*/,
                                           size_t index,
                                           void* IGL_NONNULL pixelBytes,
                                           const TextureRangeDesc& range,
                                           size_t bytesPerRow) const {
  IGL_PROFILER_FUNCTION();
  IGL_DEBUG_ASSERT(index < IGL_COLOR_ATTACHMENTS_MAX);
  IGL_DEBUG_ASSERT(range.numFaces == 1, "range.numFaces MUST be 1");
  IGL_DEBUG_ASSERT(range.numLayers == 1, "range.numLayers MUST be 1");
  IGL_DEBUG_ASSERT(range.numMipLevels == 1, "range.numMipLevels MUST be 1");
  copyBytes(getCopySource(desc_.colorAttachments[index]),
            WGPUTextureAspect_All,
            pixelBytes,
            range,
            bytesPerRow);
}

void Framebuffer::copyBytesDepthAttachment(ICommandQueue& /*cmdQueue*/,
                                           void* IGL_NONNULL pixelBytes,
                                           const TextureRangeDesc& range,
                                           size_t bytesPerRow) const {
  IGL_PROFILER_FUNCTION();
  copyBytes(
      desc_.depthAttachment.texture, WGPUTextureAspect_DepthOnly, pixelBytes, range, bytesPerRow);
}

void Framebuffer::copyBytesStencilAttachment(ICommandQueue& /*cmdQueue*/,
                                             void* IGL_NONNULL pixelBytes,
                                             const TextureRangeDesc& range,
                                             size_t bytesPerRow) const {
  IGL_PROFILER_FUNCTION();
  copyBytes(desc_.stencilAttachment.texture,
            WGPUTextureAspect_StencilOnly,
            pixelBytes,
            range,
            bytesPerRow);
}

void Framebuffer::copyBytes(const std::shared_ptr<ITexture>& texture,
                            WGPUTextureAspect aspect,
                            void* IGL_NONNULL pixelBytes,
                            const TextureRangeDesc& range,
                            size_t bytesPerRow) const {
  if (!IGL_DEBUG_VERIFY(texture != nullptr)) {
    return;
  }
  const auto& wgpuTexture = static_cast<const Texture&>(*texture);
  // Depth-only and stencil-only formats have a single aspect, which WebGPU calls All.
  const WGPUTextureAspect copyAspect = texture->getProperties().hasDepth() &&
                                               texture->getProperties().hasStencil()
                                           ? aspect
                                           : WGPUTextureAspect_All;
  const Result result = wgpuTexture.getBytes(range, copyAspect, pixelBytes, bytesPerRow);
  if (!result.isOk()) {
    IGL_LOG_ERROR("copyBytes(): %s\n", result.message.c_str());
  }
}

void Framebuffer::copyTextureColorAttachment(ICommandQueue& /*cmdQueue*/,
                                             size_t index,
                                             std::shared_ptr<ITexture> destTexture,
                                             const TextureRangeDesc& range) const {
  IGL_PROFILER_FUNCTION();
  IGL_DEBUG_ASSERT(index < IGL_COLOR_ATTACHMENTS_MAX);
  const auto& srcTexture = getCopySource(desc_.colorAttachments[index]);
  if (!IGL_DEBUG_VERIFY(srcTexture && destTexture)) {
    return;
  }
  const auto& src = static_cast<const Texture&>(*srcTexture);
  const auto& dst = static_cast<const Texture&>(*destTexture);
  if (src.getWGPUTexture() == nullptr || dst.getWGPUTexture() == nullptr) {
    IGL_LOG_ERROR("copyTextureColorAttachment(): the surface texture could not be acquired\n");
    return;
  }
  const WGPUTexelCopyTextureInfo source = {
      .texture = src.getWGPUTexture(),
      .mipLevel = src.getBaseMipLevel() + range.mipLevel,
      .origin = {.x = range.x, .y = range.y, .z = src.getWGPULayer(range.layer, range.face)},
      .aspect = WGPUTextureAspect_All,
  };
  const WGPUTexelCopyTextureInfo destination = {
      .texture = dst.getWGPUTexture(),
      .mipLevel = dst.getBaseMipLevel() + range.mipLevel,
      .origin = {.x = range.x, .y = range.y, .z = dst.getWGPULayer(range.layer, range.face)},
      .aspect = WGPUTextureAspect_All,
  };
  const WGPUExtent3D extent = {
      .width = range.width, .height = range.height, .depthOrArrayLayers = 1};

  ctx_.pushErrorScope(WGPUErrorFilter_Validation);
  const Handle<WGPUCommandEncoder> encoder(
      wgpuDeviceCreateCommandEncoder(ctx_.getDevice(), nullptr));
  wgpuCommandEncoderCopyTextureToTexture(encoder.get(), &source, &destination, &extent);
  const Handle<WGPUCommandBuffer> commands(wgpuCommandEncoderFinish(encoder.get(), nullptr));
  const WGPUCommandBuffer rawCommands = commands.get();
  wgpuQueueSubmit(ctx_.getQueue(), 1, &rawCommands);
  const Result result = ctx_.popErrorScope();
  if (!result.isOk()) {
    IGL_LOG_ERROR("copyTextureColorAttachment(): %s\n", result.message.c_str());
  }
}

void Framebuffer::updateDrawable(std::shared_ptr<ITexture> texture) {
  if (getColorAttachment(0) != texture) {
    if (!texture) {
      desc_.colorAttachments[0] = {};
    } else {
      desc_.colorAttachments[0].texture = std::move(texture);
    }
  }
}

void Framebuffer::updateDrawable(SurfaceTextures surfaceTextures) {
  updateDrawable(std::move(surfaceTextures.color));
  if (surfaceTextures.depth && surfaceTextures.depth->getProperties().hasStencil()) {
    desc_.stencilAttachment.texture = surfaceTextures.depth;
  } else {
    desc_.stencilAttachment.texture = nullptr;
  }
  desc_.depthAttachment.texture = std::move(surfaceTextures.depth);
  updateResolveAttachment(std::move(surfaceTextures.colorResolve));
}

void Framebuffer::updateResolveAttachment(std::shared_ptr<ITexture> texture) {
  desc_.colorAttachments[0].resolveTexture = std::move(texture);
}

} // namespace igl::webgpu
