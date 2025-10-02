using System;
using System.Runtime.InteropServices;
using System.Reflection;

namespace IGL.Bindings;

/// <summary>
/// Low-level P/Invoke bindings to the IGL C wrapper library
/// </summary>
public static unsafe class Native
{
    private const string LibraryName = "igl_c_wrapper";

    static Native()
    {
        // Set up custom DLL import resolver for macOS
        NativeLibrary.SetDllImportResolver(typeof(Native).Assembly, DllImportResolver);
    }

    private static IntPtr DllImportResolver(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
    {
        if (libraryName == LibraryName)
        {
            // Try to load from the absolute path in the build directory
            var iglLibPath = "/Users/alexeymedvedev/Desktop/sources/igl/build/shell/rust/igl-c-wrapper/Debug/libigl_c_wrapper.dylib";
            if (NativeLibrary.TryLoad(iglLibPath, out var handle))
            {
                return handle;
            }
        }
        return IntPtr.Zero;
    }

    // Opaque handles
    public struct IGLPlatform { }
    public struct IGLDevice { }
    public struct IGLCommandQueue { }
    public struct IGLCommandBuffer { }
    public struct IGLBuffer { }
    public struct IGLTexture { }
    public struct IGLShaderStages { }
    public struct IGLVertexInputState { }
    public struct IGLRenderPipelineState { }
    public struct IGLFramebuffer { }
    public struct IGLRenderCommandEncoder { }

    // Enums
    public enum IGLBackendType
    {
        Invalid = 0,
        OpenGL = 1,
        Metal = 2,
        Vulkan = 3,
    }

    public enum IGLBufferType
    {
        Vertex = 1,
        Index = 2,
        Uniform = 4,
    }

    public enum IGLVertexFormat
    {
        Float1 = 0,
        Float2 = 1,
        Float3 = 2,
        Float4 = 3,
    }

    public enum IGLIndexFormat
    {
        UInt16 = 0,
        UInt32 = 1,
    }

    public enum IGLLoadAction
    {
        DontCare = 0,
        Load = 1,
        Clear = 2,
    }

    public enum IGLStoreAction
    {
        DontCare = 0,
        Store = 1,
    }

    public enum IGLCullMode
    {
        None = 0,
        Front = 1,
        Back = 2,
    }

    public enum IGLWindingMode
    {
        Clockwise = 0,
        CounterClockwise = 1,
    }

    // Structs
    [StructLayout(LayoutKind.Sequential)]
    public struct IGLColor
    {
        public float R, G, B, A;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct IGLVertexAttribute
    {
        public uint BufferIndex;
        public IGLVertexFormat Format;
        public uint Offset;
        public IntPtr Name; // const char*
        public uint Location;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct IGLVertexBinding
    {
        public uint Stride;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct IGLColorAttachment
    {
        public IGLLoadAction LoadAction;
        public IGLStoreAction StoreAction;
        public IGLColor ClearColor;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct IGLDepthAttachment
    {
        public IGLLoadAction LoadAction;
        public float ClearDepth;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct IGLFrameTextures
    {
        public IGLTexture* Color;
        public IGLTexture* Depth;
    }

    // Platform functions
    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IGLPlatform* igl_platform_create_metal(void* window_handle, int width, int height);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_platform_destroy(IGLPlatform* platform);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IGLDevice* igl_platform_get_device(void* platform);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool igl_platform_get_frame_textures(IGLPlatform* platform, IGLFrameTextures* out_textures);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_platform_present_frame(IGLPlatform* platform);

    // Device functions
    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IGLBackendType igl_device_get_backend_type(IGLDevice* device);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IGLCommandQueue* igl_device_create_command_queue(IGLDevice* device);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IGLBuffer* igl_device_create_buffer(IGLDevice* device, IGLBufferType type, void* data, uint size);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IGLShaderStages* igl_device_create_shader_stages_metal(
        IGLDevice* device, byte* source, byte* vertex_entry, byte* fragment_entry);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IGLVertexInputState* igl_device_create_vertex_input_state(
        IGLDevice* device, IGLVertexAttribute* attributes, uint num_attributes,
        IGLVertexBinding* bindings, uint num_bindings);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IGLFramebuffer* igl_device_create_framebuffer(
        IGLDevice* device, IGLTexture* color_texture, IGLTexture* depth_texture);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IGLRenderPipelineState* igl_device_create_render_pipeline(
        IGLDevice* device, IGLVertexInputState* vertex_input, IGLShaderStages* shaders,
        uint color_format, uint depth_format, IGLCullMode cull_mode, IGLWindingMode winding_mode);

    // Command Queue functions
    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_command_queue_destroy(IGLCommandQueue* queue);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IGLCommandBuffer* igl_command_queue_create_command_buffer(IGLCommandQueue* queue);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_command_queue_submit(IGLCommandQueue* queue, IGLCommandBuffer* buffer);

    // Command Buffer functions
    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_command_buffer_destroy(IGLCommandBuffer* buffer);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_command_buffer_present(IGLCommandBuffer* buffer, IGLTexture* texture);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IGLRenderCommandEncoder* igl_command_buffer_create_render_encoder(
        IGLCommandBuffer* buffer, IGLFramebuffer* framebuffer,
        IGLColorAttachment* color_attachment, IGLDepthAttachment* depth_attachment);

    // Buffer functions
    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_buffer_destroy(IGLBuffer* buffer);

    // Shader functions
    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_shader_stages_destroy(IGLShaderStages* stages);

    // Vertex Input State functions
    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_vertex_input_state_destroy(IGLVertexInputState* state);

    // Framebuffer functions
    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_framebuffer_destroy(IGLFramebuffer* framebuffer);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_framebuffer_update_drawable(IGLFramebuffer* framebuffer, IGLTexture* color_texture);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IGLTexture* igl_framebuffer_get_color_attachment(IGLFramebuffer* framebuffer);

    // Render Pipeline State functions
    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_render_pipeline_state_destroy(IGLRenderPipelineState* pipeline);

    // Render Command Encoder functions
    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_render_encoder_end_encoding(IGLRenderCommandEncoder* encoder);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_render_encoder_bind_vertex_buffer(
        IGLRenderCommandEncoder* encoder, uint index, IGLBuffer* buffer);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_render_encoder_bind_index_buffer(
        IGLRenderCommandEncoder* encoder, IGLBuffer* buffer, IGLIndexFormat format);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_render_encoder_bind_pipeline(
        IGLRenderCommandEncoder* encoder, IGLRenderPipelineState* pipeline);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_render_encoder_bind_uniform_buffer(
        IGLRenderCommandEncoder* encoder, uint index, IGLBuffer* buffer);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void igl_render_encoder_draw_indexed(IGLRenderCommandEncoder* encoder, uint index_count);

    // Texture functions
    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern uint igl_texture_get_format(IGLTexture* texture);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern float igl_texture_get_aspect_ratio(IGLTexture* texture);
}
