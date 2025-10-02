//! Safe Rust wrappers around IGL graphics primitives

use igl_sys::graphics::*;
use std::ffi::CString;
use std::os::raw::c_void;
use std::ptr;

use crate::{Error, Result};

/// Device represents the graphics device
pub struct Device {
    pub(crate) handle: *mut IGLDevice,
    // Not owned, just a reference
}

impl Device {
    pub(crate) fn from_platform(platform: *mut c_void) -> Result<Self> {
        let handle = unsafe { igl_platform_get_device(platform) };
        if handle.is_null() {
            return Err(Error::NullPointer);
        }
        Ok(Device { handle })
    }

    pub fn backend_type(&self) -> BackendType {
        unsafe { igl_device_get_backend_type(self.handle).into() }
    }

    pub fn create_command_queue(&self) -> Result<CommandQueue> {
        CommandQueue::new(self)
    }

    pub fn create_buffer(&self, buffer_type: BufferType, data: &[u8]) -> Result<Buffer> {
        Buffer::new(self, buffer_type, data)
    }

    pub fn create_shader_stages_metal(
        &self,
        source: &str,
        vertex_entry: &str,
        fragment_entry: &str,
    ) -> Result<ShaderStages> {
        ShaderStages::new_metal(self, source, vertex_entry, fragment_entry)
    }

    pub fn create_vertex_input_state(
        &self,
        attributes: &[VertexAttribute],
        bindings: &[VertexBinding],
    ) -> Result<VertexInputState> {
        VertexInputState::new(self, attributes, bindings)
    }

    pub fn create_framebuffer(
        &self,
        color_texture: &Texture,
        depth_texture: &Texture,
    ) -> Result<Framebuffer> {
        Framebuffer::new(self, color_texture, depth_texture)
    }

    pub fn create_render_pipeline(
        &self,
        vertex_input: &VertexInputState,
        shaders: &ShaderStages,
        color_format: TextureFormat,
        depth_format: TextureFormat,
        cull_mode: CullMode,
        winding_mode: WindingMode,
    ) -> Result<RenderPipelineState> {
        RenderPipelineState::new(
            self,
            vertex_input,
            shaders,
            color_format,
            depth_format,
            cull_mode,
            winding_mode,
        )
    }
}

// Not Send/Sync - tied to graphics context
unsafe impl Send for Device {}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum BackendType {
    Invalid,
    OpenGL,
    Metal,
    Vulkan,
}

impl From<IGLBackendType> for BackendType {
    fn from(t: IGLBackendType) -> Self {
        match t {
            IGLBackendType::Invalid => BackendType::Invalid,
            IGLBackendType::OpenGL => BackendType::OpenGL,
            IGLBackendType::Metal => BackendType::Metal,
            IGLBackendType::Vulkan => BackendType::Vulkan,
        }
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum BufferType {
    Vertex,
    Index,
    Uniform,
}

impl From<BufferType> for IGLBufferType {
    fn from(t: BufferType) -> Self {
        match t {
            BufferType::Vertex => IGLBufferType::Vertex,
            BufferType::Index => IGLBufferType::Index,
            BufferType::Uniform => IGLBufferType::Uniform,
        }
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum VertexFormat {
    Float1,
    Float2,
    Float3,
    Float4,
}

impl From<VertexFormat> for IGLVertexFormat {
    fn from(f: VertexFormat) -> Self {
        match f {
            VertexFormat::Float1 => IGLVertexFormat::Float1,
            VertexFormat::Float2 => IGLVertexFormat::Float2,
            VertexFormat::Float3 => IGLVertexFormat::Float3,
            VertexFormat::Float4 => IGLVertexFormat::Float4,
        }
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum IndexFormat {
    UInt16,
    UInt32,
}

impl From<IndexFormat> for IGLIndexFormat {
    fn from(f: IndexFormat) -> Self {
        match f {
            IndexFormat::UInt16 => IGLIndexFormat::UInt16,
            IndexFormat::UInt32 => IGLIndexFormat::UInt32,
        }
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum CullMode {
    None,
    Front,
    Back,
}

impl From<CullMode> for IGLCullMode {
    fn from(m: CullMode) -> Self {
        match m {
            CullMode::None => IGLCullMode::None,
            CullMode::Front => IGLCullMode::Front,
            CullMode::Back => IGLCullMode::Back,
        }
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum WindingMode {
    Clockwise,
    CounterClockwise,
}

impl From<WindingMode> for IGLWindingMode {
    fn from(m: WindingMode) -> Self {
        match m {
            WindingMode::Clockwise => IGLWindingMode::Clockwise,
            WindingMode::CounterClockwise => IGLWindingMode::CounterClockwise,
        }
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum LoadAction {
    DontCare,
    Load,
    Clear,
}

impl From<LoadAction> for IGLLoadAction {
    fn from(a: LoadAction) -> Self {
        match a {
            LoadAction::DontCare => IGLLoadAction::DontCare,
            LoadAction::Load => IGLLoadAction::Load,
            LoadAction::Clear => IGLLoadAction::Clear,
        }
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum StoreAction {
    DontCare,
    Store,
}

impl From<StoreAction> for IGLStoreAction {
    fn from(a: StoreAction) -> Self {
        match a {
            StoreAction::DontCare => IGLStoreAction::DontCare,
            StoreAction::Store => IGLStoreAction::Store,
        }
    }
}

#[derive(Debug, Copy, Clone)]
pub struct Color {
    pub r: f32,
    pub g: f32,
    pub b: f32,
    pub a: f32,
}

impl Color {
    pub const fn new(r: f32, g: f32, b: f32, a: f32) -> Self {
        Self { r, g, b, a }
    }

    pub const BLACK: Color = Color::new(0.0, 0.0, 0.0, 1.0);
    pub const WHITE: Color = Color::new(1.0, 1.0, 1.0, 1.0);
    pub const RED: Color = Color::new(1.0, 0.0, 0.0, 1.0);
    pub const GREEN: Color = Color::new(0.0, 1.0, 0.0, 1.0);
    pub const BLUE: Color = Color::new(0.0, 0.0, 1.0, 1.0);
}

impl From<Color> for IGLColor {
    fn from(c: Color) -> Self {
        IGLColor {
            r: c.r,
            g: c.g,
            b: c.b,
            a: c.a,
        }
    }
}

#[derive(Debug, Clone)]
pub struct VertexAttribute {
    pub buffer_index: u32,
    pub format: VertexFormat,
    pub offset: u32,
    pub name: String,
    pub location: i32,
}

#[derive(Debug, Copy, Clone)]
pub struct VertexBinding {
    pub stride: u32,
}

// Texture format - simplified for now
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[repr(u32)]
pub enum TextureFormat {
    BGRA_UNorm8 = 10,
    Depth32Float = 41,
}

/// Command Queue manages command submission
pub struct CommandQueue {
    handle: *mut IGLCommandQueue,
}

impl CommandQueue {
    fn new(device: &Device) -> Result<Self> {
        let handle = unsafe { igl_device_create_command_queue(device.handle) };
        if handle.is_null() {
            return Err(Error::NullPointer);
        }
        Ok(CommandQueue { handle })
    }

    pub fn create_command_buffer(&self) -> Result<CommandBuffer> {
        CommandBuffer::new(self)
    }

    pub fn submit(&self, command_buffer: &CommandBuffer) {
        unsafe {
            igl_command_queue_submit(self.handle, command_buffer.handle);
        }
    }
}

impl Drop for CommandQueue {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                igl_command_queue_destroy(self.handle);
            }
        }
    }
}

unsafe impl Send for CommandQueue {}

/// Command Buffer records rendering commands
pub struct CommandBuffer {
    handle: *mut IGLCommandBuffer,
}

impl CommandBuffer {
    fn new(queue: &CommandQueue) -> Result<Self> {
        let handle = unsafe { igl_command_queue_create_command_buffer(queue.handle) };
        if handle.is_null() {
            return Err(Error::NullPointer);
        }
        Ok(CommandBuffer { handle })
    }

    pub fn create_render_encoder(
        &self,
        framebuffer: &Framebuffer,
        color_attachment: ColorAttachment,
        depth_attachment: DepthAttachment,
    ) -> Result<RenderCommandEncoder> {
        RenderCommandEncoder::new(self, framebuffer, color_attachment, depth_attachment)
    }

    pub fn present(&self, texture: &Texture) {
        unsafe {
            igl_command_buffer_present(self.handle, texture.handle);
        }
    }
}

impl Drop for CommandBuffer {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                igl_command_buffer_destroy(self.handle);
            }
        }
    }
}

#[derive(Debug, Copy, Clone)]
pub struct ColorAttachment {
    pub load_action: LoadAction,
    pub store_action: StoreAction,
    pub clear_color: Color,
}

impl Default for ColorAttachment {
    fn default() -> Self {
        Self {
            load_action: LoadAction::Clear,
            store_action: StoreAction::Store,
            clear_color: Color::BLACK,
        }
    }
}

#[derive(Debug, Copy, Clone)]
pub struct DepthAttachment {
    pub load_action: LoadAction,
    pub clear_depth: f32,
}

impl Default for DepthAttachment {
    fn default() -> Self {
        Self {
            load_action: LoadAction::Clear,
            clear_depth: 1.0,
        }
    }
}

/// Buffer holds vertex, index, or uniform data
pub struct Buffer {
    handle: *mut IGLBuffer,
}

impl Buffer {
    fn new(device: &Device, buffer_type: BufferType, data: &[u8]) -> Result<Self> {
        let handle = unsafe {
            igl_device_create_buffer(
                device.handle,
                buffer_type.into(),
                data.as_ptr() as *const c_void,
                data.len() as u32,
            )
        };
        if handle.is_null() {
            return Err(Error::NullPointer);
        }
        Ok(Buffer { handle })
    }

    pub(crate) fn as_ptr(&self) -> *mut IGLBuffer {
        self.handle
    }
}

impl Drop for Buffer {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                igl_buffer_destroy(self.handle);
            }
        }
    }
}

unsafe impl Send for Buffer {}

/// Shader stages contain vertex and fragment shaders
pub struct ShaderStages {
    handle: *mut IGLShaderStages,
}

impl ShaderStages {
    fn new_metal(
        device: &Device,
        source: &str,
        vertex_entry: &str,
        fragment_entry: &str,
    ) -> Result<Self> {
        let source_cstr = CString::new(source).map_err(|_| Error::NullPointer)?;
        let vertex_cstr = CString::new(vertex_entry).map_err(|_| Error::NullPointer)?;
        let fragment_cstr = CString::new(fragment_entry).map_err(|_| Error::NullPointer)?;

        let handle = unsafe {
            igl_device_create_shader_stages_metal(
                device.handle,
                source_cstr.as_ptr(),
                vertex_cstr.as_ptr(),
                fragment_cstr.as_ptr(),
            )
        };
        if handle.is_null() {
            return Err(Error::NullPointer);
        }
        Ok(ShaderStages { handle })
    }

    pub(crate) fn as_ptr(&self) -> *mut IGLShaderStages {
        self.handle
    }
}

impl Drop for ShaderStages {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                igl_shader_stages_destroy(self.handle);
            }
        }
    }
}

unsafe impl Send for ShaderStages {}

/// Vertex Input State describes vertex buffer layout
pub struct VertexInputState {
    handle: *mut IGLVertexInputState,
    // Keep strings alive for C pointers
    _attribute_names: Vec<CString>,
}

impl VertexInputState {
    fn new(
        device: &Device,
        attributes: &[VertexAttribute],
        bindings: &[VertexBinding],
    ) -> Result<Self> {
        let mut attribute_names: Vec<CString> = Vec::new();
        let mut c_attributes: Vec<IGLVertexAttribute> = Vec::new();

        for attr in attributes {
            let name = CString::new(attr.name.as_str()).map_err(|_| Error::NullPointer)?;
            c_attributes.push(IGLVertexAttribute {
                buffer_index: attr.buffer_index,
                format: attr.format.into(),
                offset: attr.offset,
                name: name.as_ptr(),
                location: attr.location,
            });
            attribute_names.push(name);
        }

        let c_bindings: Vec<IGLVertexBinding> = bindings
            .iter()
            .map(|b| IGLVertexBinding { stride: b.stride })
            .collect();

        let handle = unsafe {
            igl_device_create_vertex_input_state(
                device.handle,
                c_attributes.as_ptr(),
                c_attributes.len() as u32,
                c_bindings.as_ptr(),
                c_bindings.len() as u32,
            )
        };

        if handle.is_null() {
            return Err(Error::NullPointer);
        }

        Ok(VertexInputState {
            handle,
            _attribute_names: attribute_names,
        })
    }

    pub(crate) fn as_ptr(&self) -> *mut IGLVertexInputState {
        self.handle
    }
}

impl Drop for VertexInputState {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                igl_vertex_input_state_destroy(self.handle);
            }
        }
    }
}

unsafe impl Send for VertexInputState {}

/// Texture represents a GPU texture (we receive these from platform)
pub struct Texture {
    pub(crate) handle: *mut IGLTexture,
    owned: bool,
}

impl Texture {
    pub(crate) fn from_raw(handle: *mut IGLTexture) -> Self {
        Texture {
            handle,
            owned: false,
        }
    }

    pub fn format(&self) -> u32 {
        unsafe { igl_texture_get_format(self.handle) }
    }

    pub fn aspect_ratio(&self) -> f32 {
        unsafe { igl_texture_get_aspect_ratio(self.handle) }
    }
}

impl Drop for Texture {
    fn drop(&mut self) {
        // Don't destroy textures we don't own (from platform)
        // The platform manages their lifecycle
    }
}

/// Framebuffer contains render targets
pub struct Framebuffer {
    handle: *mut IGLFramebuffer,
}

impl Framebuffer {
    fn new(device: &Device, color_texture: &Texture, depth_texture: &Texture) -> Result<Self> {
        let handle = unsafe {
            igl_device_create_framebuffer(device.handle, color_texture.handle, depth_texture.handle)
        };
        if handle.is_null() {
            return Err(Error::NullPointer);
        }
        Ok(Framebuffer { handle })
    }

    pub fn update_drawable(&self, color_texture: &Texture) {
        unsafe {
            igl_framebuffer_update_drawable(self.handle, color_texture.handle);
        }
    }

    pub fn get_color_attachment(&self) -> Texture {
        let handle = unsafe { igl_framebuffer_get_color_attachment(self.handle) };
        Texture::from_raw(handle)
    }

    pub(crate) fn as_ptr(&self) -> *mut IGLFramebuffer {
        self.handle
    }
}

impl Drop for Framebuffer {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                igl_framebuffer_destroy(self.handle);
            }
        }
    }
}

unsafe impl Send for Framebuffer {}

/// Render Pipeline State contains all rendering state
pub struct RenderPipelineState {
    handle: *mut IGLRenderPipelineState,
}

impl RenderPipelineState {
    #[allow(clippy::too_many_arguments)]
    fn new(
        device: &Device,
        vertex_input: &VertexInputState,
        shaders: &ShaderStages,
        color_format: TextureFormat,
        depth_format: TextureFormat,
        cull_mode: CullMode,
        winding_mode: WindingMode,
    ) -> Result<Self> {
        let handle = unsafe {
            igl_device_create_render_pipeline(
                device.handle,
                vertex_input.as_ptr(),
                shaders.as_ptr(),
                color_format as u32,
                depth_format as u32,
                cull_mode.into(),
                winding_mode.into(),
            )
        };
        if handle.is_null() {
            return Err(Error::NullPointer);
        }
        Ok(RenderPipelineState { handle })
    }

    pub(crate) fn as_ptr(&self) -> *mut IGLRenderPipelineState {
        self.handle
    }
}

impl Drop for RenderPipelineState {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                igl_render_pipeline_state_destroy(self.handle);
            }
        }
    }
}

unsafe impl Send for RenderPipelineState {}

/// Render Command Encoder records rendering commands
pub struct RenderCommandEncoder {
    handle: *mut IGLRenderCommandEncoder,
}

impl RenderCommandEncoder {
    fn new(
        command_buffer: &CommandBuffer,
        framebuffer: &Framebuffer,
        color_attachment: ColorAttachment,
        depth_attachment: DepthAttachment,
    ) -> Result<Self> {
        let c_color = IGLColorAttachment {
            load_action: color_attachment.load_action.into(),
            store_action: color_attachment.store_action.into(),
            clear_color: color_attachment.clear_color.into(),
        };

        let c_depth = IGLDepthAttachment {
            load_action: depth_attachment.load_action.into(),
            clear_depth: depth_attachment.clear_depth,
        };

        let handle = unsafe {
            igl_command_buffer_create_render_encoder(
                command_buffer.handle,
                framebuffer.as_ptr(),
                &c_color,
                &c_depth,
            )
        };

        if handle.is_null() {
            return Err(Error::NullPointer);
        }

        Ok(RenderCommandEncoder { handle })
    }

    pub fn bind_vertex_buffer(&self, index: u32, buffer: &Buffer) {
        unsafe {
            igl_render_encoder_bind_vertex_buffer(self.handle, index, buffer.as_ptr());
        }
    }

    pub fn bind_index_buffer(&self, buffer: &Buffer, format: IndexFormat) {
        unsafe {
            igl_render_encoder_bind_index_buffer(self.handle, buffer.as_ptr(), format.into());
        }
    }

    pub fn bind_pipeline(&self, pipeline: &RenderPipelineState) {
        unsafe {
            igl_render_encoder_bind_pipeline(self.handle, pipeline.as_ptr());
        }
    }

    pub fn bind_uniform_buffer(&self, index: u32, buffer: &Buffer) {
        unsafe {
            igl_render_encoder_bind_uniform_buffer(self.handle, index, buffer.as_ptr());
        }
    }

    pub fn draw_indexed(&self, index_count: u32) {
        unsafe {
            igl_render_encoder_draw_indexed(self.handle, index_count);
        }
    }

    pub fn end_encoding(self) {
        unsafe {
            igl_render_encoder_end_encoding(self.handle);
        }
        // self is consumed, Drop won't be called
        std::mem::forget(self);
    }
}

impl Drop for RenderCommandEncoder {
    fn drop(&mut self) {
        // If not explicitly ended, we should end it
        if !self.handle.is_null() {
            unsafe {
                igl_render_encoder_end_encoding(self.handle);
            }
        }
    }
}
