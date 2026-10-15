/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/gpu_benchmark/TrafficModel.h>

#include <algorithm>
#include <unordered_set>
#include <utility>
#include <igl/Common.h>

namespace iglu::gpu_benchmark {

namespace {

[[nodiscard]] uint64_t byteCount(uint32_t bytesPerPixel,
                                 uint32_t samples,
                                 uint32_t width,
                                 uint32_t height,
                                 uint32_t layers) {
  return uint64_t{bytesPerPixel} * samples * width * height * layers;
}

[[nodiscard]] uint32_t extentAtMipLevel(uint32_t extent, uint32_t mipLevel) {
  constexpr uint32_t kBitsPerExtent = 32;
  return std::max(mipLevel < kBitsPerExtent ? extent >> mipLevel : 0u, 1u);
}

[[nodiscard]] igl::LoadAction mergeLoadActions(igl::LoadAction first, igl::LoadAction second) {
  if (first == igl::LoadAction::Load || second == igl::LoadAction::Load) {
    return igl::LoadAction::Load;
  }
  if (first == igl::LoadAction::Clear || second == igl::LoadAction::Clear) {
    return igl::LoadAction::Clear;
  }
  return igl::LoadAction::DontCare;
}

[[nodiscard]] uint32_t bytesPerPixelOrZero(const igl::ITexture* IGL_NULLABLE texture) {
  return texture != nullptr ? bytesPerPixel(texture->getFormat()) : 0u;
}

[[nodiscard]] AttachmentTraffic describeAttachment(
    const igl::ITexture& texture,
    const igl::ITexture* IGL_NULLABLE resolveTexture,
    const igl::RenderPassDesc::AttachmentDesc& attachmentDesc,
    uint32_t layers) {
  const igl::Dimensions dimensions = texture.getDimensions();
  return {
      .bytesPerPixel = bytesPerPixel(texture.getFormat()),
      .samples = std::max(texture.getSamples(), 1u),
      .width = extentAtMipLevel(dimensions.width, attachmentDesc.mipLevel),
      .height = extentAtMipLevel(dimensions.height, attachmentDesc.mipLevel),
      .layers = layers,
      .loadAction = attachmentDesc.loadAction,
      .storeAction = attachmentDesc.storeAction,
      .memoryless = texture.getStorage() == igl::ResourceStorage::Memoryless,
      .resolveBytesPerPixel = bytesPerPixelOrZero(resolveTexture),
  };
}

[[nodiscard]] SampledTexture describeSampledTexture(const igl::ITexture& texture) {
  const igl::TextureFormatProperties& properties = texture.getProperties();
  const igl::Dimensions dimensions = texture.getDimensions();
  return {
      .bytesPerPixel = properties.isCompressed() ? 0u : properties.bytesPerBlock,
      .width = dimensions.width,
      .height = dimensions.height,
      .bytes = properties.getBytesPerLayer(dimensions.width, dimensions.height, 1),
  };
}

[[nodiscard]] igl::RenderPassDesc::AttachmentDesc colorAttachmentDesc(
    const igl::RenderPassDesc& renderPass,
    size_t index) {
  IGL_DEBUG_ASSERT(index < renderPass.colorAttachments.size(),
                   "Color attachment %zu has no RenderPassDesc entry",
                   index);
  if (index < renderPass.colorAttachments.size()) {
    return renderPass.colorAttachments[index];
  }
  return {};
}

} // namespace

TrafficEstimate estimateTraffic(const std::vector<PassTraffic>& passes) {
  TrafficEstimate estimate;
  for (const PassTraffic& pass : passes) {
    for (const AttachmentTraffic& attachment : pass.attachments) {
      const uint32_t samples = std::max(attachment.samples, 1u);
      const uint32_t layers = std::max(attachment.layers, 1u);
      if (!attachment.memoryless) {
        const uint64_t storedBytes = byteCount(
            attachment.bytesPerPixel, samples, attachment.width, attachment.height, layers);
        if (attachment.loadAction == igl::LoadAction::Load) {
          estimate.readBytes += storedBytes;
        }
        if (attachment.storeAction == igl::StoreAction::Store) {
          estimate.writeBytes += storedBytes;
        }
      }
      if (attachment.storeAction == igl::StoreAction::MsaaResolve || attachment.alsoResolve) {
        estimate.writeBytes += byteCount(
            attachment.resolveBytesPerPixel, 1, attachment.width, attachment.height, layers);
      }
    }
    for (const SampledTexture& texture : pass.sampledTextures) {
      estimate.readBytes +=
          texture.bytes != 0
              ? texture.bytes
              : byteCount(texture.bytesPerPixel, 1, texture.width, texture.height, 1);
    }
  }
  return estimate;
}

uint32_t bytesPerPixel(igl::TextureFormat format) {
  const igl::TextureFormatProperties properties =
      igl::TextureFormatProperties::fromTextureFormat(format);
  if (properties.isCompressed()) {
    IGL_DEBUG_ABORT("Compressed formats have no whole number of bytes per pixel");
    return 0;
  }
  return properties.bytesPerBlock;
}

PassTraffic describeRenderPass(std::string name,
                               const igl::RenderPassDesc& renderPass,
                               const igl::IFramebuffer& framebuffer,
                               const std::vector<std::shared_ptr<igl::ITexture>>& sampledTextures) {
  PassTraffic pass{.name = std::move(name)};
  const uint32_t layers = framebuffer.getMode() == igl::FramebufferMode::Stereo ? 2u : 1u;

  for (const size_t index : framebuffer.getColorAttachmentIndices()) {
    const std::shared_ptr<igl::ITexture> color = framebuffer.getColorAttachment(index);
    if (!color) {
      continue;
    }
    const std::shared_ptr<igl::ITexture> resolve = framebuffer.getResolveColorAttachment(index);
    pass.attachments.push_back(
        describeAttachment(*color, resolve.get(), colorAttachmentDesc(renderPass, index), layers));
  }

  const std::shared_ptr<igl::ITexture> depth = framebuffer.getDepthAttachment();
  const std::shared_ptr<igl::ITexture> stencil = framebuffer.getStencilAttachment();
  if (depth) {
    const std::shared_ptr<igl::ITexture> resolve = framebuffer.getResolveDepthAttachment();
    AttachmentTraffic attachment =
        describeAttachment(*depth, resolve.get(), renderPass.depthAttachment, layers);
    if (stencil == depth) {
      const igl::RenderPassDesc::AttachmentDesc& stencilDesc = renderPass.stencilAttachment;
      const bool stores = attachment.storeAction == igl::StoreAction::Store ||
                          stencilDesc.storeAction == igl::StoreAction::Store;
      const bool resolves = attachment.storeAction == igl::StoreAction::MsaaResolve;
      attachment.loadAction = mergeLoadActions(attachment.loadAction, stencilDesc.loadAction);
      attachment.storeAction =
          stores ? igl::StoreAction::Store
                 : (resolves ? igl::StoreAction::MsaaResolve : igl::StoreAction::DontCare);
      attachment.alsoResolve = stores && resolves;
    }
    pass.attachments.push_back(attachment);
  }
  if (stencil && stencil != depth) {
    pass.attachments.push_back(
        describeAttachment(*stencil, nullptr, renderPass.stencilAttachment, layers));
  }

  pass.sampledTextures.reserve(sampledTextures.size());
  for (const std::shared_ptr<igl::ITexture>& texture : sampledTextures) {
    if (texture) {
      pass.sampledTextures.push_back(describeSampledTexture(*texture));
    }
  }
  return pass;
}

FootprintEstimate estimateFootprint(const std::vector<std::shared_ptr<igl::ITexture>>& textures,
                                    bool deviceHasMemorylessAttachments) {
  FootprintEstimate estimate;
  std::unordered_set<const igl::ITexture*> seen;
  for (const std::shared_ptr<igl::ITexture>& texture : textures) {
    if (!texture || !seen.insert(texture.get()).second) {
      continue;
    }
    const uint64_t textureBytes = static_cast<uint64_t>(texture->getEstimatedSizeInBytes()) *
                                  std::max(texture->getSamples(), 1u);
    if (texture->getStorage() != igl::ResourceStorage::Memoryless) {
      estimate.bytes += textureBytes;
      continue;
    }
    estimate.memorylessBytes += textureBytes;
    if (!deviceHasMemorylessAttachments) {
      estimate.bytes += textureBytes;
      estimate.memorylessBacked = true;
    }
  }
  return estimate;
}

} // namespace iglu::gpu_benchmark
