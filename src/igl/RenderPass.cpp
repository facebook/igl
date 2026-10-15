/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/RenderPass.h>

#include <memory>
#include <string>
#include <vector>
#include <igl/Framebuffer.h>
#include <igl/Texture.h>

namespace igl {

namespace {

constexpr const char* kStereoLayersError =
    "Stereo subpasses need attachments with at least 2 layers from the attached layer";

// The 2 views of a Stereo framebuffer render to the 2 layers of `texture` from `layer` on
bool hasStereoLayers(const ITexture& texture, uint8_t layer) noexcept {
  return texture.getNumLayers() >= static_cast<uint32_t>(layer) + 2u;
}

bool isMemoryless(const std::shared_ptr<ITexture>& texture) noexcept {
  return texture != nullptr && texture->getStorage() == ResourceStorage::Memoryless;
}

// Returns why `desc` is invalid for `texture`, or nullptr if it is valid. Only calls getSamples()
// on Memoryless textures, because some backends' placeholder textures (e.g. the OpenGL default
// framebuffer's) do not support it. `resolve` is only read when the store action is MsaaResolve.
// It is empty for stencil attachments, which have no resolve accessor.
const char* IGL_NULLABLE findMemorylessViolation(const std::shared_ptr<ITexture>& texture,
                                                 const std::shared_ptr<ITexture>& resolve,
                                                 const RenderPassDesc::AttachmentDesc& desc,
                                                 bool canCheckResolve) noexcept {
  const bool resolves = desc.storeAction == StoreAction::MsaaResolve;
  if (isMemoryless(texture)) {
    if (desc.loadAction != LoadAction::Clear && desc.loadAction != LoadAction::DontCare) {
      return "a Memoryless attachment must use LoadAction::Clear or LoadAction::DontCare";
    }
    if (desc.storeAction != StoreAction::DontCare && !resolves) {
      return "a Memoryless attachment must use StoreAction::DontCare or StoreAction::MsaaResolve";
    }
    if (resolves) {
      if (texture->getSamples() <= 1) {
        return "a Memoryless attachment can only use StoreAction::MsaaResolve when it is "
               "multisampled";
      }
      if (canCheckResolve && (resolve == nullptr || resolve == texture)) {
        return "StoreAction::MsaaResolve on a Memoryless attachment needs a separate resolve "
               "texture";
      }
    }
  }
  if (resolves && isMemoryless(resolve)) {
    return "StoreAction::MsaaResolve cannot resolve into a Memoryless texture";
  }
  return nullptr;
}

Result makeMemorylessError(const std::string& attachment, const char* violation) {
  return Result(Result::Code::ArgumentInvalid, attachment + ": " + violation);
}

Result validateMemoryless(const RenderPassDesc& renderPass,
                          const IFramebuffer& framebuffer,
                          const std::vector<size_t>& colorIndices) noexcept {
  const RenderPassDesc::AttachmentDesc defaultDesc{};
  for (const size_t i : colorIndices) {
    const RenderPassDesc::AttachmentDesc* desc = &defaultDesc;
    if (i < renderPass.colorAttachments.size()) {
      // NOLINTNEXTLINE(facebook-hte-ParameterUncheckedArrayBounds)
      desc = &renderPass.colorAttachments[i];
    }
    const std::shared_ptr<ITexture> color = framebuffer.getColorAttachment(i);
    const bool resolves = desc->storeAction == StoreAction::MsaaResolve;
    if (!resolves && !isMemoryless(color)) {
      continue;
    }
    const char* violation = findMemorylessViolation(
        color, resolves ? framebuffer.getResolveColorAttachment(i) : nullptr, *desc, true);
    if (violation != nullptr) {
      return makeMemorylessError("Color attachment " + std::to_string(i), violation);
    }
  }

  const std::shared_ptr<ITexture> depth = framebuffer.getDepthAttachment();
  if (depth != nullptr) {
    const RenderPassDesc::AttachmentDesc& desc = renderPass.depthAttachment;
    const bool resolves = desc.storeAction == StoreAction::MsaaResolve;
    const char* violation = findMemorylessViolation(
        depth, resolves ? framebuffer.getResolveDepthAttachment() : nullptr, desc, true);
    if (violation != nullptr) {
      return makeMemorylessError("Depth attachment", violation);
    }
  }

  const std::shared_ptr<ITexture> stencil = framebuffer.getStencilAttachment();
  if (stencil != nullptr) {
    const char* violation =
        findMemorylessViolation(stencil, nullptr, renderPass.stencilAttachment, false);
    if (violation != nullptr) {
      return makeMemorylessError("Stencil attachment", violation);
    }
  }
  return Result();
}

Result validateSubpasses(const RenderPassDesc& renderPass,
                         const IFramebuffer& framebuffer,
                         const std::vector<size_t>& colorIndices) noexcept {
  const size_t numSubpasses = renderPass.subpasses.size();
  if (numSubpasses < 2 || numSubpasses > IGL_SUBPASSES_MAX) {
    return Result(Result::Code::ArgumentOutOfRange,
                  "RenderPassDesc::subpasses must have between 2 and IGL_SUBPASSES_MAX entries");
  }
  const FramebufferMode mode = framebuffer.getMode();
  if (mode == FramebufferMode::Multiview) {
    return Result(Result::Code::Unsupported, "Subpasses support only mono and stereo framebuffers");
  }
  const bool isStereo = mode == FramebufferMode::Stereo;

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
    if (isStereo && !hasStereoLayers(*color, renderPass.colorAttachments[i].layer)) {
      return Result(Result::Code::ArgumentInvalid, kStereoLayersError);
    }
    if (color->getSamples() != 1) {
      return Result(Result::Code::Unsupported,
                    "Subpasses do not support multisampled color attachments");
    }
    if (framebuffer.getResolveColorAttachment(i) != nullptr) {
      return Result(Result::Code::Unsupported,
                    "Subpasses do not support resolve color attachments");
    }
    if ((color->getUsage() & TextureDesc::TextureUsageBits::InputAttachment) == 0) {
      missingInputUsageMask |= 1u << i;
    }
  }
  const std::shared_ptr<ITexture> depth = framebuffer.getDepthAttachment();
  if (depth != nullptr && isStereo && !hasStereoLayers(*depth, renderPass.depthAttachment.layer)) {
    return Result(Result::Code::ArgumentInvalid, kStereoLayersError);
  }
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
  const std::vector<size_t> colorIndices = framebuffer.getColorAttachmentIndices();
  Result result = validateMemoryless(*this, framebuffer, colorIndices);
  if (!result.isOk() || subpasses.empty()) {
    return result;
  }
  return validateSubpasses(*this, framebuffer, colorIndices);
}

} // namespace igl
