//! Rust implementation of ThreeCubesRenderSession

use glam::{Mat4, Vec3};
use igl_rs::*;
use std::mem;
use std::time::Instant;

// Vertex data structure
#[repr(C)]
#[derive(Debug, Copy, Clone)]
struct VertexPosColor {
    position: [f32; 3],
    color: [f32; 3],
}

// Uniform structure for MVP matrix
#[repr(C)]
#[derive(Debug, Copy, Clone)]
struct VertexUniforms {
    mvp_matrix: Mat4,
}

// Cube transform data
#[derive(Debug, Clone)]
struct CubeTransform {
    position: Vec3,
    rotation_axis: Vec3,
    rotation_speed: f32,
    current_angle: f32,
    color: Vec3,
}

// Metal shader source
const METAL_SHADER_SOURCE: &str = r#"
    #include <metal_stdlib>
    #include <simd/simd.h>
    using namespace metal;

    struct VertexUniformBlock {
        float4x4 mvpMatrix;
    };

    struct VertexIn {
        float3 position [[attribute(0)]];
        float3 color [[attribute(1)]];
    };

    struct VertexOut {
        float4 position [[position]];
        float3 color;
    };

    vertex VertexOut vertexShader(VertexIn in [[stage_in]],
           constant VertexUniformBlock &vUniform[[buffer(1)]]) {
        VertexOut out;
        out.position = vUniform.mvpMatrix * float4(in.position, 1.0);
        out.color = in.color;
        return out;
     }

     fragment float4 fragmentShader(VertexOut in[[stage_in]]) {
       return float4(in.color, 1.0);
     }
"#;

pub struct ThreeCubesRenderSession {
    // Graphics resources
    command_queue: CommandQueue,
    vertex_buffer: Buffer,
    index_buffer: Buffer,
    vertex_input_state: VertexInputState,
    shader_stages: ShaderStages,
    pipeline_state: Option<RenderPipelineState>,
    framebuffer: Option<Framebuffer>,

    // Cube data
    cubes: [CubeTransform; 3],

    // Timing
    start_time: Instant,
    last_frame_time: Instant,
}

impl ThreeCubesRenderSession {
    pub fn new(device: &Device) -> Result<Self> {
        // Create vertex data for a cube
        let half = 1.0f32;
        let vertex_data = [
            // Front face (red tint)
            VertexPosColor { position: [-half, half, -half], color: [1.0, 0.3, 0.3] },
            VertexPosColor { position: [half, half, -half], color: [1.0, 0.3, 0.3] },
            VertexPosColor { position: [-half, -half, -half], color: [0.8, 0.2, 0.2] },
            VertexPosColor { position: [half, -half, -half], color: [0.8, 0.2, 0.2] },
            // Back face (blue tint)
            VertexPosColor { position: [half, half, half], color: [0.3, 0.3, 1.0] },
            VertexPosColor { position: [-half, half, half], color: [0.3, 0.3, 1.0] },
            VertexPosColor { position: [half, -half, half], color: [0.2, 0.2, 0.8] },
            VertexPosColor { position: [-half, -half, half], color: [0.2, 0.2, 0.8] },
        ];

        // Index data for cube (36 indices for 12 triangles)
        let index_data: [u16; 36] = [
            0, 1, 2, 1, 3, 2, // front
            1, 4, 3, 4, 6, 3, // right
            4, 5, 6, 5, 7, 6, // back
            5, 0, 7, 0, 2, 7, // left
            5, 4, 0, 4, 1, 0, // top
            2, 3, 7, 3, 6, 7, // bottom
        ];

        // Create buffers
        let vertex_bytes = unsafe {
            std::slice::from_raw_parts(
                vertex_data.as_ptr() as *const u8,
                mem::size_of_val(&vertex_data),
            )
        };
        let vertex_buffer = device.create_buffer(BufferType::Vertex, vertex_bytes)?;

        let index_bytes = unsafe {
            std::slice::from_raw_parts(
                index_data.as_ptr() as *const u8,
                mem::size_of_val(&index_data),
            )
        };
        let index_buffer = device.create_buffer(BufferType::Index, index_bytes)?;

        // Create vertex input state
        let attributes = vec![
            VertexAttribute {
                buffer_index: 0,
                format: VertexFormat::Float3,
                offset: 0,
                name: "position".to_string(),
                location: 0,
            },
            VertexAttribute {
                buffer_index: 0,
                format: VertexFormat::Float3,
                offset: mem::size_of::<[f32; 3]>() as u32,
                name: "color_in".to_string(),
                location: 1,
            },
        ];

        let bindings = vec![VertexBinding {
            stride: mem::size_of::<VertexPosColor>() as u32,
        }];

        let vertex_input_state = device.create_vertex_input_state(&attributes, &bindings)?;

        // Create shaders
        let shader_stages = device.create_shader_stages_metal(
            METAL_SHADER_SOURCE,
            "vertexShader",
            "fragmentShader",
        )?;

        // Create command queue
        let command_queue = device.create_command_queue()?;

        // Initialize cube transforms
        let cubes = [
            // Cube 1: Left, rotating on Y axis, red
            CubeTransform {
                position: Vec3::new(-3.0, 0.0, 0.0),
                rotation_axis: Vec3::new(0.0, 1.0, 0.0),
                rotation_speed: 1.0,
                current_angle: 0.0,
                color: Vec3::new(1.0, 0.3, 0.3),
            },
            // Cube 2: Center, rotating on diagonal axis, green
            CubeTransform {
                position: Vec3::new(0.0, 0.0, 0.0),
                rotation_axis: Vec3::new(1.0, 1.0, 0.0).normalize(),
                rotation_speed: 1.5,
                current_angle: 0.0,
                color: Vec3::new(0.3, 1.0, 0.3),
            },
            // Cube 3: Right, rotating on XZ diagonal, blue
            CubeTransform {
                position: Vec3::new(3.0, 0.0, 0.0),
                rotation_axis: Vec3::new(1.0, 0.0, 1.0).normalize(),
                rotation_speed: 0.75,
                current_angle: 0.0,
                color: Vec3::new(0.3, 0.3, 1.0),
            },
        ];

        let now = Instant::now();

        Ok(ThreeCubesRenderSession {
            command_queue,
            vertex_buffer,
            index_buffer,
            vertex_input_state,
            shader_stages,
            pipeline_state: None,
            framebuffer: None,
            cubes,
            start_time: now,
            last_frame_time: now,
        })
    }

    pub fn render(
        &mut self,
        device: &Device,
        color_texture: &Texture,
        depth_texture: &Texture,
    ) -> Result<()> {
        // Update cube rotations
        let now = Instant::now();
        let delta_time = now.duration_since(self.last_frame_time).as_secs_f32();
        self.last_frame_time = now;

        for cube in &mut self.cubes {
            cube.current_angle += cube.rotation_speed * delta_time;
        }

        // Create or update framebuffer
        if self.framebuffer.is_none() {
            self.framebuffer = Some(device.create_framebuffer(color_texture, depth_texture)?);
        } else if let Some(ref fb) = self.framebuffer {
            fb.update_drawable(color_texture);
        }

        // Create pipeline if needed
        if self.pipeline_state.is_none() {
            // Get actual formats from framebuffer textures
            let color_format_raw = color_texture.format();
            let depth_format_raw = depth_texture.format();

            self.pipeline_state = Some(device.create_render_pipeline(
                &self.vertex_input_state,
                &self.shader_stages,
                unsafe { std::mem::transmute(color_format_raw) },
                unsafe { std::mem::transmute(depth_format_raw) },
                CullMode::Back,
                WindingMode::Clockwise,
            )?);
        }

        // Create command buffer
        let command_buffer = self.command_queue.create_command_buffer()?;

        // Create render encoder
        let color_attachment = ColorAttachment {
            load_action: LoadAction::Clear,
            store_action: StoreAction::Store,
            clear_color: Color::new(0.1, 0.1, 0.15, 1.0), // Dark blue background
        };

        let depth_attachment = DepthAttachment {
            load_action: LoadAction::Clear,
            clear_depth: 1.0,
        };

        let encoder = command_buffer.create_render_encoder(
            self.framebuffer.as_ref().unwrap(),
            color_attachment,
            depth_attachment,
        )?;

        // Bind vertex and index buffers
        encoder.bind_vertex_buffer(0, &self.vertex_buffer);
        encoder.bind_index_buffer(&self.index_buffer, IndexFormat::UInt16);
        encoder.bind_pipeline(self.pipeline_state.as_ref().unwrap());

        // Render each cube
        let aspect_ratio = color_texture.aspect_ratio();

        static mut FRAME_COUNT: u32 = 0;
        unsafe {
            FRAME_COUNT += 1;
        }

        for (i, cube) in self.cubes.iter().enumerate() {
            // Create MVP matrix
            let mvp = self.create_mvp_matrix(cube, aspect_ratio);

            unsafe {
                if FRAME_COUNT == 1 && i == 0 {
                    println!("[Rust MVP Matrix for cube 0, frame 1 (Column-Major)]:");
                    println!("  Col0: [{:8.4} {:8.4} {:8.4} {:8.4}]", mvp.x_axis.x, mvp.x_axis.y, mvp.x_axis.z, mvp.x_axis.w);
                    println!("  Col1: [{:8.4} {:8.4} {:8.4} {:8.4}]", mvp.y_axis.x, mvp.y_axis.y, mvp.y_axis.z, mvp.y_axis.w);
                    println!("  Col2: [{:8.4} {:8.4} {:8.4} {:8.4}]", mvp.z_axis.x, mvp.z_axis.y, mvp.z_axis.z, mvp.z_axis.w);
                    println!("  Col3: [{:8.4} {:8.4} {:8.4} {:8.4}]", mvp.w_axis.x, mvp.w_axis.y, mvp.w_axis.z, mvp.w_axis.w);
                    println!("  aspectRatio={:.4}, cube.position=({}, {}, {})", aspect_ratio, cube.position.x, cube.position.y, cube.position.z);
                    println!("[Rust MVP as rows (transposed view)]:");
                    println!("  Row0: [{:8.4} {:8.4} {:8.4} {:8.4}]", mvp.x_axis.x, mvp.y_axis.x, mvp.z_axis.x, mvp.w_axis.x);
                    println!("  Row1: [{:8.4} {:8.4} {:8.4} {:8.4}]", mvp.x_axis.y, mvp.y_axis.y, mvp.z_axis.y, mvp.w_axis.y);
                    println!("  Row2: [{:8.4} {:8.4} {:8.4} {:8.4}]", mvp.x_axis.z, mvp.y_axis.z, mvp.z_axis.z, mvp.w_axis.z);
                    println!("  Row3: [{:8.4} {:8.4} {:8.4} {:8.4}]", mvp.x_axis.w, mvp.y_axis.w, mvp.z_axis.w, mvp.w_axis.w);
                }
            }

            let uniforms = VertexUniforms { mvp_matrix: mvp };

            // Create uniform buffer for this cube
            let uniform_bytes = unsafe {
                std::slice::from_raw_parts(
                    &uniforms as *const _ as *const u8,
                    mem::size_of::<VertexUniforms>(),
                )
            };

            unsafe {
                if FRAME_COUNT == 1 && i == 0 {
                    println!("[Rust Uniform bytes as floats]:");
                    let floats = std::slice::from_raw_parts(uniform_bytes.as_ptr() as *const f32, 16);
                    for j in (0..16).step_by(4) {
                        println!("  [{:8.4} {:8.4} {:8.4} {:8.4}]", floats[j], floats[j+1], floats[j+2], floats[j+3]);
                    }
                }
            }

            let uniform_buffer = device.create_buffer(BufferType::Uniform, uniform_bytes)?;

            // Bind uniforms and draw
            encoder.bind_uniform_buffer(1, &uniform_buffer);
            encoder.draw_indexed(36);
        }

        // End encoding
        encoder.end_encoding();

        // Present the drawable
        command_buffer.present(color_texture);

        // Submit
        self.command_queue.submit(&command_buffer);

        Ok(())
    }

    fn create_mvp_matrix(&self, cube: &CubeTransform, aspect_ratio: f32) -> Mat4 {
        // Perspective projection
        let fov = 45.0f32.to_radians();
        let projection = Mat4::perspective_lh(fov, aspect_ratio, 0.1, 100.0);

        // Model transform: translate cube, then rotate
        let translation = Mat4::from_translation(cube.position + Vec3::new(0.0, 0.0, 8.0));
        let rotation = Mat4::from_axis_angle(cube.rotation_axis, cube.current_angle);
        let model = translation * rotation;

        // Combined MVP
        projection * model
    }
}
