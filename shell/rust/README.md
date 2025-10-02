# IGL Rust Bindings

Rust bindings for IGL (Intermediate Graphics Library) with a native Rust render session demo.

## Overview

This directory contains complete Rust bindings for IGL, organized into three main crates:

- **igl-sys** - Low-level FFI bindings (unsafe)
- **igl-rs** - Safe, idiomatic Rust wrapper API
- **three-cubes** - Demo application with native Rust render session

## Prerequisites

### Required Tools

1. **Rust toolchain** (1.70 or later)
   ```bash
   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
   ```

2. **CMake** (3.16 or later)
   ```bash
   brew install cmake
   ```

3. **Xcode Command Line Tools** (macOS only)
   ```bash
   xcode-select --install
   ```

### System Requirements

- **macOS**: Metal backend support (macOS 10.15+)
- **Architecture**: x86_64 or arm64 (Apple Silicon)

## Building IGL C++ Wrapper

Before building the Rust bindings, you need to build the IGL C++ wrapper library:

```bash
# From the repository root
cd /path/to/igl

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. -G Xcode

# Build the C++ wrapper
cmake --build . --config Debug --target igl_c_wrapper

# Or for Release build:
cmake --build . --config Release --target igl_c_wrapper
```

This will create `libigl_c_wrapper.a` in the build directory.

## Building the Rust Bindings

Once the C++ wrapper is built, compile the Rust bindings:

```bash
# Navigate to the Rust directory
cd shell/rust

# Build all crates in debug mode
cargo build

# Or build in release mode (optimized)
cargo build --release
```

## Running the Three Cubes Demo

The `three-cubes` demo renders three independently rotating cubes using a fully native Rust render session.

### Run in Debug Mode

```bash
cd shell/rust
cargo run --bin three-cubes
```

### Run in Release Mode (Better Performance)

```bash
cd shell/rust
cargo run --release --bin three-cubes
```

### Direct Execution

After building, you can run the binary directly:

```bash
# Debug build
./target/debug/three-cubes

# Release build
./target/release/three-cubes
```

## Project Structure

```
shell/rust/
├── igl-c-wrapper/          # C/Objective-C++ FFI layer
│   ├── include/            # C header files
│   │   ├── igl_c_wrapper.h
│   │   ├── igl_graphics.h
│   │   └── igl_platform_helper.h
│   └── src/                # C++ implementation
│       ├── igl_c_wrapper.mm
│       ├── igl_graphics.mm
│       └── igl_platform_helper.mm
├── igl-sys/                # Low-level Rust FFI bindings
│   ├── build.rs            # Build script for linking
│   └── src/
│       ├── lib.rs
│       └── graphics.rs
├── igl-rs/                 # Safe Rust wrapper API
│   └── src/
│       ├── lib.rs          # Platform API
│       └── graphics.rs     # Graphics primitives
└── three-cubes/            # Demo application
    └── src/
        ├── main.rs         # Entry point & window setup
        └── render_session.rs  # Native Rust render session
```

## Key Features

### Safe Rust API

The `igl-rs` crate provides safe wrappers with automatic resource management:

```rust
use igl_rs::*;

// Resources are automatically cleaned up via Drop trait
let device = platform.device()?;
let buffer = device.create_buffer(BufferType::Vertex, data)?;
let pipeline = device.create_render_pipeline(
    &vertex_input,
    &shaders,
    color_format,
    depth_format,
    CullMode::Back,
    WindingMode::Clockwise,
)?;
```

### Native Rust Render Session

The three-cubes demo implements a complete render session in Rust:

- Metal shader compilation from Rust strings
- Vertex/index buffer creation and management
- Render pipeline setup
- MVP matrix calculations using `glam`
- Per-frame rendering with command buffer encoding
- Independent cube rotations with different speeds

## Troubleshooting

### Build Errors

**CMake can't find IGL libraries:**
```bash
# Make sure you built the C++ wrapper first
cd build
cmake --build . --config Debug --target igl_c_wrapper
```

**Linking errors:**
```bash
# Clean and rebuild
cd shell/rust
cargo clean
cargo build
```

### Runtime Errors

**Window doesn't appear:**
- Check that you have a display connected
- Ensure Metal is supported on your system

**Black screen/No rendering:**
- Check console output for Metal errors
- Verify shaders compiled successfully

### Performance Issues

For best performance, always use release builds:
```bash
cargo build --release
cargo run --release --bin three-cubes
```

## Development

### Adding New Bindings

1. Add C FFI functions to `igl-c-wrapper/include/igl_graphics.h`
2. Implement in `igl-c-wrapper/src/igl_graphics.mm`
3. Add Rust FFI declarations to `igl-sys/src/graphics.rs`
4. Create safe wrappers in `igl-rs/src/graphics.rs`

### Testing

```bash
# Run tests for all crates
cargo test

# Run tests for a specific crate
cargo test -p igl-rs
```

## Technical Details

### FFI Architecture

The bindings use a multi-layer architecture:

1. **C FFI Layer** (`igl-c-wrapper`): Exposes IGL C++ APIs as C functions
2. **Unsafe FFI** (`igl-sys`): Raw Rust bindings to C functions
3. **Safe API** (`igl-rs`): Idiomatic Rust wrappers with RAII

### Resource Management

All graphics resources use Rust's Drop trait for automatic cleanup:

```rust
impl Drop for Buffer {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe { igl_buffer_destroy(self.handle); }
        }
    }
}
```

### Thread Safety

The current bindings are not thread-safe. Graphics commands should be issued from the main thread.

## Examples

### Creating a Simple Triangle

```rust
use igl_rs::*;

// Create vertex buffer
let vertices = [
    VertexPosColor { position: [0.0, 1.0, 0.0], color: [1.0, 0.0, 0.0] },
    VertexPosColor { position: [-1.0, -1.0, 0.0], color: [0.0, 1.0, 0.0] },
    VertexPosColor { position: [1.0, -1.0, 0.0], color: [0.0, 0.0, 1.0] },
];

let vertex_buffer = device.create_buffer(
    BufferType::Vertex,
    as_bytes(&vertices)
)?;

// Render
encoder.bind_vertex_buffer(0, &vertex_buffer);
encoder.draw(3);
```

## License

This code follows the same MIT license as the main IGL project.

## Contributing

Contributions are welcome! Please ensure:

1. Code follows Rust conventions (run `cargo fmt`)
2. All tests pass (`cargo test`)
3. New unsafe code is properly documented
4. Public APIs have documentation comments

## Credits

🦀 Built with Rust

🤖 Generated with [Claude Code](https://claude.com/claude-code)
