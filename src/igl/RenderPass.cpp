/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/RenderPass.h>

#include <memory>
#include <vector>
#include <igl/Framebuffer.h>
#include <igl/Texture.h>

namespace igl {

namespace {

Result validateSubpasses(const RenderPassDesc& renderPass,
                         const IFramebuffer& framebuffer) noexcept {
  const size_t numSubpasses = renderPass.subpasses.size();
  if (numSubpasses < 2 || numSubpasses > IGL_SUBPASSES_MAX) {
    return Result(Result::Code::ArgumentOutOfRange,
                  "RenderPassDesc::subpasses must have between 2 and IGL_SUBPASSES_MAX entries");
  }
  if (framebuffer.getMode() != FramebufferMode::Mono) {
    return Result(Result::Code::Unsupported, "Subpasses support only mono framebuffers");
  }

  const std::vector<size_t> colorIndices = framebuffer.getColorAttachmentIndices();
  const size_t numColorAttachments = colorIndices.size();
  if (numColorAttachments == 0) {
    return Result(Result::Code::ArgumentInvalid, "Subpasses require at least one color attachment");
  }
  for (size_t i = 0; i < numColorAttachments; ++i) {
    if (colorIndices[i] != i) {
      return Result(Result::Code::ArgumentInvalid,
                    "Subpasses require color attachments to be contiguous from index 0");
    }
  }
  if (renderPass.colorAttachments.size() < numColorAttachments) {
    return Result(Result::Code::ArgumentInvalid,
                  "RenderPassDesc::colorAttachments must describe every framebuffer attachment");
  }

  uint32_t missingInputUsageMask = 0;
  for (size_t i = 0; i < numColorAttachments; ++i) {
    const std::shared_ptr<ITexture> color = framebuffer.getColorAttachment(i);
    if (color == nullptr) {
      return Result(Result::Code::ArgumentInvalid,
                    "Subpasses require every color attachment to have a texture");
    }
    if (color->getSamples() != 1) {
      return Result(Result::Code::Unsupported,
                    "Subpasses do not support multisampled color attachments");
    }
    const std::shared_ptr<ITexture> resolve = framebuffer.getResolveColorAttachment(i);
    if (resolve != nullptr && resolve != color) {
      return Result(Result::Code::Unsupported,
                    "Subpasses do not support resolve color attachments");
    }
    if ((color->getUsage() & TextureDesc::TextureUsageBits::InputAttachment) == 0) {
      missingInputUsageMask |= 1u << i;
    }
  }
  const std::shared_ptr<ITexture> depth = framebuffer.getDepthAttachment();
  if (depth != nullptr && depth->getSamples() != 1) {
    return Result(Result::Code::Unsupported,
                  "Subpasses do not support a multisampled depth attachment");
  }
  if (framebuffer.getResolveDepthAttachment() != nullptr) {
    return Result(Result::Code::Unsupported, "Subpasses do not support a resolve depth attachment");
  }

  const uint32_t validMask = (1u << numColorAttachments) - 1u;
  uint32_t writtenMask = 0;
  uint32_t inputMask = 0;
  for (const RenderPassDesc::SubpassDesc& subpass : renderPass.subpasses) {
    const uint32_t colorMask = subpass.colorAttachmentMask;
    const uint32_t readMask = subpass.inputAttachmentMask;
    if (((colorMask | readMask) & ~validMask) != 0) {
      return Result(Result::Code::ArgumentOutOfRange,
                    "A subpass references a color attachment that does not exist");
    }
    if ((colorMask & readMask) != 0) {
      return Result(Result::Code::ArgumentInvalid,
                    "A subpass cannot read a color attachment that it also writes");
    }
    if ((readMask & ~writtenMask) != 0) {
      return Result(Result::Code::ArgumentInvalid,
                    "A subpass can only read color attachments written by an earlier subpass");
    }
    writtenMask |= colorMask;
    inputMask |= readMask;
  }
  if (writtenMask != validMask) {
    return Result(Result::Code::ArgumentInvalid,
                  "Every color attachment must be written by at least one subpass");
  }
  if ((inputMask & missingInputUsageMask) != 0) {
    return Result(Result::Code::ArgumentInvalid,
                  "Input attachments require TextureDesc::TextureUsageBits::InputAttachment");
  }
  return Result();
}

} // namespace

Result RenderPassDesc::validate(const IFramebuffer& framebuffer) const noexcept {
  if (subpasses.empty()) {
    return Result();
  }
  return validateSubpasses(*this, framebuffer);
}

} // namespace igl
