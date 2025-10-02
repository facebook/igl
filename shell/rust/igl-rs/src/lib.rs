//! Safe, idiomatic Rust bindings for IGL (Intermediate Graphics Library)
//!
//! This crate provides safe wrappers around the low-level FFI bindings in `igl-sys`.
//!
//! # Example
//! ```no_run
//! use igl_rs::{Platform, RenderSession};
//!
//! let platform = Platform::create_metal(window_handle, 800, 600).expect("Failed to create platform");
//! let mut session = RenderSession::new(&platform).expect("Failed to create session");
//! session.initialize().expect("Failed to initialize");
//!
//! loop {
//!     session.update().expect("Failed to update");
//! }
//! ```

use igl_sys::*;
use std::os::raw::c_void;

pub mod graphics;
pub use graphics::*;

/// Error type for IGL operations
#[derive(Debug)]
pub enum Error {
    PlatformCreationFailed,
    SessionCreationFailed,
    InitializationFailed,
    UpdateFailed,
    NullPointer,
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Error::PlatformCreationFailed => write!(f, "Failed to create IGL platform"),
            Error::SessionCreationFailed => write!(f, "Failed to create render session"),
            Error::InitializationFailed => write!(f, "Failed to initialize render session"),
            Error::UpdateFailed => write!(f, "Failed to update render session"),
            Error::NullPointer => write!(f, "Null pointer encountered"),
        }
    }
}

impl std::error::Error for Error {}

pub type Result<T> = std::result::Result<T, Error>;

/// Platform represents the graphics device and context
pub struct Platform {
    handle: *mut IGLPlatform,
}

impl Platform {
    /// Create a new Metal platform
    ///
    /// # Safety
    /// The window_handle must be a valid NSView pointer
    pub fn create_metal(window_handle: *mut c_void, width: i32, height: i32) -> Result<Self> {
        let handle = unsafe { igl_platform_create_metal(window_handle, width, height) };

        if handle.is_null() {
            return Err(Error::PlatformCreationFailed);
        }

        Ok(Platform { handle })
    }

    /// Get the raw platform handle
    pub fn as_ptr(&self) -> *mut IGLPlatform {
        self.handle
    }

    /// Resize the platform surfaces
    pub fn resize(&mut self, width: i32, height: i32) {
        unsafe {
            igl_platform_resize(self.handle, width, height);
        }
    }

    /// Get the graphics device
    pub fn device(&self) -> Result<Device> {
        Device::from_platform(self.handle as *mut c_void)
    }

    /// Get textures for the current frame (acquires drawable)
    pub fn get_frame_textures(&self) -> Result<(Texture, Texture)> {
        let mut frame_textures = IGLFrameTextures {
            color: std::ptr::null_mut(),
            depth: std::ptr::null_mut(),
        };

        let success = unsafe {
            igl_platform_get_frame_textures(self.handle, &mut frame_textures)
        };

        if !success || frame_textures.color.is_null() || frame_textures.depth.is_null() {
            return Err(Error::NullPointer);
        }

        Ok((
            Texture::from_raw(frame_textures.color),
            Texture::from_raw(frame_textures.depth),
        ))
    }

    /// Present the current frame
    pub fn present_frame(&self) {
        unsafe {
            igl_platform_present_frame(self.handle);
        }
    }
}

impl Drop for Platform {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                igl_platform_destroy(self.handle);
            }
        }
    }
}

// Platform is Send and Sync because IGL is thread-safe for these operations
unsafe impl Send for Platform {}
unsafe impl Sync for Platform {}

/// RenderSession manages the rendering loop
pub struct RenderSession {
    handle: *mut IGLRenderSession,
}

impl RenderSession {
    /// Create a new render session
    pub fn new(platform: &Platform) -> Result<Self> {
        let handle = unsafe { igl_render_session_create(platform.handle) };

        if handle.is_null() {
            return Err(Error::SessionCreationFailed);
        }

        Ok(RenderSession { handle })
    }

    /// Initialize the render session
    pub fn initialize(&mut self) -> Result<()> {
        let success = unsafe { igl_render_session_initialize(self.handle) };

        if success {
            Ok(())
        } else {
            Err(Error::InitializationFailed)
        }
    }

    /// Update the render session (renders one frame)
    pub fn update(&mut self) -> Result<()> {
        let success = unsafe { igl_render_session_update(self.handle) };

        if success {
            Ok(())
        } else {
            Err(Error::UpdateFailed)
        }
    }

    /// Teardown the render session
    pub fn teardown(&mut self) {
        unsafe {
            igl_render_session_teardown(self.handle);
        }
    }
}

impl Drop for RenderSession {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            self.teardown();
            unsafe {
                igl_render_session_destroy(self.handle);
            }
        }
    }
}

// RenderSession must be used from a single thread
// Send is safe because we can transfer ownership between threads
unsafe impl Send for RenderSession {}
