/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/PlatformDevice.h>

#include <igl/opengl/DestructionGuard.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/Framebuffer.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/SamplerState.h>
#include <igl/opengl/TextureBufferExternal.h>

namespace igl::opengl {

std::shared_ptr<Framebuffer> PlatformDevice::createFramebuffer(const FramebufferDesc& desc,
                                                               Result* outResult) const {
  auto resource = std::make_shared<CustomFramebuffer>(getContext());
  resource->initialize(desc, outResult);
  if (auto resourceTracker = owner_.getResourceTracker()) {
    resource->initResourceTracker(resourceTracker);
  }
  return resource;
}

std::shared_ptr<Framebuffer> PlatformDevice::createCurrentFramebuffer() const {
  auto resource = std::make_shared<CurrentFramebuffer>(getContext());
  if (auto resourceTracker = owner_.getResourceTracker()) {
    resource->initResourceTracker(resourceTracker);
  }
  return resource;
}

std::unique_ptr<TextureBufferExternal> PlatformDevice::createTextureBufferExternal(
    GLuint textureID,
    GLenum target,
    TextureDesc::TextureUsage usage,
    GLsizei width,
    GLsizei height,
    TextureFormat format,
    GLsizei numLayers) const {
  auto textureBuffer = std::make_unique<TextureBufferExternal>(getContext(), format);
  textureBuffer->setTextureBufferProperties(textureID, target);
  textureBuffer->setUsage(usage);
  textureBuffer->setTextureProperties(width, height, numLayers);
  if (auto resourceTracker = owner_.getResourceTracker()) {
    textureBuffer->initResourceTracker(resourceTracker);
  }
  return textureBuffer;
}

DestructionGuard PlatformDevice::getDestructionGuard() const {
  return {owner_.getSharedContext()};
}

IContext& PlatformDevice::getContext() const {
  return owner_.getContext();
}

const std::shared_ptr<IContext>& PlatformDevice::getSharedContext() const {
  return owner_.getSharedContext();
}

void PlatformDevice::blitFramebuffer(const std::shared_ptr<IFramebuffer>& src,
                                     int srcLeft,
                                     int srcTop,
                                     int srcRight,
                                     int srcBottom,
                                     const std::shared_ptr<IFramebuffer>& dst,
                                     int dstLeft,
                                     int dstTop,
                                     int dstRight,
                                     int dstBottom,
                                     GLbitfield mask,
                                     IContext& ctx,
                                     Result* outResult) {
  auto& from = static_cast<Framebuffer&>(*src);
  auto& to = static_cast<Framebuffer&>(*dst);

#if IGL_DEBUG
  // Guard against depth/stencil type mismatch:
  // GL_INVALID_OPERATION is generated if mask contains GL_DEPTH_BUFFER_BIT or GL_STENCIL_BUFFER_BIT
  // and the source and destination depth and stencil formats do not match.
  if (mask & GL_DEPTH_BUFFER_BIT) {
    auto srcDepthTexture = src->getDepthAttachment();
    auto dstDepthTexture = dst->getDepthAttachment();
    if ((!srcDepthTexture && dstDepthTexture) || (srcDepthTexture && !dstDepthTexture)) {
      IGL_ASSERT_MSG(0,
                     "PlatformDevice::blitFramebuffer: One framebuffer has depth attachment and "
                     "the other doesn't.\n");
    }
    if (srcDepthTexture && dstDepthTexture) {
      auto srcFormat = std::static_pointer_cast<igl::opengl::Texture>(srcDepthTexture)
                           ->getGLInternalTextureFormat();
      auto dstFormat = std::static_pointer_cast<igl::opengl::Texture>(dstDepthTexture)
                           ->getGLInternalTextureFormat();
      if (srcFormat != dstFormat) {
        IGL_ASSERT_MSG(0,
                       "PlatformDevice::blitFramebuffer: Mismatch of framebuffer depth attachment "
                       "formats: %d vs %d\n",
                       srcFormat,
                       dstFormat);
      }
    }
  }

  if (mask & GL_STENCIL_BUFFER_BIT) {
    auto srcStencilTexture = src->getStencilAttachment();
    auto dstStencilTexture = dst->getStencilAttachment();
    if ((!srcStencilTexture && dstStencilTexture) || (srcStencilTexture && !dstStencilTexture)) {
      IGL_ASSERT_MSG(0,
                     "PlatformDevice::blitFramebuffer: One framebuffer has stencil attachment and "
                     "the other doesn't.\n");
    }
    if (srcStencilTexture && dstStencilTexture) {
      auto srcFormat = std::static_pointer_cast<igl::opengl::Texture>(srcStencilTexture)
                           ->getGLInternalTextureFormat();
      auto dstFormat = std::static_pointer_cast<igl::opengl::Texture>(dstStencilTexture)
                           ->getGLInternalTextureFormat();
      if (srcFormat != dstFormat) {
        IGL_ASSERT_MSG(0,
                       "PlatformDevice::blitFramebuffer: Mismatch of framebuffer stencil "
                       "attachment formats: %d vs %d\n",
                       srcFormat,
                       dstFormat);
      }
    }
  }
#endif

  if (ctx.deviceFeatures().hasInternalFeature(InternalFeatures::FramebufferBlit)) {
    const FramebufferBindingGuard guard(ctx);
    ctx.bindFramebuffer(GL_DRAW_FRAMEBUFFER, to.getId());
    ctx.bindFramebuffer(GL_READ_FRAMEBUFFER, from.getId());

    ctx.blitFramebuffer(srcLeft,
                        srcTop,
                        srcRight,
                        srcBottom,
                        dstLeft,
                        dstTop,
                        dstRight,
                        dstBottom,
                        mask,
                        GL_NEAREST);
    Result::setResult(outResult, Result::Code::Ok);
  } else {
    Result::setResult(outResult, Result::Code::Unsupported);
  }
}

void PlatformDevice::blitFramebuffer(const std::shared_ptr<IFramebuffer>& src,
                                     int srcLeft,
                                     int srcTop,
                                     int srcRight,
                                     int srcBottom,
                                     const std::shared_ptr<IFramebuffer>& dst,
                                     int dstLeft,
                                     int dstTop,
                                     int dstRight,
                                     int dstBottom,
                                     GLbitfield mask,
                                     Result* outResult) const {
  auto ctx = getSharedContext();
  igl::opengl::PlatformDevice::blitFramebuffer(src,
                                               srcLeft,
                                               srcTop,
                                               srcRight,
                                               srcBottom,
                                               dst,
                                               dstLeft,
                                               dstTop,
                                               dstRight,
                                               dstBottom,
                                               mask,
                                               *ctx,
                                               outResult);
}

} // namespace igl::opengl
