//! Low-level FFI bindings for IGL graphics primitives

use std::os::raw::{c_char, c_float, c_int, c_uint, c_void};

// Opaque handle types
#[repr(C)]
pub struct IGLDevice {
    _private: [u8; 0],
}

#[repr(C)]
pub struct IGLCommandQueue {
    _private: [u8; 0],
}

#[repr(C)]
pub struct IGLCommandBuffer {
    _private: [u8; 0],
}

#[repr(C)]
pub struct IGLRenderCommandEncoder {
    _private: [u8; 0],
}

#[repr(C)]
pub struct IGLBuffer {
    _private: [u8; 0],
}

#[repr(C)]
pub struct IGLTexture {
    _private: [u8; 0],
}

#[repr(C)]
pub struct IGLFramebuffer {
    _private: [u8; 0],
}

#[repr(C)]
pub struct IGLVertexInputState {
    _private: [u8; 0],
}

#[repr(C)]
pub struct IGLShaderStages {
    _private: [u8; 0],
}

#[repr(C)]
pub struct IGLRenderPipelineState {
    _private: [u8; 0],
}

// Enums
#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum IGLBackendType {
    Invalid = 0,
    OpenGL = 1,
    Metal = 2,
    Vulkan = 3,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum IGLBufferType {
    Vertex = 1,
    Index = 2,
    Uniform = 4,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum IGLVertexFormat {
    Float1 = 0,
    Float2 = 1,
    Float3 = 2,
    Float4 = 3,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum IGLIndexFormat {
    UInt16 = 0,
    UInt32 = 1,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum IGLCullMode {
    None = 0,
    Front = 1,
    Back = 2,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum IGLWindingMode {
    Clockwise = 0,
    CounterClockwise = 1,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum IGLLoadAction {
    DontCare = 0,
    Load = 1,
    Clear = 2,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum IGLStoreAction {
    DontCare = 0,
    Store = 1,
}

// Structs
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct IGLColor {
    pub r: c_float,
    pub g: c_float,
    pub b: c_float,
    pub a: c_float,
}

#[repr(C)]
#[derive(Debug, Clone)]
pub struct IGLVertexAttribute {
    pub buffer_index: c_uint,
    pub format: IGLVertexFormat,
    pub offset: c_uint,
    pub name: *const c_char,
    pub location: c_int,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct IGLVertexBinding {
    pub stride: c_uint,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct IGLColorAttachment {
    pub load_action: IGLLoadAction,
    pub store_action: IGLStoreAction,
    pub clear_color: IGLColor,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct IGLDepthAttachment {
    pub load_action: IGLLoadAction,
    pub clear_depth: c_float,
}

extern "C" {
    // Device functions
    pub fn igl_platform_get_device(platform: *mut c_void) -> *mut IGLDevice;
    pub fn igl_device_get_backend_type(device: *mut IGLDevice) -> IGLBackendType;

    // Command Queue
    pub fn igl_device_create_command_queue(device: *mut IGLDevice) -> *mut IGLCommandQueue;
    pub fn igl_command_queue_destroy(queue: *mut IGLCommandQueue);

    // Command Buffer
    pub fn igl_command_queue_create_command_buffer(queue: *mut IGLCommandQueue) -> *mut IGLCommandBuffer;
    pub fn igl_command_buffer_destroy(buffer: *mut IGLCommandBuffer);
    pub fn igl_command_queue_submit(queue: *mut IGLCommandQueue, buffer: *mut IGLCommandBuffer);
    pub fn igl_command_buffer_present(buffer: *mut IGLCommandBuffer, texture: *mut IGLTexture);

    // Buffer creation
    pub fn igl_device_create_buffer(
        device: *mut IGLDevice,
        buffer_type: IGLBufferType,
        data: *const c_void,
        size: c_uint,
    ) -> *mut IGLBuffer;
    pub fn igl_buffer_destroy(buffer: *mut IGLBuffer);

    // Shader creation
    pub fn igl_device_create_shader_stages_metal(
        device: *mut IGLDevice,
        source: *const c_char,
        vertex_entry: *const c_char,
        fragment_entry: *const c_char,
    ) -> *mut IGLShaderStages;
    pub fn igl_shader_stages_destroy(stages: *mut IGLShaderStages);

    // Vertex Input State
    pub fn igl_device_create_vertex_input_state(
        device: *mut IGLDevice,
        attributes: *const IGLVertexAttribute,
        num_attributes: c_uint,
        bindings: *const IGLVertexBinding,
        num_bindings: c_uint,
    ) -> *mut IGLVertexInputState;
    pub fn igl_vertex_input_state_destroy(state: *mut IGLVertexInputState);

    // Framebuffer
    pub fn igl_device_create_framebuffer(
        device: *mut IGLDevice,
        color_texture: *mut IGLTexture,
        depth_texture: *mut IGLTexture,
    ) -> *mut IGLFramebuffer;
    pub fn igl_framebuffer_destroy(framebuffer: *mut IGLFramebuffer);
    pub fn igl_framebuffer_update_drawable(framebuffer: *mut IGLFramebuffer, color_texture: *mut IGLTexture);
    pub fn igl_framebuffer_get_color_attachment(framebuffer: *mut IGLFramebuffer) -> *mut IGLTexture;

    // Render Pipeline State
    pub fn igl_device_create_render_pipeline(
        device: *mut IGLDevice,
        vertex_input: *mut IGLVertexInputState,
        shaders: *mut IGLShaderStages,
        color_attachment_format: c_uint,
        depth_attachment_format: c_uint,
        cull_mode: IGLCullMode,
        winding_mode: IGLWindingMode,
    ) -> *mut IGLRenderPipelineState;
    pub fn igl_render_pipeline_state_destroy(pipeline: *mut IGLRenderPipelineState);

    // Render Command Encoder
    pub fn igl_command_buffer_create_render_encoder(
        buffer: *mut IGLCommandBuffer,
        framebuffer: *mut IGLFramebuffer,
        color_attachment: *const IGLColorAttachment,
        depth_attachment: *const IGLDepthAttachment,
    ) -> *mut IGLRenderCommandEncoder;
    pub fn igl_render_encoder_end_encoding(encoder: *mut IGLRenderCommandEncoder);
    pub fn igl_render_encoder_bind_vertex_buffer(
        encoder: *mut IGLRenderCommandEncoder,
        index: c_uint,
        buffer: *mut IGLBuffer,
    );
    pub fn igl_render_encoder_bind_index_buffer(
        encoder: *mut IGLRenderCommandEncoder,
        buffer: *mut IGLBuffer,
        format: IGLIndexFormat,
    );
    pub fn igl_render_encoder_bind_pipeline(
        encoder: *mut IGLRenderCommandEncoder,
        pipeline: *mut IGLRenderPipelineState,
    );
    pub fn igl_render_encoder_bind_uniform_buffer(
        encoder: *mut IGLRenderCommandEncoder,
        index: c_uint,
        buffer: *mut IGLBuffer,
    );
    pub fn igl_render_encoder_draw_indexed(encoder: *mut IGLRenderCommandEncoder, index_count: c_uint);

    // Texture helpers
    pub fn igl_texture_get_format(texture: *mut IGLTexture) -> c_uint;
    pub fn igl_texture_get_aspect_ratio(texture: *mut IGLTexture) -> c_float;
}
