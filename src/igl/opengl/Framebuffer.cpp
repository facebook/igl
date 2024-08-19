/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/Framebuffer.h>

#include <cstdlib>
#include <igl/RenderPass.h>
#include <igl/opengl/CommandBuffer.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/DummyTexture.h>
#include <igl/opengl/Errors.h>

#include <algorithm>
#if !IGL_PLATFORM_ANDROID
#include <string>
#else
#include <sstream>

namespace std {

// TODO: Remove once STL in Android NDK supports std::to_string
template<typename T>
string to_string(const T& t) {
  ostringstream os;
  os << t;
  return os.str();
}

} // namespace std

#endif

namespace igl::opengl {

namespace {

Result checkFramebufferStatus(IContext& context, bool read) {
  auto code = Result::Code::Ok;
  std::string message;
  GLenum framebufferTarget = GL_FRAMEBUFFER;
  if (context.deviceFeatures().hasFeature(DeviceFeatures::ReadWriteFramebuffer)) {
    framebufferTarget = read ? GL_READ_FRAMEBUFFER : GL_DRAW_FRAMEBUFFER;
  }
  // check that we've created a proper frame buffer
  const GLenum status = context.checkFramebufferStatus(framebufferTarget);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    code = Result::Code::RuntimeError;

    switch (status) {
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
      message = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
      message = "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
      message = "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
      break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
      message = "GL_FRAMEBUFFER_UNSUPPORTED";
      break;
    default:
      message = "GL_FRAMEBUFFER unknown error: " + std::to_string(status);
      break;
    }
  }

  return Result(code, message);
}

Texture::AttachmentParams toAttachmentParams(const RenderPassDesc::BaseAttachmentDesc& attachment,
                                             FramebufferMode mode) {
  Texture::AttachmentParams params{};
  params.face = attachment.face;
  params.mipLevel = attachment.mipLevel;
  params.layer = attachment.layer;
  params.read = false; // Color attachments are for writing
  params.stereo = mode == FramebufferMode::Stereo;
  return params;
}

Texture::AttachmentParams defaultWriteAttachmentParams(FramebufferMode mode) {
  Texture::AttachmentParams params{};
  params.face = 0;
  params.mipLevel = 0;
  params.layer = 0;
  params.read = false;
  params.stereo = mode == FramebufferMode::Stereo;
  return params;
}

Texture::AttachmentParams toReadAttachmentParams(const TextureRangeDesc& range,
                                                 FramebufferMode mode) {
  IGL_ASSERT_MSG(range.numLayers == 1, "range.numLayers must be 1.");
  IGL_ASSERT_MSG(range.numMipLevels == 1, "range.numMipLevels must be 1.");
  IGL_ASSERT_MSG(range.numFaces == 1, "range.numFaces must be 1.");

  Texture::AttachmentParams params{};
  params.face = static_cast<uint32_t>(range.face);
  params.mipLevel = static_cast<uint32_t>(range.mipLevel);
  params.layer = static_cast<uint32_t>(range.layer);
  params.read = true;
  params.stereo = mode == FramebufferMode::Stereo;
  return params;
}
} // namespace

FramebufferBindingGuard::FramebufferBindingGuard(IContext& context) :
  context_(context),
  currentRenderbuffer_(0),
  currentFramebuffer_(0),
  currentReadFramebuffer_(0),
  currentDrawFramebuffer_(0) {
  context_.getIntegerv(GL_RENDERBUFFER_BINDING, reinterpret_cast<GLint*>(&currentRenderbuffer_));

  // Only restore currently bound framebuffer if it's valid
  if (context.deviceFeatures().hasFeature(DeviceFeatures::ReadWriteFramebuffer)) {
    if (checkFramebufferStatus(context, true).isOk()) {
      context_.getIntegerv(GL_READ_FRAMEBUFFER_BINDING,
                           reinterpret_cast<GLint*>(&currentReadFramebuffer_));
    }
    if (checkFramebufferStatus(context, false).isOk()) {
      context_.getIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,
                           reinterpret_cast<GLint*>(&currentDrawFramebuffer_));
    }
  } else {
    if (checkFramebufferStatus(context, false).isOk()) {
      context_.getIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&currentFramebuffer_));
    }
  }
}

FramebufferBindingGuard::~FramebufferBindingGuard() {
  if (context_.deviceFeatures().hasFeature(DeviceFeatures::ReadWriteFramebuffer)) {
    context_.bindFramebuffer(GL_READ_FRAMEBUFFER, currentReadFramebuffer_);
    context_.bindFramebuffer(GL_DRAW_FRAMEBUFFER, currentDrawFramebuffer_);
  } else {
    context_.bindFramebuffer(GL_FRAMEBUFFER, currentFramebuffer_);
  }

  context_.bindRenderbuffer(GL_RENDERBUFFER, currentRenderbuffer_);
}

///--------------------------------------
/// MARK: - Framebuffer

Framebuffer::Framebuffer(IContext& context) : WithContext(context) {}

bool Framebuffer::isSwapchainBound() const {
  return frameBufferID_ == 0;
}

void Framebuffer::attachAsColor(igl::ITexture& texture,
                                uint32_t index,
                                const Texture::AttachmentParams& params) const {
  static_cast<Texture&>(texture).attachAsColor(index, params);
  IGL_ASSERT(index >= 0 && index < kNumCachedStates);
  colorCachedState_[index].updateCache(params.stereo ? FramebufferMode::Stereo
                                                     : FramebufferMode::Mono,
                                       params.layer,
                                       params.face,
                                       params.mipLevel);
}

void Framebuffer::attachAsDepth(igl::ITexture& texture,
                                const Texture::AttachmentParams& params) const {
  static_cast<Texture&>(texture).attachAsDepth(params);
  depthCachedState_.updateCache(params.stereo ? FramebufferMode::Stereo : FramebufferMode::Mono,
                                params.layer,
                                params.face,
                                params.mipLevel);
}

void Framebuffer::attachAsStencil(igl::ITexture& texture,
                                  const Texture::AttachmentParams& params) const {
  static_cast<Texture&>(texture).attachAsStencil(params);
  stencilCachedState_.updateCache(params.stereo ? FramebufferMode::Stereo : FramebufferMode::Mono,
                                  params.layer,
                                  params.face,
                                  params.mipLevel);
}

void Framebuffer::bindBuffer() const {
  getContext().bindFramebuffer(GL_FRAMEBUFFER, frameBufferID_);
}

void Framebuffer::bindBufferForRead() const {
  // TODO: enable optimization path
  if (getContext().deviceFeatures().hasFeature(DeviceFeatures::ReadWriteFramebuffer)) {
    getContext().bindFramebuffer(GL_READ_FRAMEBUFFER, frameBufferID_);
  } else {
    bindBuffer();
  }
}

void Framebuffer::copyBytesColorAttachment(ICommandQueue& /* unused */,
                                           size_t index,
                                           void* pixelBytes,
                                           const TextureRangeDesc& range,
                                           size_t bytesPerRow) const {
  // Only support attachment 0 because that's what glReadPixels supports
  if (index != 0) {
    IGL_ASSERT_MSG(0, "Invalid index: %d", index);
    return;
  }
  IGL_ASSERT_MSG(range.numFaces == 1, "range.numFaces MUST be 1");
  IGL_ASSERT_MSG(range.numLayers == 1, "range.numLayers MUST be 1");
  IGL_ASSERT_MSG(range.numMipLevels == 1, "range.numMipLevels MUST be 1");

  auto itexture = getColorAttachment(index);
  if (itexture == nullptr) {
    IGL_ASSERT_MSG(0, "The framebuffer does not have any color attachment at index %d", index);
    return;
  }

  const FramebufferBindingGuard guard(getContext());

  CustomFramebuffer extraFramebuffer(getContext());

  auto& texture = static_cast<igl::opengl::Texture&>(*itexture);

  Result ret;
  FramebufferDesc desc;
  desc.colorAttachments[0].texture = itexture;
  extraFramebuffer.initialize(desc, &ret);
  IGL_ASSERT_MSG(ret.isOk(), ret.message.c_str());

  extraFramebuffer.bindBufferForRead();
  attachAsColor(*itexture, 0, toReadAttachmentParams(range, FramebufferMode::Mono));
  checkFramebufferStatus(getContext(), true);

  const bool packRowLengthSupported =
      getContext().deviceFeatures().hasInternalFeature(InternalFeatures::PackRowLength);
  // The bytesPerRow value is used to decide both the alignment and the row length. We will only use
  // usePackRowLength when bytesPerRow is set and is a multiple of the block size.
  const bool usePackRowLength = packRowLengthSupported && bytesPerRow != 0 &&
                                bytesPerRow % itexture->getProperties().bytesPerBlock == 0;

  if (usePackRowLength) {
    const int packRowLength =
        static_cast<int>(bytesPerRow / itexture->getProperties().bytesPerBlock);
    getContext().pixelStorei(GL_PACK_ROW_LENGTH, packRowLength);
    getContext().pixelStorei(GL_PACK_ALIGNMENT, 1);
  } else {
    const int finalBytesPerRow = bytesPerRow == 0 ? itexture->getProperties().getBytesPerRow(range)
                                                  : bytesPerRow;
    if (packRowLengthSupported) {
      getContext().pixelStorei(GL_PACK_ROW_LENGTH, 0);
    }
    getContext().pixelStorei(GL_PACK_ALIGNMENT,
                             texture.getAlignment(finalBytesPerRow, range.mipLevel));
  }

  // Note read out format is based on
  // (https://www.khronos.org/registry/OpenGL-Refpages/es2.0/xhtml/glReadPixels.xml)
  // as using GL_RGBA with GL_UNSIGNED_BYTE is the only always supported combination
  // with glReadPixels.
  getContext().flush();

  // @fb-only
  // @fb-only
  const auto rangeX = static_cast<GLint>(range.x);
  const auto rangeY = static_cast<GLint>(range.y);
  const auto rangeWidth = static_cast<GLsizei>(range.width);
  const auto rangeHeight = static_cast<GLsizei>(range.height);
  const auto textureFormat = texture.getFormat();
  // Tests need GL_HALF_FLOAT_OES on iOS and GL_HALF_FLOAT on Android and everything else.
  const auto kHalfFloatFormat = getContext().deviceFeatures().hasInternalRequirement(
                                    InternalRequirement::TextureHalfFloatExtReq)
                                    ? GL_HALF_FLOAT_OES
                                    : GL_HALF_FLOAT;
  if (textureFormat == TextureFormat::RGBA_UInt32) {
    if (IGL_VERIFY(
            getContext().deviceFeatures().hasTextureFeature(TextureFeatures::TextureInteger))) {
      getContext().readPixels(
          rangeX, rangeY, rangeWidth, rangeHeight, GL_RGBA_INTEGER, GL_UNSIGNED_INT, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::R_UNorm8) {
    if (IGL_VERIFY(getContext().deviceFeatures().hasFeature(DeviceFeatures::TextureFormatRG))) {
      getContext().readPixels(
          rangeX, rangeY, rangeWidth, rangeHeight, GL_RED, GL_UNSIGNED_BYTE, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::RG_UNorm8) {
    if (IGL_VERIFY(getContext().deviceFeatures().hasFeature(DeviceFeatures::TextureFormatRG))) {
      getContext().readPixels(
          rangeX, rangeY, rangeWidth, rangeHeight, GL_RG, GL_UNSIGNED_BYTE, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::RGBA_F16) {
    if (IGL_VERIFY(getContext().deviceFeatures().hasFeature(DeviceFeatures::TextureHalfFloat))) {
      getContext().readPixels(
          rangeX, rangeY, rangeWidth, rangeHeight, GL_RGBA, kHalfFloatFormat, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::RGB_F16) {
    if (IGL_VERIFY(getContext().deviceFeatures().hasFeature(DeviceFeatures::TextureHalfFloat))) {
      getContext().readPixels(
          rangeX, rangeY, rangeWidth, rangeHeight, GL_RGB, kHalfFloatFormat, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::RG_F16) {
    if (IGL_VERIFY(getContext().deviceFeatures().hasFeature(DeviceFeatures::TextureHalfFloat)) &&
        IGL_VERIFY(getContext().deviceFeatures().hasFeature(DeviceFeatures::TextureFormatRG))) {
      getContext().readPixels(
          rangeX, rangeY, rangeWidth, rangeHeight, GL_RG, kHalfFloatFormat, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::R_F16) {
    if (IGL_VERIFY(getContext().deviceFeatures().hasFeature(DeviceFeatures::TextureHalfFloat)) &&
        IGL_VERIFY(getContext().deviceFeatures().hasFeature(DeviceFeatures::TextureFormatRG))) {
      getContext().readPixels(
          rangeX, rangeY, rangeWidth, rangeHeight, GL_RED, kHalfFloatFormat, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::RGBA_F32) {
    if (IGL_VERIFY(getContext().deviceFeatures().hasFeature(DeviceFeatures::TextureFloat))) {
      getContext().readPixels(
          rangeX, rangeY, rangeWidth, rangeHeight, GL_RGBA, GL_FLOAT, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::RGB_F32) {
    if (IGL_VERIFY(getContext().deviceFeatures().hasFeature(DeviceFeatures::TextureFloat))) {
      getContext().readPixels(
          rangeX, rangeY, rangeWidth, rangeHeight, GL_RGB, GL_FLOAT, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::RG_F32) {
    if (IGL_VERIFY(getContext().deviceFeatures().hasFeature(DeviceFeatures::TextureFloat)) &&
        IGL_VERIFY(getContext().deviceFeatures().hasFeature(DeviceFeatures::TextureFormatRG))) {
      getContext().readPixels(rangeX, rangeY, rangeWidth, rangeHeight, GL_RG, GL_FLOAT, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::R_F32) {
    if (IGL_VERIFY(getContext().deviceFeatures().hasFeature(DeviceFeatures::TextureFloat)) &&
        IGL_VERIFY(getContext().deviceFeatures().hasFeature(DeviceFeatures::TextureFormatRG))) {
      getContext().readPixels(
          rangeX, rangeY, rangeWidth, rangeHeight, GL_RED, GL_FLOAT, pixelBytes);
    }
  } else {
    getContext().readPixels(
        rangeX, rangeY, rangeWidth, rangeHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixelBytes);
  }

  // Reset the GL_PACK_ROW_LENGTH
  if (usePackRowLength) {
    getContext().pixelStorei(GL_PACK_ROW_LENGTH, 0);
  }
  // Reset the GL_PACK_ALIGNMENT (default value for GL_PACK_ALIGNMENT is 4)
  getContext().pixelStorei(GL_PACK_ALIGNMENT, 4);

  getContext().checkForErrors(nullptr, 0);
  auto error = getContext().getLastError();
  IGL_ASSERT_MSG(error.isOk(), error.message.c_str());
}

void Framebuffer::copyBytesDepthAttachment(ICommandQueue& /* unused */,
                                           void* /*pixelBytes*/,
                                           const TextureRangeDesc& /*range*/,
                                           size_t /*bytesPerRow*/) const {
  IGL_ASSERT_NOT_IMPLEMENTED();
}

void Framebuffer::copyBytesStencilAttachment(ICommandQueue& /* unused */,
                                             void* /*pixelBytes*/,
                                             const TextureRangeDesc& /*range*/,
                                             size_t /*bytesPerRow*/) const {
  IGL_ASSERT_NOT_IMPLEMENTED();
}

void Framebuffer::copyTextureColorAttachment(ICommandQueue& /*cmdQueue*/,
                                             size_t index,
                                             std::shared_ptr<ITexture> destTexture,
                                             const TextureRangeDesc& range) const {
  // Only support attachment 0 because that's what glCopyTexImage2D supports
  if (index != 0 || getColorAttachment(index) == nullptr) {
    IGL_ASSERT_MSG(0, "Invalid index: %d", index);
    return;
  }

  const FramebufferBindingGuard guard(getContext());

  bindBufferForRead();

  auto& dest = static_cast<Texture&>(*destTexture);
  dest.bind();

  getContext().copyTexSubImage2D(GL_TEXTURE_2D,
                                 0,
                                 0,
                                 0,
                                 static_cast<GLint>(range.x),
                                 static_cast<GLint>(range.y),
                                 static_cast<GLsizei>(range.width),
                                 static_cast<GLsizei>(range.height));
}

bool Framebuffer::CachedState::needsUpdate(FramebufferMode mode,
                                           uint8_t layer,
                                           uint8_t face,
                                           uint8_t mipLevel) {
  return mode_ != mode || layer_ != layer || face_ != face || mipLevel_ != mipLevel;
}

void Framebuffer::CachedState::updateCache(FramebufferMode mode,
                                           uint8_t layer,
                                           uint8_t face,
                                           uint8_t mipLevel) {
  mode_ = mode;
  layer_ = layer;
  face_ = face;
  mipLevel_ = mipLevel;
}

///--------------------------------------
/// MARK: - CustomFramebuffer

CustomFramebuffer::~CustomFramebuffer() {
  if (frameBufferID_ != 0) {
    getContext().deleteFramebuffers(1, &frameBufferID_);
    frameBufferID_ = 0;
  }
}

std::vector<size_t> CustomFramebuffer::getColorAttachmentIndices() const {
  std::vector<size_t> indices;

  for (size_t i = 0; i != IGL_COLOR_ATTACHMENTS_MAX; i++) {
    if (renderTarget_.colorAttachments[i].texture ||
        renderTarget_.colorAttachments[i].resolveTexture) {
      indices.push_back(i);
    }
  }

  return indices;
}

std::shared_ptr<ITexture> CustomFramebuffer::getColorAttachment(size_t index) const {
  IGL_ASSERT(index < IGL_COLOR_ATTACHMENTS_MAX);
  return renderTarget_.colorAttachments[index].texture;
}

std::shared_ptr<ITexture> CustomFramebuffer::getResolveColorAttachment(size_t index) const {
  IGL_ASSERT(index < IGL_COLOR_ATTACHMENTS_MAX);
  return renderTarget_.colorAttachments[index].resolveTexture;
}

std::shared_ptr<ITexture> CustomFramebuffer::getDepthAttachment() const {
  return renderTarget_.depthAttachment.texture;
}

std::shared_ptr<ITexture> CustomFramebuffer::getResolveDepthAttachment() const {
  return renderTarget_.depthAttachment.resolveTexture;
}

std::shared_ptr<ITexture> CustomFramebuffer::getStencilAttachment() const {
  return renderTarget_.stencilAttachment.texture;
}

FramebufferMode CustomFramebuffer::getMode() const {
  return renderTarget_.mode;
}

void CustomFramebuffer::updateDrawable(std::shared_ptr<ITexture> texture) {
  updateDrawableInternal({std::move(texture), nullptr}, false);
}

void CustomFramebuffer::updateDrawable(SurfaceTextures surfaceTextures) {
  updateDrawableInternal(std::move(surfaceTextures), true);
}

void CustomFramebuffer::updateResolveAttachment(std::shared_ptr<ITexture> texture) {
  if (resolveFramebuffer) {
    resolveFramebuffer->updateDrawable(texture);
  }
}

void CustomFramebuffer::updateDrawableInternal(SurfaceTextures surfaceTextures,
                                               bool updateDepthStencil) {
  auto colorAttachment0 = getColorAttachment(0);
  auto depthAttachment = updateDepthStencil ? getDepthAttachment() : nullptr;
  auto stencilAttachment = updateDepthStencil ? getStencilAttachment() : nullptr;

  const bool updateColor = colorAttachment0 != surfaceTextures.color;
  updateDepthStencil = updateDepthStencil && (depthAttachment != surfaceTextures.depth ||
                                              stencilAttachment != surfaceTextures.depth);
  if (updateColor || updateDepthStencil) {
    const FramebufferBindingGuard guard(getContext());
    bindBuffer();
    if (updateColor) {
      if (!surfaceTextures.color) {
        static_cast<Texture&>(*colorAttachment0).detachAsColor(0, false);
        renderTarget_.colorAttachments[0] = {};
      } else {
        attachAsColor(*surfaceTextures.color, 0, defaultWriteAttachmentParams(renderTarget_.mode));

        renderTarget_.colorAttachments[0].texture = std::move(surfaceTextures.color);
      }
    }
    if (updateDepthStencil) {
      if (!surfaceTextures.depth) {
        if (depthAttachment) {
          static_cast<Texture&>(*depthAttachment).detachAsDepth(false);
        }
        renderTarget_.depthAttachment.texture = nullptr;

        if (depthAttachment == stencilAttachment) {
          if (stencilAttachment) {
            static_cast<Texture&>(*stencilAttachment).detachAsStencil(false);
          }
          renderTarget_.stencilAttachment.texture = nullptr;
        }
      } else {
        attachAsDepth(*surfaceTextures.depth, defaultWriteAttachmentParams(renderTarget_.mode));
        if (surfaceTextures.depth->getProperties().hasStencil()) {
          attachAsStencil(*surfaceTextures.depth, defaultWriteAttachmentParams(renderTarget_.mode));
          renderTarget_.stencilAttachment.texture = surfaceTextures.depth;
        } else {
          if (stencilAttachment) {
            static_cast<Texture&>(*stencilAttachment).detachAsStencil(false);
          }
          renderTarget_.stencilAttachment.texture = nullptr;
        }
        renderTarget_.depthAttachment.texture = std::move(surfaceTextures.depth);
      }
    }
  }
}

bool CustomFramebuffer::isInitialized() const {
  return initialized_;
}

bool CustomFramebuffer::hasImplicitColorAttachment() const {
  if (frameBufferID_ != 0) {
    return false;
  }

  const auto& colorAttachment0 = renderTarget_.colorAttachments[0];

  return colorAttachment0.texture != nullptr &&
         static_cast<Texture&>(*colorAttachment0.texture).isImplicitStorage();
}

void CustomFramebuffer::initialize(const FramebufferDesc& desc, Result* outResult) {
  if (IGL_UNEXPECTED(isInitialized())) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Framebuffer already initialized.");
    return;
  }
  initialized_ = true;

  renderTarget_ = desc;

  // Restore framebuffer binding
  const FramebufferBindingGuard guard(getContext());
  if (!desc.debugName.empty() &&
      getContext().deviceFeatures().hasInternalFeature(InternalFeatures::DebugLabel)) {
    getContext().objectLabel(
        GL_FRAMEBUFFER, frameBufferID_, desc.debugName.size(), desc.debugName.c_str());
  }
  if (hasImplicitColorAttachment()) {
    // Don't generate framebuffer id. Use implicit framebuffer supplied by containing view
    Result::setOk(outResult);
  } else {
    prepareResource(outResult);
  }
}

void CustomFramebuffer::prepareResource(Result* outResult) {
  // create a new frame buffer if we don't already have one
  getContext().genFramebuffers(1, &frameBufferID_);
  if (IGL_UNEXPECTED(frameBufferID_ == 0)) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Failed to create framebuffer ID.");
    return;
  }

  bindBuffer();

  std::vector<GLenum> drawBuffers;

  const auto attachmentParams = defaultWriteAttachmentParams(renderTarget_.mode);
  // attach the textures and render buffers to the frame buffer
  for (size_t i = 0; i != IGL_COLOR_ATTACHMENTS_MAX; i++) {
    const auto& colorAttachment = renderTarget_.colorAttachments[i];
    if (colorAttachment.texture != nullptr) {
      attachAsColor(*colorAttachment.texture, static_cast<uint32_t>(i), attachmentParams);
      drawBuffers.push_back(static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i));
    }
  }

  std::sort(drawBuffers.begin(), drawBuffers.end());

  if (drawBuffers.size() > 1) {
    getContext().drawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
  }

  if (renderTarget_.depthAttachment.texture != nullptr) {
    attachAsDepth(*renderTarget_.depthAttachment.texture, attachmentParams);
  }

  if (renderTarget_.stencilAttachment.texture != nullptr) {
    attachAsStencil(*renderTarget_.stencilAttachment.texture, attachmentParams);
  }

  Result result = checkFramebufferStatus(getContext(), false);
  IGL_ASSERT_MSG(result.isOk(), result.message.c_str());
  if (outResult) {
    *outResult = result;
  }
  if (!result.isOk()) {
    return;
  }

  // Check if resolve framebuffer is needed
  FramebufferDesc resolveDesc;
  auto createResolveFramebuffer = false;
  uint32_t maskColorAttachments = 0;
  uint32_t maskColorResolveAttachments = 0;
  for (size_t i = 0; i != IGL_COLOR_ATTACHMENTS_MAX; i++) {
    const auto& colorAttachment = renderTarget_.colorAttachments[i];
    if (colorAttachment.texture) {
      maskColorAttachments |= 1u << i;
    }
    if (colorAttachment.resolveTexture) {
      createResolveFramebuffer = true;
      FramebufferDesc::AttachmentDesc attachment;
      attachment.texture = colorAttachment.resolveTexture;
      resolveDesc.colorAttachments[i] = attachment;
      maskColorResolveAttachments |= 1u << i;
    }
  }
  if (createResolveFramebuffer && maskColorResolveAttachments != maskColorAttachments) {
    IGL_ASSERT_NOT_REACHED();
    if (outResult) {
      *outResult = igl::Result(igl::Result::Code::ArgumentInvalid,
                               "If resolve texture is specified on a color attachment it must be "
                               "specified on all of them");
    }
    return;
  }

  if (renderTarget_.depthAttachment.resolveTexture) {
    createResolveFramebuffer = true;
    resolveDesc.depthAttachment.texture = renderTarget_.depthAttachment.resolveTexture;
  }
  if (renderTarget_.stencilAttachment.resolveTexture) {
    createResolveFramebuffer = true;
    resolveDesc.stencilAttachment.texture = renderTarget_.stencilAttachment.resolveTexture;
  }

  if (createResolveFramebuffer) {
    auto cfb = std::make_shared<CustomFramebuffer>(getContext());
    cfb->initialize(resolveDesc, &result);
    if (outResult) {
      *outResult = result;
    }
    resolveFramebuffer = std::move(cfb);
  }
}

Viewport CustomFramebuffer::getViewport() const {
  auto texture = getColorAttachment(0);

  if (texture == nullptr) {
    texture = getDepthAttachment();
  }

  if (texture == nullptr) {
    IGL_ASSERT_MSG(0, "No color/depth attachments in CustomFrameBuffer at index 0");
    return {0, 0, 0, 0};
  }

  // By default, we set viewport to dimensions of framebuffer
  const auto size = texture->getSize();
  return {0, 0, size.width, size.height};
}

void CustomFramebuffer::bind(const RenderPassDesc& renderPass) const {
  // Cache renderPass for unbind
  renderPass_ = renderPass;
  IGL_ASSERT_MSG(renderTarget_.mode != FramebufferMode::Multiview,
                 "FramebufferMode::Multiview not supported");

  bindBuffer();

  for (size_t i = 0; i != IGL_COLOR_ATTACHMENTS_MAX; i++) {
    const auto& colorAttachment = renderTarget_.colorAttachments[i];
    if (!colorAttachment.texture) {
      continue;
    }
#if !IGL_OPENGL_ES
    // OpenGL ES doesn't need to call glEnable. All it needs is an sRGB framebuffer.
    if (getContext().deviceFeatures().hasFeature(DeviceFeatures::SRGB)) {
      if (colorAttachment.texture->getProperties().isSRGB()) {
        getContext().enable(GL_FRAMEBUFFER_SRGB);
      } else {
        getContext().disable(GL_FRAMEBUFFER_SRGB);
      }
    }
#endif
    const size_t index = i;
    IGL_ASSERT(index >= 0 && index < renderPass.colorAttachments.size());
    const auto& renderPassAttachment = renderPass.colorAttachments[index];
    // When setting up a framebuffer, we attach textures as though they were a non-array
    // texture with and set layer, mip-level and face equal to 0.
    // If any of these assumptions are not true, we need to reattach with proper values.
    IGL_ASSERT(index >= 0 && index < kNumCachedStates);
    if (colorCachedState_[index].needsUpdate(renderTarget_.mode,
                                             renderPassAttachment.layer,
                                             renderPassAttachment.face,
                                             renderPassAttachment.mipLevel)) {
      attachAsColor(*colorAttachment.texture,
                    static_cast<uint32_t>(index),
                    toAttachmentParams(renderPassAttachment, renderTarget_.mode));
    }
  }
  if (renderTarget_.depthAttachment.texture) {
    const auto& renderPassAttachment = renderPass.depthAttachment;
    if (depthCachedState_.needsUpdate(renderTarget_.mode,
                                      renderPassAttachment.layer,
                                      renderPassAttachment.face,
                                      renderPassAttachment.mipLevel)) {
      attachAsDepth(*renderTarget_.depthAttachment.texture,
                    toAttachmentParams(renderPassAttachment, renderTarget_.mode));
    }
  }
  if (renderTarget_.stencilAttachment.texture) {
    const auto& renderPassAttachment = renderPass.stencilAttachment;
    if (stencilCachedState_.needsUpdate(renderTarget_.mode,
                                        renderPassAttachment.layer,
                                        renderPassAttachment.face,
                                        renderPassAttachment.mipLevel)) {
      attachAsStencil(*renderTarget_.stencilAttachment.texture,
                      toAttachmentParams(renderPassAttachment, renderTarget_.mode));
    }
  }
  // clear the buffers if we're not loading previous contents
  GLbitfield clearMask = 0;
  const auto& colorAttachment0 = renderTarget_.colorAttachments[0];

  if (colorAttachment0.texture != nullptr &&
      renderPass_.colorAttachments[0].loadAction == LoadAction::Clear) {
    clearMask |= GL_COLOR_BUFFER_BIT;
    auto clearColor = renderPass_.colorAttachments[0].clearColor;
    getContext().colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    getContext().clearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
  }
  if (renderTarget_.depthAttachment.texture != nullptr) {
    if (renderPass_.depthAttachment.loadAction == LoadAction::Clear) {
      clearMask |= GL_DEPTH_BUFFER_BIT;
      getContext().depthMask(GL_TRUE);
      getContext().clearDepthf(renderPass_.depthAttachment.clearDepth);
    }
  }
  if (renderTarget_.stencilAttachment.texture != nullptr) {
    getContext().enable(GL_STENCIL_TEST);
    if (renderPass_.stencilAttachment.loadAction == LoadAction::Clear) {
      clearMask |= GL_STENCIL_BUFFER_BIT;
      getContext().stencilMask(0xFF);
      getContext().clearStencil(renderPass_.stencilAttachment.clearStencil);
    }
  }

  if (clearMask != 0) {
    getContext().clear(clearMask);
  }
}

void CustomFramebuffer::unbind() const {
  // discard the depthStencil if we don't need to store its contents
  GLenum attachments[3];
  GLsizei numAttachments = 0;
  const auto& colorAttachment0 = renderTarget_.colorAttachments[0];

  if (colorAttachment0.texture != nullptr &&
      renderPass_.colorAttachments[0].storeAction != StoreAction::Store) {
    attachments[numAttachments++] = GL_COLOR_ATTACHMENT0;
  }
  if (renderTarget_.depthAttachment.texture != nullptr) {
    if (renderPass_.depthAttachment.storeAction != StoreAction::Store) {
      attachments[numAttachments++] = GL_DEPTH_ATTACHMENT;
    }
  }
  if (renderTarget_.stencilAttachment.texture != nullptr) {
    getContext().disable(GL_STENCIL_TEST);
    if (renderPass_.stencilAttachment.storeAction != StoreAction::Store) {
      attachments[numAttachments++] = GL_STENCIL_ATTACHMENT;
    }
  }

  if (numAttachments > 0) {
    const auto& features = getContext().deviceFeatures();
    if (features.hasInternalFeature(InternalFeatures::InvalidateFramebuffer)) {
      getContext().invalidateFramebuffer(GL_FRAMEBUFFER, numAttachments, attachments);
    }
  }
}

///--------------------------------------
/// MARK: - CurrentFramebuffer

CurrentFramebuffer::CurrentFramebuffer(IContext& context) : Super(context) {
  getContext().getIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&frameBufferID_));

  GLint viewport[4];
  getContext().getIntegerv(GL_VIEWPORT, viewport);
  viewport_.x = static_cast<float>(viewport[0]);
  viewport_.y = static_cast<float>(viewport[1]);
  viewport_.width = static_cast<float>(viewport[2]);
  viewport_.height = static_cast<float>(viewport[3]);

  colorAttachment_ = std::make_shared<DummyTexture>(Size(viewport_.width, viewport_.height));
}

std::vector<size_t> CurrentFramebuffer::getColorAttachmentIndices() const {
  return std::vector<size_t>{0};
}

std::shared_ptr<ITexture> CurrentFramebuffer::getColorAttachment(size_t index) const {
  if (index != 0) {
    IGL_ASSERT_NOT_REACHED();
  }
  return colorAttachment_;
}

std::shared_ptr<ITexture> CurrentFramebuffer::getResolveColorAttachment(size_t index) const {
  if (index != 0) {
    IGL_ASSERT_NOT_REACHED();
  }
  return colorAttachment_;
}

std::shared_ptr<ITexture> CurrentFramebuffer::getDepthAttachment() const {
  return nullptr;
}

std::shared_ptr<ITexture> CurrentFramebuffer::getResolveDepthAttachment() const {
  return nullptr;
}

std::shared_ptr<ITexture> CurrentFramebuffer::getStencilAttachment() const {
  return nullptr;
}

void CurrentFramebuffer::updateDrawable(std::shared_ptr<ITexture> /*texture*/) {
  IGL_ASSERT_NOT_REACHED();
}

void CurrentFramebuffer::updateDrawable(SurfaceTextures /*surfaceTextures*/) {
  IGL_ASSERT_NOT_REACHED();
}

void CurrentFramebuffer::updateResolveAttachment(std::shared_ptr<ITexture> /*texture*/) {
  IGL_ASSERT_NOT_REACHED();
}

Viewport CurrentFramebuffer::getViewport() const {
  return viewport_;
}

void CurrentFramebuffer::bind(const RenderPassDesc& renderPass) const {
  bindBuffer();
#if !IGL_OPENGL_ES
  // OpenGL ES doesn't need to call glEnable. All it needs is an sRGB framebuffer.
  auto colorAttach = getResolveColorAttachment(getColorAttachmentIndices()[0]);
  if (getContext().deviceFeatures().hasFeature(DeviceFeatures::SRGB)) {
    if (colorAttach && colorAttach->getProperties().isSRGB()) {
      getContext().enable(GL_FRAMEBUFFER_SRGB);
    } else {
      getContext().disable(GL_FRAMEBUFFER_SRGB);
    }
  }
#endif

  // clear the buffers if we're not loading previous contents
  GLbitfield clearMask = 0;
  if (renderPass.colorAttachments[0].loadAction != LoadAction::Load) {
    clearMask |= GL_COLOR_BUFFER_BIT;
    auto clearColor = renderPass.colorAttachments[0].clearColor;
    getContext().colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    getContext().clearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
  }
  if (renderPass.depthAttachment.loadAction != LoadAction::Load) {
    clearMask |= GL_DEPTH_BUFFER_BIT;
    getContext().depthMask(GL_TRUE);
    getContext().clearDepthf(renderPass.depthAttachment.clearDepth);
  }
  if (renderPass.stencilAttachment.loadAction != LoadAction::Load) {
    clearMask |= GL_STENCIL_BUFFER_BIT;
    getContext().stencilMask(0xFF);
    getContext().clearStencil(renderPass.stencilAttachment.clearStencil);
  }

  if (clearMask != 0) {
    getContext().clear(clearMask);
  }
}

void CurrentFramebuffer::unbind() const {
  // no-op
}

FramebufferMode CurrentFramebuffer::getMode() const {
  return FramebufferMode::Mono;
}

} // namespace igl::opengl
