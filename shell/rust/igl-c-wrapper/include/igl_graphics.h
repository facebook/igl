/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef IGL_GRAPHICS_H
#define IGL_GRAPHICS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle types for IGL objects
typedef struct IGLDevice IGLDevice;
typedef struct IGLCommandQueue IGLCommandQueue;
typedef struct IGLCommandBuffer IGLCommandBuffer;
typedef struct IGLRenderCommandEncoder IGLRenderCommandEncoder;
typedef struct IGLBuffer IGLBuffer;
typedef struct IGLTexture IGLTexture;
typedef struct IGLFramebuffer IGLFramebuffer;
typedef struct IGLVertexInputState IGLVertexInputState;
typedef struct IGLShaderStages IGLShaderStages;
typedef struct IGLRenderPipelineState IGLRenderPipelineState;

// Enums
typedef enum {
    IGL_BACKEND_TYPE_INVALID = 0,
    IGL_BACKEND_TYPE_OPENGL = 1,
    IGL_BACKEND_TYPE_METAL = 2,
    IGL_BACKEND_TYPE_VULKAN = 3,
} IGLBackendType;

typedef enum {
    IGL_BUFFER_TYPE_VERTEX = 1,
    IGL_BUFFER_TYPE_INDEX = 2,
    IGL_BUFFER_TYPE_UNIFORM = 4,
} IGLBufferType;

typedef enum {
    IGL_VERTEX_FORMAT_FLOAT1 = 0,
    IGL_VERTEX_FORMAT_FLOAT2 = 1,
    IGL_VERTEX_FORMAT_FLOAT3 = 2,
    IGL_VERTEX_FORMAT_FLOAT4 = 3,
} IGLVertexFormat;

typedef enum {
    IGL_INDEX_FORMAT_UINT16 = 0,
    IGL_INDEX_FORMAT_UINT32 = 1,
} IGLIndexFormat;

typedef enum {
    IGL_CULL_MODE_NONE = 0,
    IGL_CULL_MODE_FRONT = 1,
    IGL_CULL_MODE_BACK = 2,
} IGLCullMode;

typedef enum {
    IGL_WINDING_MODE_CLOCKWISE = 0,
    IGL_WINDING_MODE_COUNTER_CLOCKWISE = 1,
} IGLWindingMode;

typedef enum {
    IGL_LOAD_ACTION_DONT_CARE = 0,
    IGL_LOAD_ACTION_LOAD = 1,
    IGL_LOAD_ACTION_CLEAR = 2,
} IGLLoadAction;

typedef enum {
    IGL_STORE_ACTION_DONT_CARE = 0,
    IGL_STORE_ACTION_STORE = 1,
} IGLStoreAction;

// Structs
typedef struct {
    float r, g, b, a;
} IGLColor;

typedef struct {
    uint32_t bufferIndex;
    IGLVertexFormat format;
    uint32_t offset;
    const char* name;
    int32_t location;
} IGLVertexAttribute;

typedef struct {
    uint32_t stride;
} IGLVertexBinding;

typedef struct {
    IGLLoadAction loadAction;
    IGLStoreAction storeAction;
    IGLColor clearColor;
} IGLColorAttachment;

typedef struct {
    IGLLoadAction loadAction;
    float clearDepth;
} IGLDepthAttachment;

// Device functions
IGLDevice* igl_platform_get_device(void* platform);
IGLBackendType igl_device_get_backend_type(IGLDevice* device);

// Command Queue
IGLCommandQueue* igl_device_create_command_queue(IGLDevice* device);
void igl_command_queue_destroy(IGLCommandQueue* queue);

// Command Buffer
IGLCommandBuffer* igl_command_queue_create_command_buffer(IGLCommandQueue* queue);
void igl_command_buffer_destroy(IGLCommandBuffer* buffer);
void igl_command_queue_submit(IGLCommandQueue* queue, IGLCommandBuffer* buffer);
void igl_command_buffer_present(IGLCommandBuffer* buffer, IGLTexture* texture);

// Buffer creation
IGLBuffer* igl_device_create_buffer(IGLDevice* device,
                                     IGLBufferType type,
                                     const void* data,
                                     uint32_t size);
void igl_buffer_destroy(IGLBuffer* buffer);

// Shader creation
IGLShaderStages* igl_device_create_shader_stages_metal(IGLDevice* device,
                                                        const char* source,
                                                        const char* vertex_entry,
                                                        const char* fragment_entry);
void igl_shader_stages_destroy(IGLShaderStages* stages);

// Vertex Input State
IGLVertexInputState* igl_device_create_vertex_input_state(
    IGLDevice* device,
    const IGLVertexAttribute* attributes,
    uint32_t num_attributes,
    const IGLVertexBinding* bindings,
    uint32_t num_bindings);
void igl_vertex_input_state_destroy(IGLVertexInputState* state);

// Framebuffer
IGLFramebuffer* igl_device_create_framebuffer(IGLDevice* device,
                                               IGLTexture* color_texture,
                                               IGLTexture* depth_texture);
void igl_framebuffer_destroy(IGLFramebuffer* framebuffer);
void igl_framebuffer_update_drawable(IGLFramebuffer* framebuffer, IGLTexture* color_texture);
IGLTexture* igl_framebuffer_get_color_attachment(IGLFramebuffer* framebuffer);

// Render Pipeline State
IGLRenderPipelineState* igl_device_create_render_pipeline(
    IGLDevice* device,
    IGLVertexInputState* vertex_input,
    IGLShaderStages* shaders,
    uint32_t color_attachment_format,
    uint32_t depth_attachment_format,
    IGLCullMode cull_mode,
    IGLWindingMode winding_mode);
void igl_render_pipeline_state_destroy(IGLRenderPipelineState* pipeline);

// Render Command Encoder
IGLRenderCommandEncoder* igl_command_buffer_create_render_encoder(
    IGLCommandBuffer* buffer,
    IGLFramebuffer* framebuffer,
    const IGLColorAttachment* color_attachment,
    const IGLDepthAttachment* depth_attachment);
void igl_render_encoder_end_encoding(IGLRenderCommandEncoder* encoder);
void igl_render_encoder_bind_vertex_buffer(IGLRenderCommandEncoder* encoder,
                                            uint32_t index,
                                            IGLBuffer* buffer);
void igl_render_encoder_bind_index_buffer(IGLRenderCommandEncoder* encoder,
                                           IGLBuffer* buffer,
                                           IGLIndexFormat format);
void igl_render_encoder_bind_pipeline(IGLRenderCommandEncoder* encoder,
                                       IGLRenderPipelineState* pipeline);
void igl_render_encoder_bind_uniform_buffer(IGLRenderCommandEncoder* encoder,
                                             uint32_t index,
                                             IGLBuffer* buffer);
void igl_render_encoder_draw_indexed(IGLRenderCommandEncoder* encoder, uint32_t index_count);

// Texture helpers
uint32_t igl_texture_get_format(IGLTexture* texture);
float igl_texture_get_aspect_ratio(IGLTexture* texture);

#ifdef __cplusplus
}
#endif

#endif // IGL_GRAPHICS_H
