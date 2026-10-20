/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/Framebuffer.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>
#include <igl/Macros.h>
#include <igl/RenderPass.h>
#include <igl/opengl/DeviceFeatureSet.h>
#include <igl/opengl/DummyTexture.h>
#include <igl/opengl/GLIncludes.h>

namespace igl::opengl {

namespace {

/// Read pixels for half-float texture formats (RGBA_F16, RGB_F16, RG_F16, R_F16).
void readPixelsByFormatHalfFloat(IContext& context,
                                 TextureFormat textureFormat,
                                 GLint x,
                                 GLint y,
                                 GLsizei width,
                                 GLsizei height,
                                 void* pixelBytes,
                                 GLenum halfFloatFormat) {
  if (textureFormat == TextureFormat::RGBA_F16) {
    if (IGL_DEBUG_VERIFY(context.deviceFeatures().hasFeature(DeviceFeatures::TextureHalfFloat))) {
      context.readPixels(x, y, width, height, GL_RGBA, halfFloatFormat, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::RGB_F16) {
    if (IGL_DEBUG_VERIFY(context.deviceFeatures().hasFeature(DeviceFeatures::TextureHalfFloat))) {
      context.readPixels(x, y, width, height, GL_RGB, halfFloatFormat, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::RG_F16) {
    if (IGL_DEBUG_VERIFY(context.deviceFeatures().hasFeature(DeviceFeatures::TextureHalfFloat)) &&
        IGL_DEBUG_VERIFY(context.deviceFeatures().hasFeature(DeviceFeatures::TextureFormatRG))) {
      context.readPixels(x, y, width, height, GL_RG, halfFloatFormat, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::R_F16) {
    if (IGL_DEBUG_VERIFY(context.deviceFeatures().hasFeature(DeviceFeatures::TextureHalfFloat)) &&
        IGL_DEBUG_VERIFY(context.deviceFeatures().hasFeature(DeviceFeatures::TextureFormatRG))) {
      context.readPixels(x, y, width, height, GL_RED, halfFloatFormat, pixelBytes);
    }
  }
}

/// Read pixels for float texture formats (RGBA_F32, RGB_F32, RG_F32, R_F32).
void readPixelsByFormatFloat(IContext& context,
                             TextureFormat textureFormat,
                             GLint x,
                             GLint y,
                             GLsizei width,
                             GLsizei height,
                             void* pixelBytes) {
  if (textureFormat == TextureFormat::RGBA_F32) {
    if (IGL_DEBUG_VERIFY(context.deviceFeatures().hasFeature(DeviceFeatures::TextureFloat))) {
      context.readPixels(x, y, width, height, GL_RGBA, GL_FLOAT, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::RGB_F32) {
    if (IGL_DEBUG_VERIFY(context.deviceFeatures().hasFeature(DeviceFeatures::TextureFloat))) {
      context.readPixels(x, y, width, height, GL_RGB, GL_FLOAT, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::RG_F32) {
    if (IGL_DEBUG_VERIFY(context.deviceFeatures().hasFeature(DeviceFeatures::TextureFloat)) &&
        IGL_DEBUG_VERIFY(context.deviceFeatures().hasFeature(DeviceFeatures::TextureFormatRG))) {
      context.readPixels(x, y, width, height, GL_RG, GL_FLOAT, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::R_F32) {
    if (IGL_DEBUG_VERIFY(context.deviceFeatures().hasFeature(DeviceFeatures::TextureFloat)) &&
        IGL_DEBUG_VERIFY(context.deviceFeatures().hasFeature(DeviceFeatures::TextureFormatRG))) {
      context.readPixels(x, y, width, height, GL_RED, GL_FLOAT, pixelBytes);
    }
  }
}

/// Dispatch readPixels() based on textureFormat. See copyBytesColorAttachment() for context.
void readPixelsByFormat(IContext& context,
                        TextureFormat textureFormat,
                        GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        void* pixelBytes,
                        GLenum halfFloatFormat) {
  if (textureFormat == TextureFormat::RGBA_UInt32) {
    if (IGL_DEBUG_VERIFY(
            context.deviceFeatures().hasTextureFeature(TextureFeatures::TextureInteger))) {
      context.readPixels(x, y, width, height, GL_RGBA_INTEGER, GL_UNSIGNED_INT, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::R_UNorm8) {
    if (IGL_DEBUG_VERIFY(context.deviceFeatures().hasFeature(DeviceFeatures::TextureFormatRG))) {
      context.readPixels(x, y, width, height, GL_RED, GL_UNSIGNED_BYTE, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::RG_UNorm8) {
    if (IGL_DEBUG_VERIFY(context.deviceFeatures().hasFeature(DeviceFeatures::TextureFormatRG))) {
      context.readPixels(x, y, width, height, GL_RG, GL_UNSIGNED_BYTE, pixelBytes);
    }
  } else if (textureFormat == TextureFormat::RGBA_F16 || textureFormat == TextureFormat::RGB_F16 ||
             textureFormat == TextureFormat::RG_F16 || textureFormat == TextureFormat::R_F16) {
    readPixelsByFormatHalfFloat(
        context, textureFormat, x, y, width, height, pixelBytes, halfFloatFormat);
  } else if (textureFormat == TextureFormat::RGBA_F32 || textureFormat == TextureFormat::RGB_F32 ||
             textureFormat == TextureFormat::RG_F32 || textureFormat == TextureFormat::R_F32) {
    readPixelsByFormatFloat(context, textureFormat, x, y, width, height, pixelBytes);
  } else {
    context.readPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixelBytes);
  }
}

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

Texture::AttachmentParams toAttachmentParams(const RenderPassDesc::AttachmentDesc& attachment,
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
  IGL_DEBUG_ASSERT(range.numLayers == 1, "range.numLayers must be 1.");
  IGL_DEBUG_ASSERT(range.numMipLevels == 1, "range.numMipLevels must be 1.");
  IGL_DEBUG_ASSERT(range.numFaces == 1, "range.numFaces must be 1.");

  Texture::AttachmentParams params{};
  params.face = static_cast<uint32_t>(range.face);
  params.mipLevel = static_cast<uint32_t>(range.mipLevel);
  params.layer = static_cast<uint32_t>(range.layer);
  params.read = true;
  params.stereo = mode == FramebufferMode::Stereo;
  return params;
}

bool isNotLoaded(const std::shared_ptr<ITexture>& texture,
                 const RenderPassDesc::AttachmentDesc& attachment) {
  return texture != nullptr && attachment.loadAction == LoadAction::DontCare;
}

bool isMemorylessAndNotLoaded(const std::shared_ptr<ITexture>& texture,
                              const RenderPassDesc::AttachmentDesc& attachment) {
  return isNotLoaded(texture, attachment) && texture->getStorage() == ResourceStorage::Memoryless;
}

/// Fills `attachments` with the framebuffer attachments that are Memoryless and not loaded, and
/// returns how many were written. A render pass with subpasses keeps its intermediate data in tile
/// memory, so for it these are also the color attachments that are not loaded.
GLsizei collectNotLoadedAttachments(
    const FramebufferDesc& target,
    const RenderPassDesc& renderPass,
    std::array<GLenum, IGL_COLOR_ATTACHMENTS_MAX + 2>& attachments) {
  GLsizei numAttachments = 0;
  const bool hasSubpasses = !renderPass.subpasses.empty();
  const size_t numColorAttachments =
      std::min<size_t>(renderPass.colorAttachments.size(), IGL_COLOR_ATTACHMENTS_MAX);
  for (size_t i = 0; i != numColorAttachments; i++) {
    // NOLINTNEXTLINE(facebook-hte-ParameterUncheckedArrayBounds)
    const RenderPassDesc::AttachmentDesc& attachment = renderPass.colorAttachments[i];
    const std::shared_ptr<ITexture>& texture = target.colorAttachments[i].texture;
    if (hasSubpasses ? isNotLoaded(texture, attachment)
                     : isMemorylessAndNotLoaded(texture, attachment)) {
      attachments[numAttachments++] = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i);
    }
  }
  if (isMemorylessAndNotLoaded(target.depthAttachment.texture, renderPass.depthAttachment)) {
    attachments[numAttachments++] = GL_DEPTH_ATTACHMENT;
  }
  if (isMemorylessAndNotLoaded(target.stencilAttachment.texture, renderPass.stencilAttachment)) {
    attachments[numAttachments++] = GL_STENCIL_ATTACHMENT;
  }
  return numAttachments;
}

bool isMemorylessMultisampled2D(const ITexture& texture) {
  return texture.getStorage() == ResourceStorage::Memoryless && texture.getSamples() > 1 &&
         texture.getType() == TextureType::TwoD;
}

/// Whether multisampled rendering of `multisampled` can resolve into `resolve` with
/// GL_EXT_multisampled_render_to_texture
bool canResolveImplicitly(const ITexture& multisampled, const ITexture& resolve) {
  const Dimensions size = multisampled.getDimensions();
  const Dimensions resolveSize = resolve.getDimensions();
  return static_cast<const Texture&>(resolve).canAttachMultisampledRenderToTexture() &&
         (resolve.getUsage() & TextureDesc::TextureUsageBits::Attachment) != 0 &&
         resolve.getSamples() == 1 && resolve.getFormat() == multisampled.getFormat() &&
         resolveSize.width == size.width && resolveSize.height == size.height;
}

/// Whether GL_EXT_multisampled_render_to_texture accepts `samples` samples for the format of
/// `texture`. A driver leaves out an attachment with more, and can still report a framebuffer with
/// only a depth attachment complete.
bool supportsImplicitSamples(IContext& context, const ITexture& texture, uint32_t samples) {
  const GLint maxSamples = context.getMaxMultisampledRenderToTextureSamples(
      static_cast<const Texture&>(texture).getGLInternalTextureFormat());
  return maxSamples > 0 && samples <= static_cast<uint32_t>(maxSamples);
}

/// Whether `attachment` suits a framebuffer that resolves `samples` samples implicitly: it is
/// absent, or a Memoryless multisampled renderbuffer that GL_EXT_multisampled_render_to_texture
/// can render without a resolve texture, as depth is never resolved
bool isImplicitDepthStencil(IContext& context,
                            const FramebufferDesc::AttachmentDesc& attachment,
                            uint32_t samples) {
  if (attachment.resolveTexture != nullptr) {
    return false;
  }
  return attachment.texture == nullptr ||
         (isMemorylessMultisampled2D(*attachment.texture) &&
          attachment.texture->getSamples() == samples &&
          supportsImplicitSamples(context, *attachment.texture, samples));
}
} // namespace

FramebufferBindingGuard::FramebufferBindingGuard(IContext& context) : context_(context) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
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
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);
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

void Framebuffer::attachAsColor(ITexture& texture,
                                uint32_t index,
                                const Texture::AttachmentParams& params) const {
  IGL_PROFILER_FUNCTION();
  static_cast<Texture&>(texture).attachAsColor(index, params);
  IGL_DEBUG_ASSERT(index >= 0 && index < kNumCachedStates);
  colorCachedState_[index].updateCache(params.stereo ? FramebufferMode::Stereo
                                                     : FramebufferMode::Mono,
                                       params.layer,
                                       params.face,
                                       params.mipLevel);
}

void Framebuffer::attachAsDepth(ITexture& texture, const Texture::AttachmentParams& params) const {
  IGL_PROFILER_FUNCTION();
  static_cast<Texture&>(texture).attachAsDepth(params);
  depthCachedState_.updateCache(params.stereo ? FramebufferMode::Stereo : FramebufferMode::Mono,
                                params.layer,
                                params.face,
                                params.mipLevel);
}

void Framebuffer::attachAsStencil(ITexture& texture,
                                  const Texture::AttachmentParams& params) const {
  IGL_PROFILER_FUNCTION();
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
  IGL_PROFILER_FUNCTION();
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
  IGL_PROFILER_FUNCTION();
  // Only support attachment 0 because that's what glReadPixels supports
  if (index != 0) {
    IGL_DEBUG_ABORT("Invalid index: %d", index);
    return;
  }
  IGL_DEBUG_ASSERT(range.numFaces == 1, "range.numFaces MUST be 1");
  IGL_DEBUG_ASSERT(range.numLayers == 1, "range.numLayers MUST be 1");
  IGL_DEBUG_ASSERT(range.numMipLevels == 1, "range.numMipLevels MUST be 1");

  auto itexture = getColorAttachment(index);
  if (itexture == nullptr) {
    IGL_DEBUG_ABORT("The framebuffer does not have any color attachment at index %d", index);
    return;
  }

  const FramebufferBindingGuard guard(getContext());

  CustomFramebuffer extraFramebuffer(getContext());

  auto& texture = static_cast<Texture&>(*itexture);

  Result ret;
  const FramebufferDesc desc{
      .colorAttachments = {{.texture = itexture}},
  };
  extraFramebuffer.initialize(desc, &ret);
  IGL_DEBUG_ASSERT(ret.isOk(), ret.message.c_str());

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
    const int finalBytesPerRow = static_cast<int>(
        bytesPerRow == 0 ? itexture->getProperties().getBytesPerRow(range) : bytesPerRow);
    if (packRowLengthSupported) {
      getContext().pixelStorei(GL_PACK_ROW_LENGTH, 0);
    }
    getContext().pixelStorei(GL_PACK_ALIGNMENT,
                             texture.getAlignment(finalBytesPerRow, range.mipLevel, range.width));
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
  readPixelsByFormat(getContext(),
                     textureFormat,
                     rangeX,
                     rangeY,
                     rangeWidth,
                     rangeHeight,
                     pixelBytes,
                     kHalfFloatFormat);

  // Reset the GL_PACK_ROW_LENGTH
  if (usePackRowLength) {
    getContext().pixelStorei(GL_PACK_ROW_LENGTH, 0);
  }
  // Reset the GL_PACK_ALIGNMENT (default value for GL_PACK_ALIGNMENT is 4)
  getContext().pixelStorei(GL_PACK_ALIGNMENT, 4);

  getContext().checkForErrors(nullptr, 0);
  auto error = getContext().getLastError();
  IGL_DEBUG_ASSERT(error.isOk(), error.message.c_str());
}

void Framebuffer::copyBytesDepthAttachment(ICommandQueue& /* unused */,
                                           void* /*pixelBytes*/,
                                           const TextureRangeDesc& /*range*/,
                                           size_t /*bytesPerRow*/) const {
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
}

void Framebuffer::copyBytesStencilAttachment(ICommandQueue& /* unused */,
                                             void* /*pixelBytes*/,
                                             const TextureRangeDesc& /*range*/,
                                             size_t /*bytesPerRow*/) const {
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
}

void Framebuffer::copyTextureColorAttachment(ICommandQueue& /*cmdQueue*/,
                                             size_t index,
                                             std::shared_ptr<ITexture> destTexture,
                                             const TextureRangeDesc& range) const {
  IGL_PROFILER_FUNCTION();
  // Only support attachment 0 because that's what glCopyTexImage2D supports
  if (index != 0 || getColorAttachment(index) == nullptr) {
    IGL_DEBUG_ABORT("Invalid index: %d", index);
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

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
bool Framebuffer::CachedState::needsUpdate(FramebufferMode newMode,
                                           uint8_t newLayer,
                                           uint8_t newFace,
                                           uint8_t newMipLevel) {
  // NOLINTEND(bugprone-easily-swappable-parameters)
  return mode != newMode || layer != newLayer || face != newFace || mipLevel != newMipLevel;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
void Framebuffer::CachedState::updateCache(FramebufferMode newMode,
                                           uint8_t newLayer,
                                           uint8_t newFace,
                                           uint8_t newMipLevel) {
  // NOLINTEND(bugprone-easily-swappable-parameters)
  mode = newMode;
  layer = newLayer;
  face = newFace;
  mipLevel = newMipLevel;
}

///--------------------------------------
/// MARK: - CustomFramebuffer

CustomFramebuffer::~CustomFramebuffer() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_DESTROY);
  if (frameBufferID_ != 0) {
    getContext().deleteFramebuffers(1, &frameBufferID_);
    frameBufferID_ = 0;
  }
}

std::vector<size_t> CustomFramebuffer::getColorAttachmentIndices() const {
  IGL_PROFILER_FUNCTION();
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
  IGL_DEBUG_ASSERT(index < IGL_COLOR_ATTACHMENTS_MAX);
  return renderTarget_.colorAttachments[index].texture;
}

std::shared_ptr<ITexture> CustomFramebuffer::getResolveColorAttachment(size_t index) const {
  IGL_DEBUG_ASSERT(index < IGL_COLOR_ATTACHMENTS_MAX);
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
  updateDrawableInternal({.color = std::move(texture), .depth = nullptr}, false);
}

void CustomFramebuffer::updateDrawable(SurfaceTextures surfaceTextures) {
  updateDrawableInternal(std::move(surfaceTextures), true);
}

void CustomFramebuffer::updateResolveAttachment(std::shared_ptr<ITexture> texture) {
  IGL_PROFILER_FUNCTION();
  auto& colorAttachment0 = renderTarget_.colorAttachments[0];
  if (frameBufferID_ == 0 || colorAttachment0.resolveTexture == texture) {
    return;
  }
  const bool wasImplicit = resolvesImplicitly();
  std::shared_ptr<ITexture> previousColor = wasImplicit ? colorAttachment0.resolveTexture : nullptr;
  colorAttachment0.resolveTexture = std::move(texture);
  if (!wasImplicit && getImplicitResolveSamples() == 0) {
    // The framebuffer resolves with a blit before and after, so only the resolve framebuffer has
    // to change
    if (resolveFramebuffer_ != nullptr && colorAttachment0.resolveTexture != nullptr) {
      resolveFramebuffer_->updateDrawable(colorAttachment0.resolveTexture);
    } else {
      setupResolveFramebuffer(nullptr);
    }
    return;
  }
  const FramebufferBindingGuard guard(getContext());
  bindBuffer();
  reattachTextures(previousColor, {.resolve = true});
}

void CustomFramebuffer::updateDrawableInternal(SurfaceTextures surfaceTextures,
                                               bool updateDepthStencil) {
  IGL_PROFILER_FUNCTION();
  auto colorAttachment0 = getColorAttachment(0);
  auto depthAttachment = updateDepthStencil ? getDepthAttachment() : nullptr;
  auto stencilAttachment = updateDepthStencil ? getStencilAttachment() : nullptr;

  const bool updateColor = colorAttachment0 != surfaceTextures.color;
  updateDepthStencil = updateDepthStencil && (depthAttachment != surfaceTextures.depth ||
                                              stencilAttachment != surfaceTextures.depth);
  if (!updateColor && !updateDepthStencil) {
    return;
  }

  const FramebufferBindingGuard guard(getContext());
  bindBuffer();
  const bool wasImplicit = resolvesImplicitly();
  // A framebuffer that resolves implicitly has the resolve texture attached instead of the color
  const std::shared_ptr<ITexture> previousColor =
      wasImplicit ? renderTarget_.colorAttachments[0].resolveTexture : colorAttachment0;
  if (updateColor) {
    if (!surfaceTextures.color && previousColor) {
      static_cast<Texture&>(*previousColor).detachAsColor(0, false);
    }
    // The resolve texture stays for the next color texture
    renderTarget_.colorAttachments[0].texture = std::move(surfaceTextures.color);
  }
  if (updateDepthStencil) {
    std::shared_ptr<ITexture> stencil;
    if (surfaceTextures.depth) {
      if (surfaceTextures.depth->getProperties().hasStencil()) {
        stencil = surfaceTextures.depth;
      }
    } else if (depthAttachment != stencilAttachment) {
      // Only a stencil texture that is the depth texture goes when the depth texture does
      stencil = stencilAttachment;
    }
    // Detaching a texture releases its implicit renderbuffer, so an implicit framebuffer detaches
    // the textures that others replace as well
    if (depthAttachment && depthAttachment != surfaceTextures.depth &&
        (wasImplicit || !surfaceTextures.depth)) {
      static_cast<Texture&>(*depthAttachment).detachAsDepth(false);
    }
    if (stencilAttachment && stencilAttachment != stencil && (wasImplicit || !stencil)) {
      static_cast<Texture&>(*stencilAttachment).detachAsStencil(false);
    }
    renderTarget_.stencilAttachment.texture = stencil;
    renderTarget_.depthAttachment.texture = std::move(surfaceTextures.depth);
  }
  reattachTextures(previousColor, {.color = updateColor, .depthStencil = updateDepthStencil});
}

bool CustomFramebuffer::isInitialized() const {
  return initialized_;
}

bool CustomFramebuffer::hasImplicitColorAttachment() const {
  IGL_PROFILER_FUNCTION();
  if (frameBufferID_ != 0) {
    return false;
  }

  const auto& colorAttachment0 = renderTarget_.colorAttachments[0];

  return colorAttachment0.texture != nullptr &&
         static_cast<Texture&>(*colorAttachment0.texture).isImplicitStorage();
}

// NOLINTNEXTLINE(misc-no-recursion)
void CustomFramebuffer::initialize(const FramebufferDesc& desc, Result* outResult) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
  if (IGL_DEBUG_VERIFY_NOT(isInitialized())) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Framebuffer already initialized.");
    return;
  }
  initialized_ = true;

  renderTarget_ = desc;

  // Restore framebuffer binding
  const FramebufferBindingGuard guard(getContext());
  if (hasImplicitColorAttachment()) {
    // Don't generate framebuffer id. Use implicit framebuffer supplied by containing view
    Result::setOk(outResult);
  } else {
    prepareResource(desc.debugName, outResult);
  }
}

// NOLINTNEXTLINE(misc-no-recursion)
void CustomFramebuffer::prepareResource(const std::string& debugName, Result* outResult) {
  IGL_PROFILER_FUNCTION();
  // create a new frame buffer if we don't already have one
  getContext().genFramebuffers(1, &frameBufferID_);
  if (IGL_DEBUG_VERIFY_NOT(frameBufferID_ == 0)) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Failed to create framebuffer ID.");
    return;
  }

  bindBuffer();

  if (!debugName.empty() &&
      getContext().deviceFeatures().hasInternalFeature(InternalFeatures::DebugLabel)) {
    getContext().objectLabel(
        GL_FRAMEBUFFER, frameBufferID_, static_cast<GLsizei>(debugName.size()), debugName.c_str());
  }

  // attach the textures and render buffers to the frame buffer
  implicitSamples_ = getImplicitResolveSamples();
  attachTextures();

  std::vector<GLenum> drawBuffers;
  bool hasBuffers = false;
  for (size_t i = 0; i != IGL_COLOR_ATTACHMENTS_MAX; i++) {
    if (renderTarget_.colorAttachments[i].texture != nullptr) {
      drawBuffers.push_back(static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i));
      if (i) {
        // skip attachment 0
        hasBuffers = true;
      }
    } else {
      drawBuffers.push_back(GL_NONE);
    }
  }

  if (hasBuffers) {
    // needs to be called when there's more than one color attachment or if there's a single color
    // attachment at a non-zero index
    getContext().drawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
  }

  Result result = checkFramebufferStatus(getContext(), false);
  if (!result.isOk() && resolvesImplicitly()) {
    // The driver can reject the samples, for example by rounding those of the attachments up
    // differently. Resolve with the resolve framebuffer instead.
    fallBackToBlit();
    result = checkFramebufferStatus(getContext(), false);
  }
  IGL_DEBUG_ASSERT(result.isOk(), result.message.c_str());
  if (outResult) {
    *outResult = result;
  }
  if (!result.isOk()) {
    return;
  }

  if (!resolvesImplicitly()) {
    setupResolveFramebuffer(outResult);
  }
}

// NOLINTNEXTLINE(misc-no-recursion)
void CustomFramebuffer::setupResolveFramebuffer(Result* outResult) {
  IGL_PROFILER_FUNCTION();
  resolveFramebuffer_ = nullptr;
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
  // A resolve texture that is kept for an attachment whose texture an update removed is no mismatch
  if (createResolveFramebuffer && (maskColorAttachments & ~maskColorResolveAttachments) != 0) {
    IGL_DEBUG_ASSERT_NOT_REACHED();
    if (outResult) {
      *outResult = Result(igl::Result::Code::ArgumentInvalid,
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
    Result result;
    auto cfb = std::make_shared<CustomFramebuffer>(getContext());
    cfb->initialize(resolveDesc, &result);
    if (outResult) {
      *outResult = result;
    }
    resolveFramebuffer_ = std::move(cfb);
  }
}

bool CustomFramebuffer::resolvesImplicitly() const {
  return implicitSamples_ > 1;
}

uint32_t CustomFramebuffer::getImplicitResolveSamples() const {
  IGL_PROFILER_FUNCTION();
  IContext& context = getContext();
  const DeviceFeatureSet& features = context.deviceFeatures();
  if (frameBufferID_ == 0 || renderTarget_.mode != FramebufferMode::Mono ||
      !features.hasInternalFeature(InternalFeatures::MultisampledRenderToTexture)) {
    return 0;
  }

  // Every color attachment has to be a Memoryless multisampled one with a resolve texture that the
  // GPU can resolve into, and all of them have to use the same number of samples
  uint32_t samples = 0;
  for (size_t i = 0; i != IGL_COLOR_ATTACHMENTS_MAX; i++) {
    const auto& colorAttachment = renderTarget_.colorAttachments[i];
    if (colorAttachment.texture == nullptr) {
      if (colorAttachment.resolveTexture != nullptr) {
        return 0;
      }
      continue;
    }
    if (!isMemorylessMultisampled2D(*colorAttachment.texture) ||
        colorAttachment.resolveTexture == nullptr ||
        !canResolveImplicitly(*colorAttachment.texture, *colorAttachment.resolveTexture)) {
      return 0;
    }
    // Only GL_COLOR_ATTACHMENT0 can use the extension without its second version
    if (i != 0 && !features.hasExtension(Extensions::MultiSampleExt2)) {
      return 0;
    }
    const uint32_t colorSamples = colorAttachment.texture->getSamples();
    if ((samples != 0 && colorSamples != samples) ||
        !supportsImplicitSamples(context, *colorAttachment.texture, colorSamples)) {
      return 0;
    }
    samples = colorSamples;
  }

  if (samples == 0 || !isImplicitDepthStencil(context, renderTarget_.depthAttachment, samples) ||
      !isImplicitDepthStencil(context, renderTarget_.stencilAttachment, samples)) {
    return 0;
  }
  return samples;
}

void CustomFramebuffer::attachColor(size_t index, Texture::AttachmentParams params) const {
  const auto& colorAttachment = renderTarget_.colorAttachments[index];
  params.implicitSamples = implicitSamples_;
  attachAsColor(resolvesImplicitly() ? *colorAttachment.resolveTexture : *colorAttachment.texture,
                static_cast<uint32_t>(index),
                params);
}

void CustomFramebuffer::attachDepth(Texture::AttachmentParams params) const {
  params.implicitSamples = implicitSamples_;
  attachAsDepth(*renderTarget_.depthAttachment.texture, params);
}

void CustomFramebuffer::attachStencil(Texture::AttachmentParams params) const {
  params.implicitSamples = implicitSamples_;
  attachAsStencil(*renderTarget_.stencilAttachment.texture, params);
}

void CustomFramebuffer::attachTextures() const {
  IGL_PROFILER_FUNCTION();
  const auto attachmentParams = defaultWriteAttachmentParams(renderTarget_.mode);
  for (size_t i = 0; i != IGL_COLOR_ATTACHMENTS_MAX; i++) {
    if (renderTarget_.colorAttachments[i].texture != nullptr) {
      attachColor(i, attachmentParams);
    }
  }
  if (renderTarget_.depthAttachment.texture != nullptr) {
    attachDepth(attachmentParams);
  }
  if (renderTarget_.stencilAttachment.texture != nullptr) {
    attachStencil(attachmentParams);
  }
}

void CustomFramebuffer::detachTextures() const {
  IGL_PROFILER_FUNCTION();
  for (size_t i = 0; i != IGL_COLOR_ATTACHMENTS_MAX; i++) {
    const auto& colorAttachment = renderTarget_.colorAttachments[i];
    if (colorAttachment.texture != nullptr) {
      const auto& attached = resolvesImplicitly() ? colorAttachment.resolveTexture
                                                  : colorAttachment.texture;
      static_cast<Texture&>(*attached).detachAsColor(static_cast<uint32_t>(i), false);
    }
  }
  if (renderTarget_.depthAttachment.texture != nullptr) {
    static_cast<Texture&>(*renderTarget_.depthAttachment.texture).detachAsDepth(false);
  }
  if (renderTarget_.stencilAttachment.texture != nullptr) {
    static_cast<Texture&>(*renderTarget_.stencilAttachment.texture).detachAsStencil(false);
  }
}

void CustomFramebuffer::attachReplacedTextures(const Replaced& replaced) const {
  IGL_PROFILER_FUNCTION();
  const auto attachmentParams = defaultWriteAttachmentParams(renderTarget_.mode);
  // A framebuffer that resolves implicitly never attaches the multisampled color texture
  const bool colorReplaced = resolvesImplicitly() ? replaced.resolve : replaced.color;
  if (colorReplaced && renderTarget_.colorAttachments[0].texture != nullptr) {
    attachColor(0, attachmentParams);
  }
  if (replaced.depthStencil) {
    if (renderTarget_.depthAttachment.texture != nullptr) {
      attachDepth(attachmentParams);
    }
    if (renderTarget_.stencilAttachment.texture != nullptr) {
      attachStencil(attachmentParams);
    }
  }
}

void CustomFramebuffer::detachImplicitColors(const std::shared_ptr<ITexture>& previousColor) const {
  IGL_PROFILER_FUNCTION();
  for (size_t i = 0; i != IGL_COLOR_ATTACHMENTS_MAX; i++) {
    // Only color attachment 0 can have another texture in renderTarget_ than the attached one
    const auto& attached = i == 0 ? previousColor
                                  : renderTarget_.colorAttachments[i].resolveTexture;
    if (attached != nullptr) {
      static_cast<Texture&>(*attached).detachAsColor(static_cast<uint32_t>(i), false);
    }
  }
}

void CustomFramebuffer::fallBackToBlit() {
  IGL_PROFILER_FUNCTION();
  detachTextures();
  implicitSamples_ = 0;
  attachTextures();
}

void CustomFramebuffer::reattachTextures(const std::shared_ptr<ITexture>& previousColor,
                                         const Replaced& replaced) {
  IGL_PROFILER_FUNCTION();
  const uint32_t previousSamples = implicitSamples_;
  implicitSamples_ = getImplicitResolveSamples();
  const bool sameSamples = implicitSamples_ == previousSamples;
  if (sameSamples) {
    attachReplacedTextures(replaced);
  } else {
    if (previousSamples > 1) {
      // Skia binds texture 0 before it changes the number of samples of a texture that is attached
      // with glFramebufferTexture2DMultisample(), because the Adreno 3xx and Mali-G drivers corrupt
      // the contents of the texture otherwise
      detachImplicitColors(previousColor);
    }
    // The textures attach differently now, so attach all of them
    attachTextures();
  }

  const bool triedImplicit = resolvesImplicitly();
  if (triedImplicit) {
    // A resolve texture that replaces another one has its format and size, and the samples stay, so
    // the framebuffer stays complete. Any other change can have the driver round the samples of an
    // attachment up differently.
    const bool resolveSwap = sameSamples && replaced.resolve && !replaced.color &&
                             !replaced.depthStencil;
    if (!resolveSwap && !checkFramebufferStatus(getContext(), false).isOk()) {
      fallBackToBlit();
    }
  }

  if (resolvesImplicitly()) {
    // The GPU resolves in tile memory, so there is no resolve blit
    resolveFramebuffer_ = nullptr;
  } else if (previousSamples > 1 || triedImplicit) {
    setupResolveFramebuffer(nullptr);
  }
}

Viewport CustomFramebuffer::getViewport() const {
  IGL_PROFILER_FUNCTION();
  auto texture = getColorAttachment(0);

  if (texture == nullptr) {
    texture = getDepthAttachment();
  }

  if (texture == nullptr) {
    IGL_DEBUG_ABORT("No color/depth attachments in CustomFrameBuffer at index 0");
    return {.x = 0, .y = 0, .width = 0, .height = 0};
  }

  // By default, we set viewport to dimensions of framebuffer
  const auto size = texture->getSize();
  return {.x = 0, .y = 0, .width = size.width, .height = size.height};
}

void CustomFramebuffer::applyClearMask(int targetCount) const {
  IGL_PROFILER_FUNCTION();
  // clear the buffers if we're not loading previous contents
  GLbitfield clearMask = 0;
  if (getContext().deviceFeatures().hasInternalFeature(InternalFeatures::ClearBufferfv) &&
      targetCount > 1) {
    for (size_t index = 0; index != IGL_COLOR_ATTACHMENTS_MAX; index++) {
      const auto& colorAttachment = renderTarget_.colorAttachments[index];

      if (colorAttachment.texture != nullptr &&
          renderPass_.colorAttachments[index].loadAction == LoadAction::Clear) {
        auto clearColor = renderPass_.colorAttachments[index].clearColor;
        getContext().colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        getContext().clearBufferfv(GL_COLOR, static_cast<GLint>(index), clearColor.toFloatPtr());
      }
    }
  } else {
    const auto& colorAttachment0 = renderTarget_.colorAttachments[0];

    if (colorAttachment0.texture != nullptr &&
        renderPass_.colorAttachments[0].loadAction == LoadAction::Clear) {
      clearMask |= GL_COLOR_BUFFER_BIT;
      auto clearColor = renderPass_.colorAttachments[0].clearColor;
      getContext().colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      getContext().clearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    }
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

void CustomFramebuffer::bind(const RenderPassDesc& renderPass) const {
  IGL_PROFILER_FUNCTION();
  // Cache renderPass for unbind
  renderPass_ = renderPass;
  renderPass_.timestampQuery = {};
  IGL_DEBUG_ASSERT(renderTarget_.mode != FramebufferMode::Multiview,
                   "FramebufferMode::Multiview not supported");

  bindBuffer();

  int targetCount = 0;
  for (size_t i = 0; i != IGL_COLOR_ATTACHMENTS_MAX; i++) {
    const auto& colorAttachment = renderTarget_.colorAttachments[i];
    if (!colorAttachment.texture) {
      continue;
    }
    targetCount++;
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
    IGL_DEBUG_ASSERT(index >= 0 && index < renderPass.colorAttachments.size());
    // NOLINTNEXTLINE(facebook-hte-ParameterUncheckedArrayBounds)
    const auto& renderPassAttachment = renderPass.colorAttachments[index];
    // When setting up a framebuffer, we attach textures as though they were a non-array
    // texture with and set layer, mip-level and face equal to 0.
    // If any of these assumptions are not true, we need to reattach with proper values.
    IGL_DEBUG_ASSERT(index >= 0 && index < kNumCachedStates);
    if (colorCachedState_[index].needsUpdate(renderTarget_.mode,
                                             renderPassAttachment.layer,
                                             renderPassAttachment.face,
                                             renderPassAttachment.mipLevel)) {
      attachColor(index, toAttachmentParams(renderPassAttachment, renderTarget_.mode));
    }
  }
  if (renderTarget_.depthAttachment.texture) {
    const auto& renderPassAttachment = renderPass.depthAttachment;
    if (depthCachedState_.needsUpdate(renderTarget_.mode,
                                      renderPassAttachment.layer,
                                      renderPassAttachment.face,
                                      renderPassAttachment.mipLevel)) {
      attachDepth(toAttachmentParams(renderPassAttachment, renderTarget_.mode));
    }
  }
  if (renderTarget_.stencilAttachment.texture) {
    const auto& renderPassAttachment = renderPass.stencilAttachment;
    if (stencilCachedState_.needsUpdate(renderTarget_.mode,
                                        renderPassAttachment.layer,
                                        renderPassAttachment.face,
                                        renderPassAttachment.mipLevel)) {
      attachStencil(toAttachmentParams(renderPassAttachment, renderTarget_.mode));
    }
  }

  // Memoryless attachments, and for a render pass with subpasses color attachments, that are not
  // loaded have no contents worth fetching into tile memory.
  std::array<GLenum, IGL_COLOR_ATTACHMENTS_MAX + 2> notLoadedAttachments{};
  const GLsizei numNotLoadedAttachments =
      collectNotLoadedAttachments(renderTarget_, renderPass, notLoadedAttachments);
  if (numNotLoadedAttachments > 0 &&
      getContext().deviceFeatures().hasInternalFeature(InternalFeatures::InvalidateFramebuffer)) {
    getContext().invalidateFramebuffer(
        GL_FRAMEBUFFER, numNotLoadedAttachments, notLoadedAttachments.data());
  }

  applyClearMask(targetCount);
}

void CustomFramebuffer::unbind() const {
  IGL_PROFILER_FUNCTION();
  // Discard attachments whose store action is not Store. For MRT, iterate all
  // color attachments so any of them tagged DontCare/Invalidate get released
  // (otherwise the driver must preserve their contents, costing bandwidth on
  // tilers).
  GLenum attachments[IGL_COLOR_ATTACHMENTS_MAX + 2];
  GLsizei numAttachments = 0;

  // The default framebuffer names its buffers GL_COLOR, GL_DEPTH and GL_STENCIL;
  // the *_ATTACHMENT names are only valid for framebuffer objects, and passing
  // them for the default framebuffer is GL_INVALID_ENUM -- raised on every frame
  // that renders to a view.
  const bool isDefaultFramebuffer = hasImplicitColorAttachment();

  for (size_t i = 0; i < IGL_COLOR_ATTACHMENTS_MAX; ++i) {
    const auto& colorAttachment = renderTarget_.colorAttachments[i];
    if (colorAttachment.texture != nullptr) {
      const StoreAction storeAction = renderPass_.colorAttachments[i].storeAction;
      // What an implicit resolve writes into the resolve texture is the result of MsaaResolve, and
      // discarding it would discard the resolve
      const bool discard = resolvesImplicitly() ? storeAction == StoreAction::DontCare
                                                : storeAction != StoreAction::Store;
      if (discard) {
        attachments[numAttachments++] =
            isDefaultFramebuffer ? GL_COLOR : static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i);
      }
    }
    if (isDefaultFramebuffer) {
      // The default framebuffer has a single color buffer.
      break;
    }
  }
  if (renderTarget_.depthAttachment.texture != nullptr) {
    if (renderPass_.depthAttachment.storeAction != StoreAction::Store) {
      attachments[numAttachments++] = isDefaultFramebuffer ? GL_DEPTH : GL_DEPTH_ATTACHMENT;
    }
  }
  if (renderTarget_.stencilAttachment.texture != nullptr) {
    getContext().disable(GL_STENCIL_TEST);
    if (renderPass_.stencilAttachment.storeAction != StoreAction::Store) {
      attachments[numAttachments++] = isDefaultFramebuffer ? GL_STENCIL : GL_STENCIL_ATTACHMENT;
    }
  }

  if (numAttachments > 0) {
    const auto& features = getContext().deviceFeatures();
    if (features.hasInternalFeature(InternalFeatures::InvalidateFramebuffer)) {
      getContext().invalidateFramebuffer(GL_FRAMEBUFFER, numAttachments, attachments);
    }
  }

  // Mali GPUs hang when passes that resolve implicitly follow each other on a framebuffer that
  // stays bound. Binding another framebuffer ends each pass.
  if (resolvesImplicitly()) {
    getContext().bindFramebuffer(GL_FRAMEBUFFER, 0);
  }
}

bool CustomFramebuffer::hasTextureAttachments() const {
  return !hasImplicitColorAttachment();
}

///--------------------------------------
/// MARK: - CurrentFramebuffer

CurrentFramebuffer::CurrentFramebuffer(IContext& context) : Super(context) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_CREATE);
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
    IGL_DEBUG_ASSERT_NOT_REACHED();
  }
  return colorAttachment_;
}

std::shared_ptr<ITexture> CurrentFramebuffer::getResolveColorAttachment(size_t index) const {
  if (index != 0) {
    IGL_DEBUG_ASSERT_NOT_REACHED();
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
  IGL_DEBUG_ASSERT_NOT_REACHED();
}

void CurrentFramebuffer::updateDrawable(SurfaceTextures /*surfaceTextures*/) {
  IGL_DEBUG_ASSERT_NOT_REACHED();
}

void CurrentFramebuffer::updateResolveAttachment(std::shared_ptr<ITexture> /*texture*/) {
  IGL_DEBUG_ASSERT_NOT_REACHED();
}

Viewport CurrentFramebuffer::getViewport() const {
  return viewport_;
}

void CurrentFramebuffer::bind(const RenderPassDesc& renderPass) const {
  IGL_PROFILER_FUNCTION();
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
  // NOLINTNEXTLINE(facebook-hte-ParameterUncheckedArrayBounds)
  if (renderPass.colorAttachments[0].loadAction == LoadAction::Clear) {
    clearMask |= GL_COLOR_BUFFER_BIT;
    auto clearColor = renderPass.colorAttachments[0].clearColor;
    getContext().colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    getContext().clearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
  }
  if (renderPass.depthAttachment.loadAction == LoadAction::Clear) {
    clearMask |= GL_DEPTH_BUFFER_BIT;
    getContext().depthMask(GL_TRUE);
    getContext().clearDepthf(renderPass.depthAttachment.clearDepth);
  }
  if (renderPass.stencilAttachment.loadAction == LoadAction::Clear) {
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

bool CurrentFramebuffer::hasTextureAttachments() const {
  return false;
}

FramebufferMode CurrentFramebuffer::getMode() const {
  return FramebufferMode::Mono;
}

} // namespace igl::opengl
