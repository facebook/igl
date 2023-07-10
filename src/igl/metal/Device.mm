/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/Device.h>

#import <Foundation/Foundation.h>
#include <igl/metal/Buffer.h>
#include <igl/metal/BufferSynchronizationManager.h>
#include <igl/metal/CommandQueue.h>
#include <igl/metal/ComputePipelineState.h>
#include <igl/metal/DepthStencilState.h>
#include <igl/metal/Framebuffer.h>
#include <igl/metal/PlatformDevice.h>
#include <igl/metal/RenderPipelineState.h>
#include <igl/metal/Result.h>
#include <igl/metal/SamplerState.h>
#include <igl/metal/Shader.h>
#include <igl/metal/Texture.h>
#include <igl/metal/VertexInputState.h>
#include <sstream>
#include <unordered_set>

namespace igl {
namespace metal {

// https://developer.apple.com/documentation/quartzcore/cametallayer/2938720-maximumdrawablecount?language=objc
// Max number of Metal drawables in the resource pool managed by Core Animation is 3.
#define IGL_METAL_MAX_IN_FLIGHT_BUFFERS 3

Device::Device(id<MTLDevice> device) :
  device_(device), platformDevice_(*this), deviceFeatureSet_(device) {
  bufferSyncManager_ =
      std::make_shared<BufferSynchronizationManager>(IGL_METAL_MAX_IN_FLIGHT_BUFFERS);
}

Device::~Device() = default;

std::shared_ptr<ICommandQueue> Device::createCommandQueue(const CommandQueueDesc& /*desc*/,
                                                          Result* outResult) {
  id<MTLCommandQueue> metalObject = [device_ newCommandQueue];
  auto resource =
      std::make_shared<CommandQueue>(metalObject, bufferSyncManager_, deviceStatistics_);
  Result::setOk(outResult);
  return resource;
}

namespace {
id<MTLBuffer> createMetalBuffer(id<MTLDevice> device,
                                const BufferDesc& desc,
                                MTLResourceOptions options) {
  id<MTLBuffer> metalObject = nil;
  if (desc.data != nullptr) {
    metalObject = [device newBufferWithBytes:desc.data length:desc.length options:options];
  } else {
    metalObject = [device newBufferWithLength:desc.length options:options];
  }
  return metalObject;
}
}

std::unique_ptr<IBuffer> Device::createBuffer(const BufferDesc& desc,
                                              Result* outResult) const noexcept {
  if (desc.hint & BufferDesc::BufferAPIHintBits::Ring) {
    return createRingBuffer(desc, outResult);
  }
  if (desc.hint & BufferDesc::BufferAPIHintBits::NoCopy) {
    return createBufferNoCopy(desc, outResult);
  }
  MTLStorageMode storage = toMTLStorageMode(desc.storage);
  MTLResourceOptions options = MTLResourceOptionCPUCacheModeDefault | storage;

  id<MTLBuffer> metalObject = createMetalBuffer(device_, desc, options);
  std::unique_ptr<IBuffer> resource = std::make_unique<Buffer>(
      std::move(metalObject), options, desc.hint, 0 /* No accepted hints */);
  if (getResourceTracker()) {
    resource->initResourceTracker(getResourceTracker());
  }
  Result::setOk(outResult);
  return resource;
}

std::unique_ptr<IBuffer> Device::createRingBuffer(const BufferDesc& desc,
                                                  Result* outResult) const noexcept {
  MTLStorageMode storage = toMTLStorageMode(desc.storage);
  MTLResourceOptions options = MTLResourceOptionCPUCacheModeDefault | storage;

  // Create a ring of buffers
  std::vector<id<MTLBuffer>> bufferRing;
  for (size_t i = 0; i < bufferSyncManager_->getMaxInflightBuffers(); i++) {
    id<MTLBuffer> metalObject = createMetalBuffer(device_, desc, options);
    bufferRing.push_back(metalObject);
  }
  std::unique_ptr<IBuffer> resource =
      std::make_unique<RingBuffer>(std::move(bufferRing), options, bufferSyncManager_, desc.hint);

  if (getResourceTracker()) {
    resource->initResourceTracker(getResourceTracker());
  }
  Result::setOk(outResult);
  return resource;
}

std::unique_ptr<IBuffer> Device::createBufferNoCopy(const BufferDesc& desc,
                                                    Result* outResult) const {
  MTLStorageMode storage = toMTLStorageMode(desc.storage);

  typedef void (^Deallocator)(void* pointer, NSUInteger length);
  Deallocator deallocator = nil;
  MTLResourceOptions options = MTLResourceOptionCPUCacheModeDefault | storage;
  id<MTLBuffer> metalObject = [device_ newBufferWithBytesNoCopy:const_cast<void*>(desc.data)
                                                         length:desc.length
                                                        options:options
                                                    deallocator:deallocator];

  std::unique_ptr<IBuffer> resource = std::make_unique<Buffer>(
      metalObject, options, desc.hint, BufferDesc::BufferAPIHintBits::NoCopy);
  if (getResourceTracker()) {
    resource->initResourceTracker(getResourceTracker());
  }
  Result::setOk(outResult);
  return resource;
}

std::shared_ptr<ISamplerState> Device::createSamplerState(const SamplerStateDesc& desc,
                                                          Result* outResult) const {
  return platformDevice_.createSamplerState(desc, outResult);
}

std::shared_ptr<ITexture> Device::createTexture(const TextureDesc& desc,
                                                Result* outResult) const noexcept {
  const auto sanitized = sanitize(desc);

  MTLTextureDescriptor* metalDesc = [MTLTextureDescriptor new];
  metalDesc.textureType = Texture::convertType(sanitized.type, sanitized.numSamples);
  metalDesc.pixelFormat = Texture::textureFormatToMTLPixelFormat(sanitized.format);
  if (metalDesc.pixelFormat == MTLPixelFormatInvalid) {
    Result::setResult(
        outResult,
        Result::Code::Unsupported,
        "Invalid Texture Format : " +
            std::string(TextureFormatProperties::fromTextureFormat(sanitized.format).name));
    IGL_ASSERT_MSG(0, outResult->message.c_str());
    return nullptr;
  }
  metalDesc.width = sanitized.width;
  metalDesc.height = sanitized.height;
  metalDesc.depth = sanitized.depth;
  metalDesc.arrayLength = sanitized.numLayers;
  metalDesc.mipmapLevelCount = sanitized.numMipLevels;

  // The default sampleCount value is 1. If textureType is not MTLTextureType2DMultisample, this
  // value must be 1. Support for different sample count values varies by device. Call the
  // supportsTextureSampleCount: method to determine if your desired sample count value is
  // supported.
  IGL_ASSERT_MSG(sanitized.numSamples > 0, "Texture : Samples cannot be zero");
  metalDesc.sampleCount = 1;
  if (metalDesc.textureType == MTLTextureType2DMultisample) {
    metalDesc.mipmapLevelCount = 1;
    if (sanitized.numSamples > 0) {
      IGL_ASSERT([device_ supportsTextureSampleCount:sanitized.numSamples]);
      metalDesc.sampleCount = sanitized.numSamples;
    }
  }
#if TARGET_OS_OSX
  // MTLTextureType2DMultisampleArray not available in iOS
  if (@available(macOS 10.14, *)) {
    if (metalDesc.textureType == MTLTextureType2DMultisampleArray) {
      metalDesc.mipmapLevelCount = 1;
    }
  }
#endif
  metalDesc.usage = Texture::toMTLTextureUsage(sanitized.usage);
  metalDesc.storageMode = toMTLStorageMode(sanitized.storage);

  metalDesc.resourceOptions =
      MTLResourceCPUCacheModeDefaultCache | toMTLResourceStorageMode(sanitized.storage);

  id<MTLTexture> metalObject = [device_ newTextureWithDescriptor:metalDesc];
  if (!metalObject) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Failed to create Metal texture");
    IGL_ASSERT_MSG(0, outResult->message.c_str());
    return nullptr;
  }
  auto iglObject = std::make_shared<Texture>(metalObject);
  if (getResourceTracker()) {
    iglObject->initResourceTracker(getResourceTracker());
  }
  Result::setOk(outResult);
  return iglObject;
}

std::shared_ptr<igl::IVertexInputState> Device::createVertexInputState(
    const VertexInputStateDesc& desc,
    Result* outResult) const {
  // Avoid buffer overrun in numAttributes.
  if (desc.numAttributes > IGL_VERTEX_ATTRIBUTES_MAX) {
    Result::setResult(outResult,
                      Result::Code::ArgumentOutOfRange,
                      "numAttributes is too large in VertexInputStateDesc");
    IGL_ASSERT_MSG(0, outResult->message.c_str());
    return nullptr;
  }

  // Avoid buffer overrun in numInputBindings.
  if (desc.numInputBindings > IGL_VERTEX_BINDINGS_MAX) {
    Result::setResult(outResult,
                      Result::Code::ArgumentOutOfRange,
                      "numInputBindings is too large in VertexInputStateDesc");
    IGL_ASSERT_MSG(0, outResult->message.c_str());
    return nullptr;
  }

  // Verify that bufferIndex and location are in their respective ranges.
  std::unordered_set<int> bufferIndexSet;
  std::unordered_set<int> attributeLocationSet;
  for (int i = 0; i < desc.numAttributes; ++i) {
    size_t bufferIndex = desc.attributes[i].bufferIndex;
    if (bufferIndex >= IGL_VERTEX_BINDINGS_MAX) {
      Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "bufferIndex out of range");
      IGL_ASSERT_MSG(0, outResult->message.c_str());
      return nullptr;
    }

    int attribLocation = desc.attributes[i].location;
    if (attribLocation < 0 || attribLocation >= IGL_VERTEX_ATTRIBUTES_MAX) {
      Result::setResult(
          outResult, Result::Code::ArgumentOutOfRange, "attribute location out of range");
      IGL_ASSERT_MSG(0, outResult->message.c_str());
      return nullptr;
    }

    bufferIndexSet.insert(static_cast<int>(bufferIndex));
    attributeLocationSet.insert(attribLocation);
  }

  // Number of unique buffer indices should match the number of bindings
  if (bufferIndexSet.size() != desc.numInputBindings) {
    std::ostringstream msg;
    msg << "desc.numInputBindings : expected value is " << bufferIndexSet.size()
        << ", but actual value is " << desc.numInputBindings;
    Result::setResult(outResult, Result::Code::ArgumentInvalid, msg.str());
    IGL_ASSERT_MSG(0, outResult->message.c_str());
    return nullptr;
  }

  // Attribute locations must be unique.
  // So number of attribute locations should match the desc.numAttributes
  if (attributeLocationSet.size() != desc.numAttributes) {
    std::ostringstream msg;
    msg << "desc.numAttributes : expected value is " << attributeLocationSet.size()
        << ", but actual value is " << desc.numAttributes;
    Result::setResult(
        outResult, Result::Code::ArgumentInvalid, "attribute locations are not unique");
    IGL_ASSERT_MSG(0, outResult->message.c_str());
    return nullptr;
  }

  MTLVertexDescriptor* metalDesc = [MTLVertexDescriptor vertexDescriptor];
  if (metalDesc == nil) {
    Result::setResult(
        outResult, Result::Code::RuntimeError, "failed to create MTLVertexDescriptor");
    IGL_ASSERT_MSG(0, outResult->message.c_str());
    return nullptr;
  }

  // Validation completed. Populate the metal vertex descriptor.
  for (int i = 0; i < desc.numAttributes; ++i) {
    size_t bufferIndex = desc.attributes[i].bufferIndex;
    size_t dstAttribIndex = desc.attributes[i].location;

    metalDesc.attributes[dstAttribIndex].format =
        VertexInputState::convertAttributeFormat(desc.attributes[i].format);
    metalDesc.attributes[dstAttribIndex].bufferIndex = bufferIndex;
    metalDesc.attributes[dstAttribIndex].offset = desc.attributes[i].offset;

    metalDesc.layouts[bufferIndex].stride = desc.inputBindings[bufferIndex].stride;
    metalDesc.layouts[bufferIndex].stepFunction =
        VertexInputState::convertSampleFunction(desc.inputBindings[bufferIndex].sampleFunction);
    metalDesc.layouts[bufferIndex].stepRate = desc.inputBindings[bufferIndex].sampleRate;
  }

  auto iglObject = std::make_shared<VertexInputState>(metalDesc);

  Result::setOk(outResult);
  return iglObject;
}

std::shared_ptr<igl::IDepthStencilState> Device::createDepthStencilState(
    const DepthStencilStateDesc& desc,
    Result* outResult) const {
  MTLDepthStencilDescriptor* metalDesc = [MTLDepthStencilDescriptor new];

  metalDesc.depthCompareFunction = DepthStencilState::convertCompareFunction(desc.compareFunction);
  metalDesc.depthWriteEnabled = desc.isDepthWriteEnabled;
  metalDesc.frontFaceStencil = DepthStencilState::convertStencilDescriptor(desc.frontFaceStencil);
  metalDesc.backFaceStencil = DepthStencilState::convertStencilDescriptor(desc.backFaceStencil);
  id<MTLDepthStencilState> metalObject = [device_ newDepthStencilStateWithDescriptor:metalDesc];

  std::shared_ptr<DepthStencilState> iglObject = std::make_shared<DepthStencilState>(metalObject);

  Result::setOk(outResult);
  return iglObject;
}

std::shared_ptr<igl::IComputePipelineState> Device::createComputePipeline(
    const ComputePipelineDesc& desc,
    Result* outResult) const {
  NSError* error = nil;

  if (IGL_UNEXPECTED(desc.shaderStages == nullptr)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Missing shader stages");
    return nullptr;
  }
  if (!IGL_VERIFY(desc.shaderStages->getType() == ShaderStagesType::Compute)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Shader stages not for compute");
    return nullptr;
  }
  if (!IGL_VERIFY(desc.shaderStages->getComputeModule())) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Missing compute shader");
    return nullptr;
  }

  MTLComputePipelineDescriptor* descriptor = [[MTLComputePipelineDescriptor alloc] init];
  descriptor.computeFunction =
      static_cast<ShaderModule*>(desc.shaderStages->getComputeModule().get())->get();
  MTLComputePipelineReflection* reflection = nil;
  id<MTLComputePipelineState> metalObject =
      [device_ newComputePipelineStateWithDescriptor:descriptor
                                             options:MTLPipelineOptionNone
                                          reflection:&reflection
                                               error:&error];
  setResultFrom(outResult, error);
  if (error != nil) {
    return nullptr;
  }
  std::shared_ptr<ComputePipelineState> computePipelineState =
      std::make_shared<ComputePipelineState>(metalObject, reflection);

  return computePipelineState;
}

std::shared_ptr<igl::IRenderPipelineState> Device::createRenderPipeline(
    const RenderPipelineDesc& desc,
    Result* outResult) const {
  // TODO
  //  Size drawableSize = IGLNativeDrawableSize(layer_);
  //  graphicsDesc.viewportState.viewportCount = 1;
  //  graphicsDesc.viewportState.viewports[0] = (Viewport){0.0, 0.0, drawableSize.width,
  //  drawableSize.height, 0.0, 1.0};

  NSError* error = nil;
  MTLRenderPipelineDescriptor* metalDesc = [MTLRenderPipelineDescriptor new];

  metalDesc.sampleCount = desc.sampleCount;

  // (optional, can be null) Vertex input
  auto vertexInput = desc.vertexInputState;
  auto metalVertexInput = vertexInput ? static_cast<VertexInputState*>(vertexInput.get())->get()
                                      : nullptr;

  metalDesc.vertexDescriptor = metalVertexInput;

  if (!IGL_VERIFY(desc.shaderStages)) {
    Result::setResult(
        outResult, Result::Code::RuntimeError, "RenderPipeline requires shader stages");
    return nullptr;
  }
  if (!IGL_VERIFY(desc.shaderStages->getType() == ShaderStagesType::Render)) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Shader stages not for render");
    return nullptr;
  }

  // Vertex shader is required
  auto vertexModule = desc.shaderStages->getVertexModule();
  if (!IGL_VERIFY(vertexModule)) {
    Result::setResult(
        outResult, Result::Code::RuntimeError, "RenderPipeline requires vertex module");
    return nullptr;
  }

  auto vertexFunc = static_cast<ShaderModule*>(vertexModule.get());
  metalDesc.vertexFunction = vertexFunc->get();

  if (!IGL_VERIFY(metalDesc.vertexFunction)) {
    Result::setResult(
        outResult, Result::Code::RuntimeError, "RenderPipeline requires non-null vertex function");
    return nullptr;
  }

  // Fragment shader is optional
  auto fragmentModule = desc.shaderStages->getFragmentModule();
  if (fragmentModule) {
    auto fragmentFunc = static_cast<ShaderModule*>(fragmentModule.get());
    metalDesc.fragmentFunction = fragmentFunc->get();
  }

  // Framebuffer
  for (uint32_t i = 0; i < desc.targetDesc.colorAttachments.size(); ++i) {
    auto& src = desc.targetDesc.colorAttachments[i];
    MTLRenderPipelineColorAttachmentDescriptor* dst = metalDesc.colorAttachments[i];
    dst.pixelFormat = Texture::textureFormatToMTLPixelFormat(src.textureFormat);
    dst.writeMask = RenderPipelineState::convertColorWriteMask(src.colorWriteBits);
    dst.blendingEnabled = src.blendEnabled;
    dst.rgbBlendOperation = MTLBlendOperation(src.rgbBlendOp);
    dst.alphaBlendOperation = MTLBlendOperation(src.alphaBlendOp);
    dst.sourceRGBBlendFactor = MTLBlendFactor(src.srcRGBBlendFactor);
    dst.sourceAlphaBlendFactor = MTLBlendFactor(src.srcAlphaBlendFactor);
    dst.destinationRGBBlendFactor = MTLBlendFactor(src.dstRGBBlendFactor);
    dst.destinationAlphaBlendFactor = MTLBlendFactor(src.dstAlphaBlendFactor);
  }

  // Depth and Stencil
  metalDesc.depthAttachmentPixelFormat =
      Texture::textureFormatToMTLPixelFormat(desc.targetDesc.depthAttachmentFormat);
  metalDesc.stencilAttachmentPixelFormat =
      Texture::textureFormatToMTLPixelFormat(desc.targetDesc.stencilAttachmentFormat);

  MTLRenderPipelineReflection* reflection = nil;

  // Create reflection for use later in binding, etc.
  id<MTLRenderPipelineState> metalObject =
      [device_ newRenderPipelineStateWithDescriptor:metalDesc
                                            options:MTLPipelineOptionArgumentInfo |
                                                    MTLPipelineOptionBufferTypeInfo
                                         reflection:&reflection
                                              error:&error];
  setResultFrom(outResult, error);
  if (error != nil) {
    IGL_LOG_ERROR("%s\n", [error.localizedDescription UTF8String]);
    return nullptr;
  }

  return std::make_shared<RenderPipelineState>(
      metalObject, reflection, desc.cullMode, desc.frontFaceWinding, desc.polygonFillMode);
}

std::unique_ptr<IShaderLibrary> Device::createShaderLibrary(const ShaderLibraryDesc& desc,
                                                            Result* outResult) const {
  if (IGL_UNEXPECTED(desc.moduleInfo.empty())) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid);
    return nullptr;
  }
  id<MTLLibrary> metalLibrary = nil;
  NSError* error = nil;
  if (desc.input.type == ShaderInputType::Binary) {
    if (desc.input.length == 0 || desc.input.data == nullptr) {
      Result::setResult(outResult, Result::Code::ArgumentNull);
      return nullptr;
    }
    // With null queue and destructor, dispatch_data_create() function stores a pointer to the data
    // buffer and leaves the responsibility of releasing the buffer to the client
    auto data = dispatch_data_create(desc.input.data,
                                     desc.input.length,
                                     nullptr /* dispatch_queue_t queue */,
                                     nullptr /* dispatch_block_t
                      destructor */
    );

    metalLibrary = [device_ newLibraryWithData:data error:&error];
  } else {
    if (!desc.input.source || !strlen(desc.input.source)) {
      Result::setResult(outResult, Result::Code::ArgumentNull);
      return nullptr;
    }
    MTLCompileOptions* compileOpts = [MTLCompileOptions new];
    compileOpts.fastMathEnabled = desc.input.options.fastMathEnabled;

    NSString* shaderSource = [NSString stringWithUTF8String:desc.input.source];
    metalLibrary = [device_ newLibraryWithSource:shaderSource options:compileOpts error:&error];
  }

  if (!metalLibrary) {
    IGL_ASSERT_MSG(!error, "%s\n", [error.localizedDescription UTF8String]);
    setResultFrom(outResult, error);
    return nullptr;
  }
  if (error) {
    // Compilation successful but with warnings
    IGL_LOG_INFO("%s\n", [error.localizedDescription UTF8String]);
  }
  std::vector<std::shared_ptr<IShaderModule>> modules;
  modules.reserve(desc.moduleInfo.size());
  for (const auto& info : desc.moduleInfo) {
    NSString* shaderEntrypoint = [NSString stringWithUTF8String:info.entryPoint.c_str()];
    if (!shaderEntrypoint) {
      Result::setResult(outResult, Result::Code::RuntimeError);
      return nullptr;
    }

    auto metalFunction = [metalLibrary newFunctionWithName:shaderEntrypoint];
    if (!metalFunction) {
      IGL_ASSERT_MSG(0, "Could not find function '%s' in library\n", info.entryPoint.c_str());
      Result::setResult(
          outResult, Result::Code::RuntimeError, "Could not find function in library");
      return nullptr;
    }
    modules.emplace_back(std::make_shared<metal::ShaderModule>(info, metalFunction));
  }

  auto shaderLibrary = std::make_unique<ShaderLibrary>(std::move(modules));
  if (auto resourceTracker = getResourceTracker()) {
    shaderLibrary->initResourceTracker(resourceTracker);
  }
  Result::setOk(outResult);

  return shaderLibrary;
}

std::shared_ptr<IShaderModule> Device::createShaderModule(const ShaderModuleDesc& desc,
                                                          Result* outResult) const {
  auto libraryDesc =
      desc.input.type == ShaderInputType::String
          ? ShaderLibraryDesc::fromStringInput(desc.input.source, {desc.info}, desc.debugName)
          : ShaderLibraryDesc::fromBinaryInput(
                desc.input.data, desc.input.length, {desc.info}, desc.debugName);
  auto library = createShaderLibrary(libraryDesc, outResult);
  if (library != nullptr) {
    return library->getShaderModule(desc.info.entryPoint);
  }
  return nullptr;
}

std::unique_ptr<IShaderStages> Device::createShaderStages(const ShaderStagesDesc& desc,
                                                          Result* outResult) const {
  Result result;
  auto stages = std::make_unique<ShaderStages>(desc);
  if (auto resourceTracker = getResourceTracker()) {
    stages->initResourceTracker(resourceTracker);
  }
  Result::setResult(outResult, result.code, result.message);
  if (!result.isOk()) {
    return nullptr;
  }

  return std::move(stages);
}

const PlatformDevice& Device::getPlatformDevice() const noexcept {
  return platformDevice_;
}

bool Device::hasFeature(DeviceFeatures feature) const {
  return deviceFeatureSet_.hasFeature(feature);
}

bool Device::hasRequirement(DeviceRequirement requirement) const {
  return deviceFeatureSet_.hasRequirement(requirement);
}

bool Device::getFeatureLimits(DeviceFeatureLimits featureLimits, size_t& result) const {
  return deviceFeatureSet_.getFeatureLimits(featureLimits, result);
}

ICapabilities::TextureFormatCapabilities Device::getTextureFormatCapabilities(
    TextureFormat format) const {
  return deviceFeatureSet_.getTextureFormatCapabilities(format);
}

ShaderVersion Device::getShaderVersion() const {
  // From https://developer.apple.com/documentation/metal/mtllanguageversion
  ShaderVersion version{.family = ShaderFamily::Metal};
#if IGL_PLATFORM_IOS
  std::vector<DeviceFeatureDesc> featureSet;
  if (@available(iOS 15, *)) {
    version.majorVersion = 2;
    version.minorVersion = 4;
  } else if (@available(iOS 14, *)) {
    version.majorVersion = 2;
    version.minorVersion = 3;
  } else if (@available(iOS 13, *)) {
    version.majorVersion = 2;
    version.minorVersion = 2;
  } else if (@available(iOS 12, *)) {
    version.majorVersion = 2;
    version.minorVersion = 1;
  } else if (@available(iOS 11, *)) {
    version.majorVersion = 2;
    version.minorVersion = 0;
  } else if (@available(iOS 10, *)) {
    version.majorVersion = 1;
    version.minorVersion = 2;
  } else if (@available(iOS 9, *)) {
    version.majorVersion = 1;
    version.minorVersion = 1;
  }
#elif IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
  if (@available(macOS 12.0, *)) {
    version.majorVersion = 2;
    version.minorVersion = 4;
  } else if (@available(macOS 11.0, *)) {
    version.majorVersion = 2;
    version.minorVersion = 3;
  } else if (@available(macOS 10.15, *)) {
    version.majorVersion = 2;
    version.minorVersion = 2;
  } else if (@available(macOS 10.14, *)) {
    version.majorVersion = 2;
    version.minorVersion = 1;
  } else if (@available(macOS 10.13, *)) {
    version.majorVersion = 2;
    version.minorVersion = 0;
  } else if (@available(macOS 10.12, *)) {
    version.majorVersion = 1;
    version.minorVersion = 2;
  } else if (@available(macOS 10.11, *)) {
    version.majorVersion = 1;
    version.minorVersion = 1;
  }
#endif
  return version;
}

size_t Device::getCurrentDrawCount() const {
  return deviceStatistics_.getDrawCount();
}

MTLStorageMode Device::toMTLStorageMode(ResourceStorage storage) {
  switch (storage) {
  case ResourceStorage::Private:
    return MTLStorageModePrivate;
  case ResourceStorage::Shared:
    return MTLStorageModeShared;
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
  case ResourceStorage::Managed:
  default:
    return MTLStorageModeManaged;
#else
  case ResourceStorage::Memoryless:
    return MTLStorageModeMemoryless;
  default:
    return MTLStorageModeShared;
#endif
  }
}

MTLResourceOptions Device::toMTLResourceStorageMode(ResourceStorage storage) {
  switch (storage) {
  case ResourceStorage::Private:
    return MTLResourceStorageModePrivate;
  case ResourceStorage::Shared:
    return MTLResourceStorageModeShared;
#if IGL_PLATFORM_MACOS || IGL_PLATFORM_MACCATALYST
  case ResourceStorage::Managed:
  default:
    return MTLResourceStorageModeManaged;
#else
  case ResourceStorage::Memoryless:
    return MTLResourceStorageModeMemoryless;
  default:
    return MTLResourceStorageModeShared;
#endif
  }
}
} // namespace metal
} // namespace igl
