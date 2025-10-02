using System;
using System.Numerics;
using System.Runtime.InteropServices;
using IGL.Bindings;

namespace ThreeCubes;

/// <summary>
/// Ultra-simple triangle test to verify rendering works
/// </summary>
public class SimpleTriangleTest : IDisposable
{
    [StructLayout(LayoutKind.Sequential)]
    private struct VertexPosColor
    {
        public Vector3 Position;
        public Vector3 Color;
    }

    private const string MetalShaderSource = @"
        #include <metal_stdlib>
        using namespace metal;

        struct VertexIn {
            float3 position [[attribute(0)]];
            float3 color [[attribute(1)]];
        };

        struct VertexOut {
            float4 position [[position]];
            float3 color;
        };

        vertex VertexOut vertexShader(VertexIn in [[stage_in]]) {
            VertexOut out;
            out.position = float4(in.position, 1.0);
            out.color = in.color;
            return out;
        }

        fragment float4 fragmentShader(VertexOut in [[stage_in]]) {
            return float4(in.color, 1.0);
        }
    ";

    private readonly CommandQueue _commandQueue;
    private readonly IGL.Bindings.Buffer _vertexBuffer;
    private readonly IGL.Bindings.Buffer _indexBuffer;
    private readonly VertexInputState _vertexInputState;
    private readonly ShaderStages _shaderStages;

    private Framebuffer? _framebuffer;
    private RenderPipelineState? _pipelineState;

    public SimpleTriangleTest(Device device)
    {
        Console.WriteLine("Creating Simple Triangle Test...");

        // Simple triangle in clip space (-1 to +1)
        var vertices = new VertexPosColor[]
        {
            new() { Position = new(0.0f, 0.5f, 0.0f), Color = new(1.0f, 0.0f, 0.0f) },   // Top - Red
            new() { Position = new(-0.5f, -0.5f, 0.0f), Color = new(0.0f, 1.0f, 0.0f) }, // Bottom left - Green
            new() { Position = new(0.5f, -0.5f, 0.0f), Color = new(0.0f, 0.0f, 1.0f) },  // Bottom right - Blue
        };

        var vertexBytes = MemoryMarshal.AsBytes(vertices.AsSpan());
        _vertexBuffer = device.CreateBuffer(BufferType.Vertex, vertexBytes);

        var indices = new ushort[] { 0, 1, 2 };
        var indexBytes = MemoryMarshal.AsBytes(indices.AsSpan());
        _indexBuffer = device.CreateBuffer(BufferType.Index, indexBytes);

        Console.WriteLine($"Created vertex buffer: {vertexBytes.Length} bytes, index buffer: {indexBytes.Length} bytes");

        // Vertex input state
        var attributes = new VertexAttribute[]
        {
            new()
            {
                BufferIndex = 0,
                Format = VertexFormat.Float3,
                Offset = 0,
                Name = "position",
                Location = 0
            },
            new()
            {
                BufferIndex = 0,
                Format = VertexFormat.Float3,
                Offset = (uint)Marshal.SizeOf<Vector3>(),
                Name = "color",
                Location = 1
            }
        };

        var bindings = new VertexBinding[]
        {
            new() { Stride = (uint)Marshal.SizeOf<VertexPosColor>() }
        };

        _vertexInputState = device.CreateVertexInputState(attributes, bindings);

        // Shaders (no uniforms!)
        _shaderStages = device.CreateShaderStagesMetal(MetalShaderSource, "vertexShader", "fragmentShader");

        // Command queue
        _commandQueue = device.CreateCommandQueue();

        Console.WriteLine("Simple Triangle Test created!");
    }

    public void Render(Device device, Texture colorTexture, Texture depthTexture)
    {
        // Create or update framebuffer
        if (_framebuffer == null)
        {
            _framebuffer = device.CreateFramebuffer(colorTexture, depthTexture);
        }
        else
        {
            _framebuffer.UpdateDrawable(colorTexture);
        }

        // Create pipeline if needed
        if (_pipelineState == null)
        {
            var colorFormat = colorTexture.Format;
            var depthFormat = depthTexture.Format;

            _pipelineState = device.CreateRenderPipeline(
                _vertexInputState,
                _shaderStages,
                colorFormat,
                depthFormat,
                CullMode.None,  // No culling
                WindingMode.CounterClockwise);

            Console.WriteLine("Pipeline created!");
        }

        // Create command buffer
        var commandBuffer = _commandQueue.CreateCommandBuffer();

        // Render pass
        var colorAttachment = new ColorAttachment
        {
            LoadAction = LoadAction.Clear,
            StoreAction = StoreAction.Store,
            ClearColor = new Color(0.2f, 0.2f, 0.2f, 1.0f)  // Gray background
        };

        var depthAttachment = new DepthAttachment
        {
            LoadAction = LoadAction.Clear,
            ClearDepth = 1.0f
        };

        var encoder = commandBuffer.CreateRenderEncoder(_framebuffer, colorAttachment, depthAttachment);

        // Bind and draw
        encoder.BindVertexBuffer(0, _vertexBuffer);
        encoder.BindIndexBuffer(_indexBuffer, IndexFormat.UInt16);
        encoder.BindPipeline(_pipelineState);
        encoder.DrawIndexed(3);  // Draw 3 indices

        encoder.EndEncoding();

        // Present and submit
        commandBuffer.Present(colorTexture);
        _commandQueue.Submit(commandBuffer);

        commandBuffer.Dispose();
    }

    public void Dispose()
    {
        _pipelineState?.Dispose();
        _framebuffer?.Dispose();
        _shaderStages.Dispose();
        _vertexInputState.Dispose();
        _indexBuffer.Dispose();
        _vertexBuffer.Dispose();
        _commandQueue.Dispose();
    }
}
