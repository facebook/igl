/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Device.h>

#include <algorithm>
#include <igl/Framebuffer.h>

namespace igl {

bool IDevice::defaultVerifyScope() {
  return scopeDepth_ > 0;
}

TextureDesc IDevice::sanitize(const TextureDesc& desc) const {
  TextureDesc sanitized = desc;
  if (desc.width == 0 || desc.height == 0 || desc.depth == 0 || desc.numLayers == 0 ||
      desc.numSamples == 0 || desc.numMipLevels == 0) {
    sanitized.width = std::max(sanitized.width, 1u);
    sanitized.height = std::max(sanitized.height, 1u);
    sanitized.depth = std::max(sanitized.depth, 1u);
    sanitized.numLayers = std::max(sanitized.numLayers, 1u);
    sanitized.numSamples = std::max(sanitized.numSamples, 1u);
    sanitized.numMipLevels = std::max(sanitized.numMipLevels, 1u);
    IGL_LOG_ERROR(
        "width (%u), height (%u), depth (%u), numLayers (%u), numSamples (%u) and numMipLevels "
        "(%u) should be at least 1.\n",
        desc.width,
        desc.height,
        desc.depth,
        desc.numLayers,
        desc.numSamples,
        desc.numMipLevels);
  }

  return sanitized;
}

Color IDevice::backendDebugColor() const noexcept {
  switch (getBackendType()) {
  case BackendType::Invalid:
    return {0.f, 0.f, 0.f, 0.f}; // Clear
  case BackendType::OpenGL:
    return {1.f, 1.f, 0.f, 1.f}; // Yellow
  case BackendType::Metal:
    return {1.f, 0.f, 1.f, 1.f}; // Magenta
  case BackendType::Vulkan:
    return {0.f, 1.f, 1.f, 1.f}; // Cyan
  case BackendType::D3D12:
    return {0.f, 1.f, 1.f, 1.f}; // Match Vulkan for parity testing
  // @fb-only
    return {0.f, 1.f, 0.f, 1.f}; // Green @fb-only
  case BackendType::Custom:
    return {0.f, 0.f, 1.f, 1.f}; // Blue
  }
  IGL_UNREACHABLE_RETURN(Color(0.f, 0.f, 0.f, 0.f)) // Clear
}

DeviceScope::DeviceScope(IDevice& device) : device_(device) {
  device_.beginScope();
}

DeviceScope::~DeviceScope() {
  device_.endScope();
}

std::shared_ptr<IFramebuffer> IDevice::createFramebufferFromBaseDesc(
    const base::FramebufferInteropDesc& desc) {
  auto makeTextureDesc = [](const base::AttachmentInteropDesc& attachment) -> TextureDesc {
    return TextureDesc{
        .width = attachment.width,
        .height = attachment.height,
        .depth = attachment.depth,
        .numLayers = attachment.numLayers,
        .numSamples = attachment.numSamples,
        .usage = static_cast<TextureDesc::TextureUsage>(
            attachment.isSampled ? (TextureDesc::TextureUsageBits::Attachment |
                                    TextureDesc::TextureUsageBits::Sampled)
                                 : TextureDesc::TextureUsageBits::Attachment),
        .numMipLevels = attachment.numMipLevels,
        .type = attachment.type,
        .format = attachment.format,
        .storage = ResourceStorage::Private,
    };
  };

  FramebufferDesc fbDesc;

  for (size_t i = 0; i < base::kMaxColorAttachments; ++i) {
    const auto* attachmentDesc = desc.colorAttachments[i];
    if (attachmentDesc && attachmentDesc->format != TextureFormat::Invalid) {
      Result result;
      TextureDesc textureDesc = makeTextureDesc(*attachmentDesc);
      if (attachmentDesc->numLayers == 2) {
        fbDesc.mode = FramebufferMode::Stereo;
      }

      if (attachmentDesc->numSamples > 1) {
        TextureDesc resolveTextureDesc = textureDesc;
        textureDesc.usage =
            static_cast<TextureDesc::TextureUsage>(TextureDesc::TextureUsageBits::Attachment);
        resolveTextureDesc.numSamples = 1;

        auto resolveTexture = createTexture(resolveTextureDesc, &result);
        if (resolveTexture && result.isOk()) {
          fbDesc.colorAttachments[i].resolveTexture = std::move(resolveTexture);
        } else {
          return nullptr;
        }
      }

      auto texture = createTexture(textureDesc, &result);
      if (texture && result.isOk()) {
        fbDesc.colorAttachments[i].texture = std::move(texture);
      } else {
        return nullptr;
      }
    }
  }

  if (desc.depthAttachment && desc.depthAttachment->format != TextureFormat::Invalid) {
    Result result;
    TextureDesc textureDesc = makeTextureDesc(*desc.depthAttachment);
    if (desc.depthAttachment->numLayers == 2) {
      fbDesc.mode = FramebufferMode::Stereo;
    }

    if (desc.depthAttachment->numSamples > 1) {
      TextureDesc resolveTextureDesc = textureDesc;
      textureDesc.usage =
          static_cast<TextureDesc::TextureUsage>(TextureDesc::TextureUsageBits::Attachment);
      resolveTextureDesc.numSamples = 1;

      auto resolveTexture = createTexture(resolveTextureDesc, &result);
      if (resolveTexture && result.isOk()) {
        fbDesc.depthAttachment.resolveTexture = std::move(resolveTexture);
      } else {
        return nullptr;
      }
    }

    auto texture = createTexture(textureDesc, &result);
    if (texture && result.isOk()) {
      fbDesc.depthAttachment.texture = std::move(texture);
    } else {
      return nullptr;
    }
  }

  if (desc.depthAttachment == desc.stencilAttachment) {
    fbDesc.stencilAttachment = fbDesc.depthAttachment;
  } else if (desc.stencilAttachment && desc.stencilAttachment->format != TextureFormat::Invalid) {
    Result result;
    TextureDesc textureDesc = makeTextureDesc(*desc.stencilAttachment);
    if (desc.stencilAttachment->numLayers == 2) {
      fbDesc.mode = FramebufferMode::Stereo;
    }

    if (desc.stencilAttachment->numSamples > 1) {
      TextureDesc resolveTextureDesc = textureDesc;
      textureDesc.usage =
          static_cast<TextureDesc::TextureUsage>(TextureDesc::TextureUsageBits::Attachment);
      resolveTextureDesc.numSamples = 1;

      auto resolveTexture = createTexture(resolveTextureDesc, &result);
      if (resolveTexture && result.isOk()) {
        fbDesc.stencilAttachment.resolveTexture = std::move(resolveTexture);
      } else {
        return nullptr;
      }
    }

    auto texture = createTexture(textureDesc, &result);
    if (texture && result.isOk()) {
      fbDesc.stencilAttachment.texture = std::move(texture);
    } else {
      return nullptr;
    }
  }

  Result result;
  return createFramebuffer(fbDesc, &result);
}

} // namespace igl
