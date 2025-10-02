//! Low-level unsafe FFI bindings to IGL C wrapper
//!
//! This crate provides raw FFI bindings to the IGL graphics library.
//! Use the `igl-rs` crate for safe, idiomatic Rust APIs.

#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]

use std::os::raw::{c_int, c_void};

pub mod graphics;

// Opaque types
#[repr(C)]
pub struct IGLPlatform {
    _private: [u8; 0],
}

#[repr(C)]
pub struct IGLRenderSession {
    _private: [u8; 0],
}

pub type IGLNativeWindowHandle = *mut c_void;

#[repr(C)]
pub struct IGLSurfaceTextures {
    pub color_texture: *mut c_void,
    pub depth_texture: *mut c_void,
}

extern "C" {
    // Platform creation/destruction
    pub fn igl_platform_create_metal(
        window_handle: IGLNativeWindowHandle,
        width: c_int,
        height: c_int,
    ) -> *mut IGLPlatform;

    pub fn igl_platform_destroy(platform: *mut IGLPlatform);

    // RenderSession creation/destruction
    pub fn igl_render_session_create(platform: *mut IGLPlatform) -> *mut IGLRenderSession;

    pub fn igl_render_session_destroy(session: *mut IGLRenderSession);

    // RenderSession lifecycle
    pub fn igl_render_session_initialize(session: *mut IGLRenderSession) -> bool;

    pub fn igl_render_session_update(session: *mut IGLRenderSession) -> bool;

    pub fn igl_render_session_teardown(session: *mut IGLRenderSession);

    // Platform helpers
    pub fn igl_platform_get_surface_textures(
        platform: *mut IGLPlatform,
        out_textures: *mut IGLSurfaceTextures,
    ) -> bool;

    pub fn igl_platform_present(platform: *mut IGLPlatform);

    // Window management
    pub fn igl_platform_resize(platform: *mut IGLPlatform, width: c_int, height: c_int);
}

// Platform helper for getting frame textures
#[repr(C)]
pub struct IGLFrameTextures {
    pub color: *mut graphics::IGLTexture,
    pub depth: *mut graphics::IGLTexture,
}

extern "C" {
    pub fn igl_platform_get_frame_textures(
        platform: *mut IGLPlatform,
        out_textures: *mut IGLFrameTextures,
    ) -> bool;

    pub fn igl_platform_present_frame(platform: *mut IGLPlatform);
}
