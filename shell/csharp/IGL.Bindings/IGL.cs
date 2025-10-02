using System;
using System.Runtime.InteropServices;
using System.Text;
using static IGL.Bindings.Native;

namespace IGL.Bindings;

/// <summary>
/// Safe wrapper for IGL Platform
/// </summary>
public unsafe class Platform : IDisposable
{
    private IGLPlatform* _handle;

    public Platform(IntPtr windowHandle, int width, int height)
    {
        _handle = igl_platform_create_metal((void*)windowHandle, width, height);
        if (_handle == null)
            throw new InvalidOperationException("Failed to create IGL Metal platform");
    }

    public Device GetDevice()
    {
        var device = igl_platform_get_device(_handle);
        if (device == null)
            throw new InvalidOperationException("Failed to get device from platform");
        return new Device(device);
    }

    public (Texture Color, Texture Depth) GetFrameTextures()
    {
        IGLFrameTextures textures;
        if (!igl_platform_get_frame_textures(_handle, &textures))
            throw new InvalidOperationException("Failed to get frame textures");

        return (new Texture(textures.Color, false), new Texture(textures.Depth, false));
    }

    public void PresentFrame()
    {
        igl_platform_present_frame(_handle);
    }

    public void Dispose()
    {
        if (_handle != null)
        {
            igl_platform_destroy(_handle);
            _handle = null;
        }
    }
}

/// <summary>
/// Safe wrapper for IGL Device (non-owning)
/// </summary>
public unsafe class Device
{
    internal readonly IGLDevice* Handle;

    internal Device(IGLDevice* handle)
    {
        Handle = handle;
    }

    public BackendType BackendType => (BackendType)igl_device_get_backend_type(Handle);

    public CommandQueue CreateCommandQueue()
    {
        var queue = igl_device_create_command_queue(Handle);
        if (queue == null)
            throw new InvalidOperationException("Failed to create command queue");
        return new CommandQueue(queue);
    }

    public Buffer CreateBuffer(BufferType type, ReadOnlySpan<byte> data)
    {
        fixed (byte* ptr = data)
        {
            var buffer = igl_device_create_buffer(Handle, (IGLBufferType)type, ptr, (uint)data.Length);
            if (buffer == null)
                throw new InvalidOperationException("Failed to create buffer");
            return new Buffer(buffer);
        }
    }

    public ShaderStages CreateShaderStagesMetal(string source, string vertexEntry, string fragmentEntry)
    {
        var sourceBytes = Encoding.UTF8.GetBytes(source + "\0");
        var vertexBytes = Encoding.UTF8.GetBytes(vertexEntry + "\0");
        var fragmentBytes = Encoding.UTF8.GetBytes(fragmentEntry + "\0");

        fixed (byte* srcPtr = sourceBytes, vertPtr = vertexBytes, fragPtr = fragmentBytes)
        {
            var stages = igl_device_create_shader_stages_metal(Handle, srcPtr, vertPtr, fragPtr);
            if (stages == null)
                throw new InvalidOperationException("Failed to create shader stages");
            return new ShaderStages(stages);
        }
    }

    public VertexInputState CreateVertexInputState(VertexAttribute[] attributes, VertexBinding[] bindings)
    {
        var nativeAttrs = new IGLVertexAttribute[attributes.Length];
        var namePtrs = new IntPtr[attributes.Length];

        for (int i = 0; i < attributes.Length; i++)
        {
            namePtrs[i] = Marshal.StringToHGlobalAnsi(attributes[i].Name);
            nativeAttrs[i] = new IGLVertexAttribute
            {
                BufferIndex = attributes[i].BufferIndex,
                Format = (IGLVertexFormat)attributes[i].Format,
                Offset = attributes[i].Offset,
                Name = namePtrs[i],
                Location = attributes[i].Location
            };
        }

        var nativeBindings = new IGLVertexBinding[bindings.Length];
        for (int i = 0; i < bindings.Length; i++)
        {
            nativeBindings[i] = new IGLVertexBinding { Stride = bindings[i].Stride };
        }

        try
        {
            fixed (IGLVertexAttribute* attrPtr = nativeAttrs)
            fixed (IGLVertexBinding* bindPtr = nativeBindings)
            {
                var state = igl_device_create_vertex_input_state(
                    Handle, attrPtr, (uint)attributes.Length, bindPtr, (uint)bindings.Length);

                if (state == null)
                    throw new InvalidOperationException("Failed to create vertex input state");

                return new VertexInputState(state);
            }
        }
        finally
        {
            foreach (var ptr in namePtrs)
                Marshal.FreeHGlobal(ptr);
        }
    }

    public Framebuffer CreateFramebuffer(Texture colorTexture, Texture depthTexture)
    {
        var fb = igl_device_create_framebuffer(Handle, colorTexture.Handle, depthTexture.Handle);
        if (fb == null)
            throw new InvalidOperationException("Failed to create framebuffer");
        return new Framebuffer(fb);
    }

    public RenderPipelineState CreateRenderPipeline(
        VertexInputState vertexInput,
        ShaderStages shaders,
        uint colorFormat,
        uint depthFormat,
        CullMode cullMode,
        WindingMode windingMode)
    {
        var pipeline = igl_device_create_render_pipeline(
            Handle, vertexInput.Handle, shaders.Handle,
            colorFormat, depthFormat,
            (IGLCullMode)cullMode, (IGLWindingMode)windingMode);

        if (pipeline == null)
            throw new InvalidOperationException("Failed to create render pipeline");

        return new RenderPipelineState(pipeline);
    }
}

public unsafe class CommandQueue : IDisposable
{
    internal readonly IGLCommandQueue* Handle;

    internal CommandQueue(IGLCommandQueue* handle) => Handle = handle;

    public CommandBuffer CreateCommandBuffer()
    {
        var buffer = igl_command_queue_create_command_buffer(Handle);
        if (buffer == null)
            throw new InvalidOperationException("Failed to create command buffer");
        return new CommandBuffer(buffer);
    }

    public void Submit(CommandBuffer buffer)
    {
        igl_command_queue_submit(Handle, buffer.Handle);
    }

    public void Dispose()
    {
        if (Handle != null)
            igl_command_queue_destroy(Handle);
    }
}

public unsafe class CommandBuffer : IDisposable
{
    internal readonly IGLCommandBuffer* Handle;

    internal CommandBuffer(IGLCommandBuffer* handle) => Handle = handle;

    public void Present(Texture texture)
    {
        igl_command_buffer_present(Handle, texture.Handle);
    }

    public RenderCommandEncoder CreateRenderEncoder(
        Framebuffer framebuffer,
        ColorAttachment colorAttachment,
        DepthAttachment depthAttachment)
    {
        var nativeColor = new IGLColorAttachment
        {
            LoadAction = (IGLLoadAction)colorAttachment.LoadAction,
            StoreAction = (IGLStoreAction)colorAttachment.StoreAction,
            ClearColor = new IGLColor
            {
                R = colorAttachment.ClearColor.R,
                G = colorAttachment.ClearColor.G,
                B = colorAttachment.ClearColor.B,
                A = colorAttachment.ClearColor.A
            }
        };

        var nativeDepth = new IGLDepthAttachment
        {
            LoadAction = (IGLLoadAction)depthAttachment.LoadAction,
            ClearDepth = depthAttachment.ClearDepth
        };

        var encoder = igl_command_buffer_create_render_encoder(
            Handle, framebuffer.Handle, &nativeColor, &nativeDepth);

        if (encoder == null)
            throw new InvalidOperationException("Failed to create render encoder");

        return new RenderCommandEncoder(encoder);
    }

    public void Dispose()
    {
        if (Handle != null)
            igl_command_buffer_destroy(Handle);
    }
}

public unsafe class Buffer : IDisposable
{
    internal readonly IGLBuffer* Handle;

    internal Buffer(IGLBuffer* handle) => Handle = handle;

    public void Dispose()
    {
        if (Handle != null)
            igl_buffer_destroy(Handle);
    }
}

public unsafe class Texture
{
    internal readonly IGLTexture* Handle;
    private readonly bool _owned;

    internal Texture(IGLTexture* handle, bool owned = true)
    {
        Handle = handle;
        _owned = owned;
    }

    public uint Format => igl_texture_get_format(Handle);
    public float AspectRatio => igl_texture_get_aspect_ratio(Handle);
}

public unsafe class ShaderStages : IDisposable
{
    internal readonly IGLShaderStages* Handle;

    internal ShaderStages(IGLShaderStages* handle) => Handle = handle;

    public void Dispose()
    {
        if (Handle != null)
            igl_shader_stages_destroy(Handle);
    }
}

public unsafe class VertexInputState : IDisposable
{
    internal readonly IGLVertexInputState* Handle;

    internal VertexInputState(IGLVertexInputState* handle) => Handle = handle;

    public void Dispose()
    {
        if (Handle != null)
            igl_vertex_input_state_destroy(Handle);
    }
}

public unsafe class Framebuffer : IDisposable
{
    internal readonly IGLFramebuffer* Handle;

    internal Framebuffer(IGLFramebuffer* handle) => Handle = handle;

    public void UpdateDrawable(Texture colorTexture)
    {
        igl_framebuffer_update_drawable(Handle, colorTexture.Handle);
    }

    public Texture GetColorAttachment()
    {
        var texture = igl_framebuffer_get_color_attachment(Handle);
        if (texture == null)
            throw new InvalidOperationException("Failed to get color attachment");
        return new Texture(texture, false);
    }

    public void Dispose()
    {
        if (Handle != null)
            igl_framebuffer_destroy(Handle);
    }
}

public unsafe class RenderPipelineState : IDisposable
{
    internal readonly IGLRenderPipelineState* Handle;

    internal RenderPipelineState(IGLRenderPipelineState* handle) => Handle = handle;

    public void Dispose()
    {
        if (Handle != null)
            igl_render_pipeline_state_destroy(Handle);
    }
}

public unsafe class RenderCommandEncoder
{
    internal readonly IGLRenderCommandEncoder* Handle;

    internal RenderCommandEncoder(IGLRenderCommandEncoder* handle) => Handle = handle;

    public void BindVertexBuffer(uint index, Buffer buffer)
    {
        igl_render_encoder_bind_vertex_buffer(Handle, index, buffer.Handle);
    }

    public void BindIndexBuffer(Buffer buffer, IndexFormat format)
    {
        igl_render_encoder_bind_index_buffer(Handle, buffer.Handle, (IGLIndexFormat)format);
    }

    public void BindPipeline(RenderPipelineState pipeline)
    {
        igl_render_encoder_bind_pipeline(Handle, pipeline.Handle);
    }

    public void BindUniformBuffer(uint index, Buffer buffer)
    {
        igl_render_encoder_bind_uniform_buffer(Handle, index, buffer.Handle);
    }

    public void DrawIndexed(uint indexCount)
    {
        igl_render_encoder_draw_indexed(Handle, indexCount);
    }

    public void EndEncoding()
    {
        igl_render_encoder_end_encoding(Handle);
        // Note: encoder is destroyed after endEncoding in the C wrapper
    }
}

// Enums for public API
public enum BackendType
{
    Invalid = 0,
    OpenGL = 1,
    Metal = 2,
    Vulkan = 3
}

public enum BufferType
{
    Vertex = 1,
    Index = 2,
    Uniform = 4
}

public enum VertexFormat
{
    Float1 = 0,
    Float2 = 1,
    Float3 = 2,
    Float4 = 3
}

public enum IndexFormat
{
    UInt16 = 0,
    UInt32 = 1
}

public enum LoadAction
{
    DontCare = 0,
    Load = 1,
    Clear = 2
}

public enum StoreAction
{
    DontCare = 0,
    Store = 1
}

public enum CullMode
{
    None = 0,
    Front = 1,
    Back = 2
}

public enum WindingMode
{
    Clockwise = 0,
    CounterClockwise = 1
}

// Structs for public API
public struct Color
{
    public float R, G, B, A;
    public Color(float r, float g, float b, float a) => (R, G, B, A) = (r, g, b, a);
}

public struct VertexAttribute
{
    public uint BufferIndex;
    public VertexFormat Format;
    public uint Offset;
    public string Name;
    public uint Location;
}

public struct VertexBinding
{
    public uint Stride;
}

public struct ColorAttachment
{
    public LoadAction LoadAction;
    public StoreAction StoreAction;
    public Color ClearColor;
}

public struct DepthAttachment
{
    public LoadAction LoadAction;
    public float ClearDepth;
}
