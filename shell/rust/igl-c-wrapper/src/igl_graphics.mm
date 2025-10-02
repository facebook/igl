/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "igl_graphics.h"
#include "igl_c_wrapper.h"
#include "igl_platform_internal.h"

#include <igl/IGL.h>
#include <igl/ShaderCreator.h>
#include <shell/shared/platform/Platform.h>
#include <IGLU/managedUniformBuffer/ManagedUniformBuffer.h>

extern "C" {

// Device functions
IGLDevice* igl_platform_get_device(void* platform) {
    auto* wrapper = static_cast<IGLPlatform*>(platform);
    if (!wrapper || !wrapper->platform) {
        return nullptr;
    }
    // Return pointer to the IDevice - getDevice() returns a reference, so take its address
    return reinterpret_cast<IGLDevice*>(const_cast<igl::IDevice*>(&wrapper->platform->getDevice()));
}

IGLBackendType igl_device_get_backend_type(IGLDevice* device) {
    auto* dev = reinterpret_cast<igl::IDevice*>(device);
    switch (dev->getBackendType()) {
        case igl::BackendType::Invalid:
            return IGL_BACKEND_TYPE_INVALID;
        case igl::BackendType::OpenGL:
            return IGL_BACKEND_TYPE_OPENGL;
        case igl::BackendType::Metal:
            return IGL_BACKEND_TYPE_METAL;
        case igl::BackendType::Vulkan:
            return IGL_BACKEND_TYPE_VULKAN;
        default:
            return IGL_BACKEND_TYPE_INVALID;
    }
}

// Command Queue
IGLCommandQueue* igl_device_create_command_queue(IGLDevice* device) {
    auto* dev = reinterpret_cast<igl::IDevice*>(device);
    auto queue = dev->createCommandQueue({}, nullptr);
    return reinterpret_cast<IGLCommandQueue*>(new std::shared_ptr<igl::ICommandQueue>(queue));
}

void igl_command_queue_destroy(IGLCommandQueue* queue) {
    delete reinterpret_cast<std::shared_ptr<igl::ICommandQueue>*>(queue);
}

// Command Buffer
IGLCommandBuffer* igl_command_queue_create_command_buffer(IGLCommandQueue* queue) {
    auto* q = reinterpret_cast<std::shared_ptr<igl::ICommandQueue>*>(queue);
    auto buffer = (*q)->createCommandBuffer({}, nullptr);
    return reinterpret_cast<IGLCommandBuffer*>(new std::shared_ptr<igl::ICommandBuffer>(buffer));
}

void igl_command_buffer_destroy(IGLCommandBuffer* buffer) {
    delete reinterpret_cast<std::shared_ptr<igl::ICommandBuffer>*>(buffer);
}

void igl_command_queue_submit(IGLCommandQueue* queue, IGLCommandBuffer* buffer) {
    auto* q = reinterpret_cast<std::shared_ptr<igl::ICommandQueue>*>(queue);
    auto* b = reinterpret_cast<std::shared_ptr<igl::ICommandBuffer>*>(buffer);
    (*q)->submit(**b);
}

void igl_command_buffer_present(IGLCommandBuffer* buffer, IGLTexture* texture) {
    auto* b = reinterpret_cast<std::shared_ptr<igl::ICommandBuffer>*>(buffer);
    auto* t = reinterpret_cast<std::shared_ptr<igl::ITexture>*>(texture);
    (*b)->present(*t);
}

// Buffer creation
IGLBuffer* igl_device_create_buffer(IGLDevice* device,
                                     IGLBufferType type,
                                     const void* data,
                                     uint32_t size) {
    printf("[C Wrapper] Creating buffer: type=%d, size=%u, data=%p, device=%p\n",
           (int)type, size, data, device);

    if (!device) {
        printf("[C Wrapper] ERROR: device is null!\n");
        return nullptr;
    }

    auto* dev = reinterpret_cast<igl::IDevice*>(device);
    printf("[C Wrapper] IDevice pointer: %p\n", dev);

    igl::BufferDesc::BufferTypeBits bufferType;
    switch (type) {
        case IGL_BUFFER_TYPE_VERTEX:
            bufferType = igl::BufferDesc::BufferTypeBits::Vertex;
            break;
        case IGL_BUFFER_TYPE_INDEX:
            bufferType = igl::BufferDesc::BufferTypeBits::Index;
            break;
        case IGL_BUFFER_TYPE_UNIFORM:
            bufferType = igl::BufferDesc::BufferTypeBits::Uniform;
            break;
        default:
            printf("[C Wrapper] ERROR: Invalid buffer type: %d\n", (int)type);
            return nullptr;
    }

    printf("[C Wrapper] Creating buffer with desc...\n");
    igl::BufferDesc desc(bufferType, data, size);
    auto buffer = dev->createBuffer(desc, nullptr);

    if (!buffer) {
        printf("[C Wrapper] ERROR: createBuffer returned null!\n");
        return nullptr;
    }

    printf("[C Wrapper] Buffer created successfully\n");
    return reinterpret_cast<IGLBuffer*>(new std::shared_ptr<igl::IBuffer>(std::move(buffer)));
}

void igl_buffer_destroy(IGLBuffer* buffer) {
    delete reinterpret_cast<std::shared_ptr<igl::IBuffer>*>(buffer);
}

// Shader creation
IGLShaderStages* igl_device_create_shader_stages_metal(IGLDevice* device,
                                                        const char* source,
                                                        const char* vertex_entry,
                                                        const char* fragment_entry) {
    auto* dev = reinterpret_cast<igl::IDevice*>(device);
    auto stages = igl::ShaderStagesCreator::fromLibraryStringInput(
        *dev, source, vertex_entry, fragment_entry, "", nullptr);
    return reinterpret_cast<IGLShaderStages*>(new std::unique_ptr<igl::IShaderStages>(std::move(stages)));
}

void igl_shader_stages_destroy(IGLShaderStages* stages) {
    delete reinterpret_cast<std::unique_ptr<igl::IShaderStages>*>(stages);
}

// Vertex Input State
IGLVertexInputState* igl_device_create_vertex_input_state(
    IGLDevice* device,
    const IGLVertexAttribute* attributes,
    uint32_t num_attributes,
    const IGLVertexBinding* bindings,
    uint32_t num_bindings) {

    auto* dev = reinterpret_cast<igl::IDevice*>(device);

    igl::VertexInputStateDesc desc;
    desc.numAttributes = num_attributes;
    desc.numInputBindings = num_bindings;

    for (uint32_t i = 0; i < num_attributes; ++i) {
        igl::VertexAttributeFormat format;
        switch (attributes[i].format) {
            case IGL_VERTEX_FORMAT_FLOAT1: format = igl::VertexAttributeFormat::Float1; break;
            case IGL_VERTEX_FORMAT_FLOAT2: format = igl::VertexAttributeFormat::Float2; break;
            case IGL_VERTEX_FORMAT_FLOAT3: format = igl::VertexAttributeFormat::Float3; break;
            case IGL_VERTEX_FORMAT_FLOAT4: format = igl::VertexAttributeFormat::Float4; break;
            default: format = igl::VertexAttributeFormat::Float3;
        }

        desc.attributes[i] = {
            .bufferIndex = attributes[i].bufferIndex,
            .format = format,
            .offset = attributes[i].offset,
            .name = attributes[i].name,
            .location = attributes[i].location,
        };
    }

    for (uint32_t i = 0; i < num_bindings; ++i) {
        desc.inputBindings[i] = {
            .stride = bindings[i].stride,
        };
    }

    auto state = dev->createVertexInputState(desc, nullptr);
    return reinterpret_cast<IGLVertexInputState*>(new std::shared_ptr<igl::IVertexInputState>(state));
}

void igl_vertex_input_state_destroy(IGLVertexInputState* state) {
    delete reinterpret_cast<std::shared_ptr<igl::IVertexInputState>*>(state);
}

// Framebuffer
IGLFramebuffer* igl_device_create_framebuffer(IGLDevice* device,
                                               IGLTexture* color_texture,
                                               IGLTexture* depth_texture) {
    auto* dev = reinterpret_cast<igl::IDevice*>(device);
    auto* color = reinterpret_cast<std::shared_ptr<igl::ITexture>*>(color_texture);
    auto* depth = reinterpret_cast<std::shared_ptr<igl::ITexture>*>(depth_texture);

    igl::FramebufferDesc desc;
    desc.colorAttachments[0].texture = *color;
    desc.depthAttachment.texture = *depth;

    auto framebuffer = dev->createFramebuffer(desc, nullptr);
    return reinterpret_cast<IGLFramebuffer*>(new std::shared_ptr<igl::IFramebuffer>(framebuffer));
}

void igl_framebuffer_destroy(IGLFramebuffer* framebuffer) {
    delete reinterpret_cast<std::shared_ptr<igl::IFramebuffer>*>(framebuffer);
}

void igl_framebuffer_update_drawable(IGLFramebuffer* framebuffer, IGLTexture* color_texture) {
    auto* fb = reinterpret_cast<std::shared_ptr<igl::IFramebuffer>*>(framebuffer);
    auto* color = reinterpret_cast<std::shared_ptr<igl::ITexture>*>(color_texture);
    (*fb)->updateDrawable(*color);
}

IGLTexture* igl_framebuffer_get_color_attachment(IGLFramebuffer* framebuffer) {
    auto* fb = reinterpret_cast<std::shared_ptr<igl::IFramebuffer>*>(framebuffer);
    auto texture = (*fb)->getColorAttachment(0);
    return reinterpret_cast<IGLTexture*>(new std::shared_ptr<igl::ITexture>(texture));
}

// Render Pipeline State
IGLRenderPipelineState* igl_device_create_render_pipeline(
    IGLDevice* device,
    IGLVertexInputState* vertex_input,
    IGLShaderStages* shaders,
    uint32_t color_attachment_format,
    uint32_t depth_attachment_format,
    IGLCullMode cull_mode,
    IGLWindingMode winding_mode) {

    auto* dev = reinterpret_cast<igl::IDevice*>(device);
    auto* vi = reinterpret_cast<std::shared_ptr<igl::IVertexInputState>*>(vertex_input);
    auto* ss = reinterpret_cast<std::unique_ptr<igl::IShaderStages>*>(shaders);

    igl::RenderPipelineDesc desc;
    desc.vertexInputState = *vi;
    desc.shaderStages = std::shared_ptr<igl::IShaderStages>(std::move(*ss));
    desc.targetDesc.colorAttachments.resize(1);
    desc.targetDesc.colorAttachments[0].textureFormat = static_cast<igl::TextureFormat>(color_attachment_format);
    desc.targetDesc.depthAttachmentFormat = static_cast<igl::TextureFormat>(depth_attachment_format);

    switch (cull_mode) {
        case IGL_CULL_MODE_NONE: desc.cullMode = igl::CullMode::Disabled; break;
        case IGL_CULL_MODE_FRONT: desc.cullMode = igl::CullMode::Front; break;
        case IGL_CULL_MODE_BACK: desc.cullMode = igl::CullMode::Back; break;
    }

    switch (winding_mode) {
        case IGL_WINDING_MODE_CLOCKWISE: desc.frontFaceWinding = igl::WindingMode::Clockwise; break;
        case IGL_WINDING_MODE_COUNTER_CLOCKWISE: desc.frontFaceWinding = igl::WindingMode::CounterClockwise; break;
    }

    auto pipeline = dev->createRenderPipeline(desc, nullptr);
    return reinterpret_cast<IGLRenderPipelineState*>(new std::shared_ptr<igl::IRenderPipelineState>(pipeline));
}

void igl_render_pipeline_state_destroy(IGLRenderPipelineState* pipeline) {
    delete reinterpret_cast<std::shared_ptr<igl::IRenderPipelineState>*>(pipeline);
}

// Render Command Encoder
IGLRenderCommandEncoder* igl_command_buffer_create_render_encoder(
    IGLCommandBuffer* buffer,
    IGLFramebuffer* framebuffer,
    const IGLColorAttachment* color_attachment,
    const IGLDepthAttachment* depth_attachment) {

    auto* b = reinterpret_cast<std::shared_ptr<igl::ICommandBuffer>*>(buffer);
    auto* fb = reinterpret_cast<std::shared_ptr<igl::IFramebuffer>*>(framebuffer);

    igl::RenderPassDesc renderPass;

    if (color_attachment) {
        renderPass.colorAttachments.resize(1);
        renderPass.colorAttachments[0].loadAction = static_cast<igl::LoadAction>(color_attachment->loadAction);
        renderPass.colorAttachments[0].storeAction = static_cast<igl::StoreAction>(color_attachment->storeAction);
        renderPass.colorAttachments[0].clearColor = {
            color_attachment->clearColor.r,
            color_attachment->clearColor.g,
            color_attachment->clearColor.b,
            color_attachment->clearColor.a
        };
    }

    if (depth_attachment) {
        renderPass.depthAttachment.loadAction = static_cast<igl::LoadAction>(depth_attachment->loadAction);
        renderPass.depthAttachment.clearDepth = depth_attachment->clearDepth;
    }

    auto encoder = (*b)->createRenderCommandEncoder(renderPass, *fb);
    return reinterpret_cast<IGLRenderCommandEncoder*>(new std::shared_ptr<igl::IRenderCommandEncoder>(std::move(encoder)));
}

void igl_render_encoder_end_encoding(IGLRenderCommandEncoder* encoder) {
    auto* enc = reinterpret_cast<std::shared_ptr<igl::IRenderCommandEncoder>*>(encoder);
    (*enc)->endEncoding();
    delete enc;
}

void igl_render_encoder_bind_vertex_buffer(IGLRenderCommandEncoder* encoder,
                                            uint32_t index,
                                            IGLBuffer* buffer) {
    auto* enc = reinterpret_cast<std::shared_ptr<igl::IRenderCommandEncoder>*>(encoder);
    auto* buf = reinterpret_cast<std::shared_ptr<igl::IBuffer>*>(buffer);
    printf("[C Wrapper RENDER] bindVertexBuffer(index=%u, buffer=%p)\n", index, buffer);
    (*enc)->bindVertexBuffer(index, **buf);
}

void igl_render_encoder_bind_index_buffer(IGLRenderCommandEncoder* encoder,
                                           IGLBuffer* buffer,
                                           IGLIndexFormat format) {
    auto* enc = reinterpret_cast<std::shared_ptr<igl::IRenderCommandEncoder>*>(encoder);
    auto* buf = reinterpret_cast<std::shared_ptr<igl::IBuffer>*>(buffer);

    igl::IndexFormat iglFormat = (format == IGL_INDEX_FORMAT_UINT16)
        ? igl::IndexFormat::UInt16
        : igl::IndexFormat::UInt32;

    printf("[C Wrapper RENDER] bindIndexBuffer(buffer=%p, format=%s)\n",
           buffer, (format == IGL_INDEX_FORMAT_UINT16) ? "UInt16" : "UInt32");
    (*enc)->bindIndexBuffer(**buf, iglFormat);
}

void igl_render_encoder_bind_pipeline(IGLRenderCommandEncoder* encoder,
                                       IGLRenderPipelineState* pipeline) {
    auto* enc = reinterpret_cast<std::shared_ptr<igl::IRenderCommandEncoder>*>(encoder);
    auto* pipe = reinterpret_cast<std::shared_ptr<igl::IRenderPipelineState>*>(pipeline);
    printf("[C Wrapper RENDER] bindRenderPipelineState(pipeline=%p)\n", pipeline);
    (*enc)->bindRenderPipelineState(*pipe);
}

void igl_render_encoder_bind_uniform_buffer(IGLRenderCommandEncoder* encoder,
                                             uint32_t index,
                                             IGLBuffer* buffer) {
    auto* enc = reinterpret_cast<std::shared_ptr<igl::IRenderCommandEncoder>*>(encoder);
    auto* buf = reinterpret_cast<std::shared_ptr<igl::IBuffer>*>(buffer);
    printf("[C Wrapper RENDER] bindBuffer(index=%u, uniformBuffer=%p)\n", index, buffer);
    (*enc)->bindBuffer(index, buf->get());
}

void igl_render_encoder_draw_indexed(IGLRenderCommandEncoder* encoder, uint32_t index_count) {
    auto* enc = reinterpret_cast<std::shared_ptr<igl::IRenderCommandEncoder>*>(encoder);
    printf("[C Wrapper RENDER] drawIndexed(indexCount=%u)\n", index_count);
    (*enc)->drawIndexed(index_count);
}

// Texture helpers
uint32_t igl_texture_get_format(IGLTexture* texture) {
    auto* tex = reinterpret_cast<std::shared_ptr<igl::ITexture>*>(texture);
    return static_cast<uint32_t>((*tex)->getProperties().format);
}

float igl_texture_get_aspect_ratio(IGLTexture* texture) {
    auto* tex = reinterpret_cast<std::shared_ptr<igl::ITexture>*>(texture);
    return (*tex)->getAspectRatio();
}

} // extern "C"
