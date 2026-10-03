/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/Texture.h>

#include <algorithm>
#include <optional>
#include <utility>
#include <igl/webgpu/DeviceFeatureSet.h>
#include <igl/webgpu/Readback.h>
#include <igl/webgpu/ResourcesBinder.h>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

struct Texture::Storage {
  Storage(WebGPUContext& ctx, Handle<WGPUTexture> texture) :
    ctx(ctx), texture(std::move(texture)) {}
  ~Storage() {
    ctx.getResourceTracker().retire(std::move(texture), lastUseSerial);
  }
  Storage(const Storage&) = delete;
  Storage& operator=(const Storage&) = delete;
  Storage(Storage&&) = delete;
  Storage& operator=(Storage&&) = delete;

  WebGPUContext& ctx;
  Handle<WGPUTexture> texture;
  uint64_t lastUseSerial = 0;
};

namespace {

uint32_t getWGPULayerCount(const TextureDesc& desc) {
  switch (desc.type) {
  case TextureType::Cube:
    return desc.numLayers * 6;
  case TextureType::ThreeD:
    return desc.depth;
  case TextureType::TwoD:
  case TextureType::TwoDArray:
  case TextureType::Invalid:
  case TextureType::ExternalImage:
    return desc.numLayers;
  }
  IGL_UNREACHABLE_RETURN(1)
}

WGPUTextureViewDimension getSampledViewDimension(const TextureDesc& desc) {
  switch (desc.type) {
  case TextureType::TwoD:
    return WGPUTextureViewDimension_2D;
  case TextureType::TwoDArray:
    return WGPUTextureViewDimension_2DArray;
  case TextureType::Cube:
    return desc.numLayers > 1 ? WGPUTextureViewDimension_CubeArray : WGPUTextureViewDimension_Cube;
  case TextureType::ThreeD:
    return WGPUTextureViewDimension_3D;
  case TextureType::Invalid:
  case TextureType::ExternalImage:
    return WGPUTextureViewDimension_Undefined;
  }
  IGL_UNREACHABLE_RETURN(WGPUTextureViewDimension_Undefined)
}

WGPUTextureAspect getSampledAspect(WGPUTextureFormat format) {
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (format) {
  case WGPUTextureFormat_Depth24PlusStencil8:
  case WGPUTextureFormat_Depth32FloatStencil8:
    return WGPUTextureAspect_DepthOnly;
  default:
    return WGPUTextureAspect_All;
  }
}

uint32_t roundUp(uint32_t value, uint32_t multiple) {
  return (value + multiple - 1) / multiple * multiple;
}

} // namespace

uint32_t getCopyBytesPerTexel(WGPUTextureFormat format, WGPUTextureAspect aspect) {
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (format) {
  case WGPUTextureFormat_Depth16Unorm:
    return 2;
  case WGPUTextureFormat_Depth32Float:
    return 4;
  case WGPUTextureFormat_Stencil8:
    return 1;
  case WGPUTextureFormat_Depth24Plus:
    return 0;
  case WGPUTextureFormat_Depth24PlusStencil8:
    return aspect == WGPUTextureAspect_StencilOnly ? 1 : 0;
  case WGPUTextureFormat_Depth32FloatStencil8:
    return aspect == WGPUTextureAspect_StencilOnly ? 1
           : aspect == WGPUTextureAspect_DepthOnly ? 4
                                                   : 0;
  default:
    break;
  }
  const TextureFormat iglFormat = wgpuTextureFormatToTextureFormat(format);
  if (iglFormat == TextureFormat::Invalid) {
    return 0;
  }
  const auto props = TextureFormatProperties::fromTextureFormat(iglFormat);
  return props.isCompressed() ? 0 : props.bytesPerBlock;
}

std::shared_ptr<Texture> Texture::create(WebGPUContext& ctx,
                                         const DeviceFeatureSet& features,
                                         const TextureDesc& desc,
                                         Result* IGL_NULLABLE outResult) {
  const std::optional<WGPUTextureFormat> format = textureFormatToWGPUTextureFormat(desc.format);
  const auto caps = features.getTextureFormatCapabilities(desc.format);
  if (!format || caps == ICapabilities::TextureFormatCapabilityBits::Unsupported) {
    Result::setResult(outResult, Result::Code::Unsupported, "Texture format is not supported");
    return nullptr;
  }
  const std::optional<WGPUTextureDimension> dimension =
      textureTypeToWGPUTextureDimension(desc.type);
  if (!dimension) {
    Result::setResult(outResult, Result::Code::Unsupported, "Texture type is not supported");
    return nullptr;
  }
  if (desc.numSamples != 1 && desc.numSamples != 4) {
    Result::setResult(outResult, Result::Code::Unsupported, "WebGPU textures have 1 or 4 samples");
    return nullptr;
  }
  if ((desc.usage & TextureDesc::TextureUsageBits::Storage) != 0) {
    Result::setResult(
        outResult, Result::Code::Unsupported, "Storage textures are not supported yet");
    return nullptr;
  }
  if (desc.exportability == TextureDesc::TextureExportability::Exportable) {
    Result::setResult(outResult, Result::Code::Unimplemented, "Exportable textures (WebGPU)");
    return nullptr;
  }

  WGPUTextureUsage usage = WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst;
  if ((desc.usage & TextureDesc::TextureUsageBits::Sampled) != 0) {
    usage |= WGPUTextureUsage_TextureBinding;
  }
  if ((desc.usage & TextureDesc::TextureUsageBits::Attachment) != 0) {
    // Requesting RENDER_ATTACHMENT for a format that cannot be rendered to is a validation error.
    if ((caps & ICapabilities::TextureFormatCapabilityBits::Attachment) == 0) {
      Result::setResult(outResult, Result::Code::Unsupported, "Texture format is not renderable");
      return nullptr;
    }
    usage |= WGPUTextureUsage_RenderAttachment;
  }

  WGPUTextureDescriptor textureDesc = WGPU_TEXTURE_DESCRIPTOR_INIT;
  textureDesc.label = toWGPUStringView(desc.debugName);
  textureDesc.usage = usage;
  textureDesc.dimension = *dimension;
  textureDesc.size = {
      .width = desc.width,
      .height = desc.height,
      .depthOrArrayLayers = getWGPULayerCount(desc),
  };
  textureDesc.format = *format;
  textureDesc.mipLevelCount = desc.numMipLevels;
  textureDesc.sampleCount = desc.numSamples;

  ctx.pushErrorScope(WGPUErrorFilter_OutOfMemory);
  ctx.pushErrorScope(WGPUErrorFilter_Validation);
  Handle<WGPUTexture> texture(wgpuDeviceCreateTexture(ctx.getDevice(), &textureDesc));
  Result validation = ctx.popErrorScope();
  Result outOfMemory = ctx.popErrorScope();
  if (!validation.isOk() || !outOfMemory.isOk() || !texture) {
    Result::setResult(outResult,
                      !validation.isOk() ? std::move(validation) : std::move(outOfMemory));
    return nullptr;
  }
  if (desc.mipmapGeneration == TextureDesc::TextureMipmapGeneration::AutoGenerateOnUpload &&
      desc.numMipLevels > 1) {
    IGL_LOG_INFO_ONCE("WebGPU does not generate mipmaps on upload yet\n");
  }

  auto result = std::shared_ptr<Texture>(
      new Texture(std::make_shared<Storage>(ctx, std::move(texture)), desc, *format, 0, 0));
  Result ret = result->createSampledView();
  if (!ret.isOk()) {
    Result::setResult(outResult, std::move(ret));
    return nullptr;
  }
  Result::setOk(outResult);
  return result;
}

std::shared_ptr<Texture> Texture::createView(std::shared_ptr<Texture> parent,
                                             const TextureViewDesc& desc,
                                             Result* IGL_NULLABLE outResult) {
  if (!parent) {
    Result::setResult(outResult, Result::Code::ArgumentNull, "A base texture is required");
    return nullptr;
  }
  if (desc.format != TextureFormat::Invalid && desc.format != parent->getFormat()) {
    Result::setResult(
        outResult, Result::Code::Unsupported, "Texture views cannot change the format");
    return nullptr;
  }
  if (!desc.swizzle.identity()) {
    Result::setResult(
        outResult, Result::Code::Unsupported, "WebGPU texture views cannot swizzle components");
    return nullptr;
  }
  const TextureFormatProperties& parentProps = parent->getProperties();
  const ImageAspectFlags sampledAspect = parentProps.isStencilOnly()      ? ImageAspectBits_Stencil
                                         : parentProps.isDepthOrStencil() ? ImageAspectBits_Depth
                                                                          : ImageAspectBits_Color;
  if (desc.aspect != ImageAspectBits_Invalid && desc.aspect != sampledAspect) {
    Result::setResult(outResult,
                      Result::Code::Unsupported,
                      "WebGPU texture views only expose the color, depth or stencil-only aspect");
    return nullptr;
  }
  if (desc.numMipLevels == 0 || desc.numLayers == 0 ||
      desc.mipLevel + desc.numMipLevels > parent->getNumMipLevels()) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Invalid texture view range");
    return nullptr;
  }

  const TextureDesc& parentDesc = parent->desc_;
  const uint32_t baseMipLevel = parent->baseMipLevel_ + desc.mipLevel;
  // Cube views count `layer` and `numLayers` in cubes of six array layers.
  const uint32_t firstLayer = desc.type == TextureType::Cube ? desc.layer * 6 : desc.layer;
  const uint32_t baseLayer = parent->baseLayer_ + firstLayer;
  const uint32_t layerCount = desc.type == TextureType::Cube ? desc.numLayers * 6 : desc.numLayers;
  if (desc.type != TextureType::ThreeD && firstLayer + layerCount > getWGPULayerCount(parentDesc)) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Invalid texture view layers");
    return nullptr;
  }

  TextureDesc viewDesc = parentDesc;
  viewDesc.type = desc.type;
  viewDesc.width = std::max(parentDesc.width >> desc.mipLevel, 1u);
  viewDesc.height = std::max(parentDesc.height >> desc.mipLevel, 1u);
  viewDesc.depth =
      desc.type == TextureType::ThreeD ? std::max(parentDesc.depth >> desc.mipLevel, 1u) : 1u;
  viewDesc.numLayers = desc.numLayers;
  viewDesc.numMipLevels = desc.numMipLevels;
  viewDesc.debugName = desc.debugName;

  auto result = std::shared_ptr<Texture>(
      new Texture(parent->storage_, viewDesc, parent->wgpuFormat_, baseMipLevel, baseLayer));
  Result ret = result->createSampledView();
  if (!ret.isOk()) {
    Result::setResult(outResult, std::move(ret));
    return nullptr;
  }
  Result::setOk(outResult);
  return result;
}

Texture::Texture(std::shared_ptr<Storage> storage,
                 const TextureDesc& desc,
                 WGPUTextureFormat wgpuFormat,
                 uint32_t baseMipLevel,
                 uint32_t baseLayer) :
  ITexture(desc.format),
  storage_(std::move(storage)),
  desc_(desc),
  wgpuFormat_(wgpuFormat),
  baseMipLevel_(baseMipLevel),
  baseLayer_(baseLayer),
  textureId_(allocateResourceId()) {}

Texture::~Texture() {
  storage_->ctx.evictBindGroups(textureId_);
}

Result Texture::createSampledView() {
  WGPUTextureViewDescriptor viewDesc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
  viewDesc.label = toWGPUStringView(desc_.debugName);
  // Undefined resolves to the format of the selected aspect.
  viewDesc.format = WGPUTextureFormat_Undefined;
  viewDesc.dimension = getSampledViewDimension(desc_);
  viewDesc.baseMipLevel = baseMipLevel_;
  viewDesc.mipLevelCount = desc_.numMipLevels;
  viewDesc.baseArrayLayer = desc_.type == TextureType::ThreeD ? 0 : baseLayer_;
  viewDesc.arrayLayerCount = desc_.type == TextureType::ThreeD ? 1 : getWGPULayerCount(desc_);
  viewDesc.aspect = getSampledAspect(wgpuFormat_);
  WebGPUContext& ctx = storage_->ctx;
  ctx.pushErrorScope(WGPUErrorFilter_Validation);
  sampledView_.reset(wgpuTextureCreateView(storage_->texture.get(), &viewDesc));
  return ctx.popErrorScope();
}

WGPUTextureView IGL_NULLABLE Texture::getAttachmentView(uint32_t mipLevel, uint32_t layer) const {
  const auto key = std::make_tuple(mipLevel, layer);
  if (const auto it = attachmentViews_.find(key); it != attachmentViews_.end()) {
    return it->second.get();
  }
  const bool is3D = desc_.type == TextureType::ThreeD;
  WGPUTextureViewDescriptor viewDesc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
  viewDesc.format = wgpuFormat_;
  viewDesc.dimension = is3D ? WGPUTextureViewDimension_3D : WGPUTextureViewDimension_2D;
  viewDesc.baseMipLevel = baseMipLevel_ + mipLevel;
  viewDesc.mipLevelCount = 1;
  viewDesc.baseArrayLayer = is3D ? 0 : baseLayer_ + layer;
  viewDesc.arrayLayerCount = 1;
  viewDesc.aspect = WGPUTextureAspect_All;
  Handle<WGPUTextureView> view(wgpuTextureCreateView(storage_->texture.get(), &viewDesc));
  const WGPUTextureView result = view.get();
  attachmentViews_.emplace(key, std::move(view));
  return result;
}

uint32_t Texture::getWGPULayer(uint32_t layer, uint32_t face) const noexcept {
  if (desc_.type == TextureType::ThreeD) {
    return 0;
  }
  return baseLayer_ + (desc_.type == TextureType::Cube ? layer * 6 + face : layer);
}

void Texture::recordUse(uint64_t serial) const noexcept {
  storage_->lastUseSerial = std::max(storage_->lastUseSerial, serial);
}

WGPUTexture IGL_NULLABLE Texture::getWGPUTexture() const noexcept {
  return storage_->texture.get();
}

Dimensions Texture::getDimensions() const {
  return Dimensions{desc_.width, desc_.height, desc_.depth};
}

uint32_t Texture::getNumLayers() const {
  return desc_.numLayers;
}

TextureType Texture::getType() const {
  return desc_.type;
}

TextureDesc::TextureUsage Texture::getUsage() const {
  return desc_.usage;
}

uint32_t Texture::getSamples() const {
  return desc_.numSamples;
}

uint32_t Texture::getNumMipLevels() const {
  return desc_.numMipLevels;
}

void Texture::generateMipmap(ICommandQueue& /*cmdQueue*/,
                             const TextureRangeDesc* IGL_NULLABLE /*range*/) const {
  IGL_LOG_ERROR_ONCE("generateMipmap() is not supported by the WebGPU backend yet\n");
}

void Texture::generateMipmap(ICommandBuffer& /*cmdBuffer*/,
                             const TextureRangeDesc* IGL_NULLABLE /*range*/) const {
  IGL_LOG_ERROR_ONCE("generateMipmap() is not supported by the WebGPU backend yet\n");
}

bool Texture::isRequiredGenerateMipmap() const {
  return false;
}

uint64_t Texture::getTextureId() const {
  return textureId_;
}

void* IGL_NULLABLE Texture::getNativeImage() const {
  return storage_->texture.get();
}

void* IGL_NULLABLE Texture::getNativeImageView() const {
  return sampledView_.get();
}

const base::AttachmentInteropDesc& Texture::getDesc() const {
  attachmentDesc_ = {
      .width = desc_.width,
      .height = desc_.height,
      .depth = desc_.depth,
      .numLayers = desc_.numLayers,
      .numSamples = desc_.numSamples,
      .numMipLevels = desc_.numMipLevels,
      .type = desc_.type,
      .format = desc_.format,
      .isSampled = (desc_.usage & TextureDesc::TextureUsageBits::Sampled) != 0,
  };
  return attachmentDesc_;
}

Result Texture::uploadInternal(TextureType type,
                               const TextureRangeDesc& range,
                               const void* IGL_NULLABLE data,
                               size_t bytesPerRow,
                               const uint32_t* IGL_NULLABLE /*mipLevelBytes*/) const {
  if (data == nullptr) {
    return Result();
  }
  const void* IGL_NONNULL nonNullData = static_cast<const void* IGL_NONNULL>(data);
  const TextureFormatProperties& props = getProperties();
  WebGPUContext& ctx = storage_->ctx;
  const bool is3D = type == TextureType::ThreeD;

  ctx.pushErrorScope(WGPUErrorFilter_Validation);
  for (uint32_t mip = range.mipLevel; mip < range.mipLevel + range.numMipLevels; ++mip) {
    const TextureRangeDesc mipRange = range.atMipLevel(mip);
    for (uint32_t layer = range.layer; layer < range.layer + range.numLayers; ++layer) {
      for (uint32_t face = range.face; face < range.face + range.numFaces; ++face) {
        const TextureRangeDesc subRange = mipRange.atLayer(layer).atFace(face);
        const uint32_t rowBytes = bytesPerRow != 0 ? static_cast<uint32_t>(bytesPerRow)
                                                   : props.getBytesPerRow(subRange);
        const uint32_t rows = props.getRows(subRange);
        const uint32_t depth = is3D ? subRange.depth : 1;
        const WGPUTexelCopyTextureInfo destination = {
            .texture = storage_->texture.get(),
            .mipLevel = baseMipLevel_ + mip,
            .origin = {.x = subRange.x,
                       .y = subRange.y,
                       .z = is3D ? subRange.z : getWGPULayer(layer, face)},
            .aspect = WGPUTextureAspect_All,
        };
        const WGPUTexelCopyBufferLayout layout = {
            .offset = 0,
            .bytesPerRow = rowBytes,
            .rowsPerImage = rows,
        };
        // Block-compressed copies cover whole blocks, even past the edge of a small mip level.
        const WGPUExtent3D extent = {
            .width = roundUp(subRange.width, props.blockWidth),
            .height = roundUp(subRange.height, props.blockHeight),
            .depthOrArrayLayers = depth,
        };
        const size_t dataSize = static_cast<size_t>(rowBytes) * rows * depth;
        wgpuQueueWriteTexture(ctx.getQueue(),
                              &destination,
                              getSubRangeStart(nonNullData, range, subRange, bytesPerRow),
                              dataSize,
                              &layout,
                              &extent);
      }
    }
  }
  return ctx.popErrorScope();
}

Result Texture::getBytes(const TextureRangeDesc& range,
                         WGPUTextureAspect aspect,
                         void* IGL_NONNULL outData,
                         size_t bytesPerRow,
                         bool flipVertically) const {
  const uint32_t bytesPerTexel = getCopyBytesPerTexel(wgpuFormat_, aspect);
  if (bytesPerTexel == 0) {
    return Result(Result::Code::Unsupported, "This texture aspect cannot be read back on WebGPU");
  }
  if (getSamples() != 1) {
    return Result(Result::Code::Unsupported, "Multisampled textures cannot be read back");
  }
  const bool is3D = desc_.type == TextureType::ThreeD;
  return readTexture(storage_->ctx,
                     {
                         .texture = storage_->texture.get(),
                         .aspect = aspect,
                         .mipLevel = baseMipLevel_ + range.mipLevel,
                         .layer = is3D ? range.z : getWGPULayer(range.layer, range.face),
                         .x = range.x,
                         .y = range.y,
                         .width = range.width,
                         .height = range.height,
                         .bytesPerTexel = bytesPerTexel,
                         .dstBytesPerRow = bytesPerRow,
                         .flipVertically = flipVertically,
                     },
                     outData);
}

} // namespace igl::webgpu
