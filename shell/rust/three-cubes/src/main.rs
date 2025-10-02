//! Three Rotating Cubes Demo using IGL and Rust
//!
//! This application demonstrates using IGL (Intermediate Graphics Library)
//! from Rust to render three independently rotating cubes.
//! The render session is implemented entirely in Rust!

mod render_session;

use igl_rs::Platform;
use raw_window_handle::HasWindowHandle;
use render_session::ThreeCubesRenderSession;
use winit::{
    event::{Event, WindowEvent},
    event_loop::EventLoop,
    window::WindowBuilder,
};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("Three Rotating Cubes - IGL + Rust Demo");
    println!("======================================");
    println!("🦀 Render session implemented in Rust! 🦀\n");

    // Create window with winit
    let event_loop = EventLoop::new()?;
    let window = WindowBuilder::new()
        .with_title("Three Rotating Cubes - IGL + Rust (Native Rust Renderer)")
        .with_inner_size(winit::dpi::LogicalSize::new(1024, 768))
        .build(&event_loop)?;

    let window_size = window.inner_size();
    println!("Window size: {}x{}", window_size.width, window_size.height);

    // Get native window handle for macOS
    let window_handle = match window.window_handle()?.as_raw() {
        raw_window_handle::RawWindowHandle::AppKit(handle) => {
            println!("Got AppKit window handle");
            handle.ns_view.as_ptr() as *mut std::ffi::c_void
        }
        _ => {
            return Err("Unsupported platform - only macOS is supported".into());
        }
    };

    // Create IGL platform
    println!("Creating IGL Metal platform...");
    let platform = Platform::create_metal(
        window_handle,
        window_size.width as i32,
        window_size.height as i32,
    )?;
    println!("Platform created successfully");

    // Get device
    println!("Getting graphics device...");
    let device = platform.device()?;
    println!("Device obtained: backend = {:?}", device.backend_type());

    // Create Rust render session
    println!("Creating Rust render session...");
    let mut session = ThreeCubesRenderSession::new(&device)?;
    println!("Rust render session created successfully!");

    println!("\nStarting render loop...");
    println!("Close the window to exit\n");

    // Main event loop
    event_loop.run(move |event, target| {
        match event {
            Event::WindowEvent { event, .. } => match event {
                WindowEvent::CloseRequested => {
                    println!("\nWindow close requested, exiting...");
                    target.exit();
                }
                WindowEvent::Resized(new_size) => {
                    println!("Window resized to: {}x{}", new_size.width, new_size.height);
                }
                WindowEvent::RedrawRequested => {
                    // Get textures for this frame
                    match platform.get_frame_textures() {
                        Ok((color_texture, depth_texture)) => {
                            // Render using Rust render session
                            if let Err(e) = session.render(&device, &color_texture, &depth_texture) {
                                eprintln!("Render error: {:?}", e);
                                target.exit();
                            }

                            // Present frame
                            platform.present_frame();
                        }
                        Err(e) => {
                            eprintln!("Failed to get frame textures: {:?}", e);
                            target.exit();
                        }
                    }
                }
                _ => {}
            },
            Event::AboutToWait => {
                // Request redraw for continuous rendering
                window.request_redraw();
            }
            _ => {}
        }
    })?;

    println!("Application exited successfully");
    Ok(())
}
