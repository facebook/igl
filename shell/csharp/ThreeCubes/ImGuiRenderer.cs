using System;
using System.Numerics;
using System.Runtime.InteropServices;
using ImGuiNET;
using IGL.Bindings;

namespace ThreeCubes;

/// <summary>
/// ImGui renderer that uses IGL (Intermediate Graphics Library) as backend
/// Renders ImGui UI overlay on top of 3D content
/// </summary>
public class ImGuiRenderer : IDisposable
{
    private Device _device;
    private CommandQueue _commandQueue;
    private IGL.Bindings.Buffer? _vertexBuffer;
    private IGL.Bindings.Buffer? _indexBuffer;
    private ShaderStages? _shaderStages;
    private VertexInputState? _vertexInputState;
    private RenderPipelineState? _pipelineState;

    private int _windowWidth;
    private int _windowHeight;
    private int _vertexBufferSize = 10000;
    private int _indexBufferSize = 10000;

    // ImGui shader for Metal
    private const string ImGuiMetalShader = @"
        #include <metal_stdlib>
        using namespace metal;

        struct VertexIn {
            float2 position [[attribute(0)]];
            float2 uv [[attribute(1)]];
            float4 color [[attribute(2)]];
        };

        struct VertexOut {
            float4 position [[position]];
            float2 uv;
            float4 color;
        };

        struct Uniforms {
            float4x4 projectionMatrix;
        };

        vertex VertexOut vertexShader(VertexIn in [[stage_in]],
                                      constant Uniforms &uniforms [[buffer(1)]]) {
            VertexOut out;
            out.position = uniforms.projectionMatrix * float4(in.position, 0.0, 1.0);
            out.uv = in.uv;
            out.color = in.color;
            return out;
        }

        fragment float4 fragmentShader(VertexOut in [[stage_in]]) {
            // Simple solid color rendering (no texture)
            return in.color;
        }
    ";

    public ImGuiRenderer(Device device, int windowWidth, int windowHeight)
    {
        _device = device;
        _windowWidth = windowWidth;
        _windowHeight = windowHeight;

        _commandQueue = device.CreateCommandQueue();

        // Initialize ImGui
        ImGui.CreateContext();
        var io = ImGui.GetIO();
        io.DisplaySize = new Vector2(windowWidth, windowHeight);
        io.DisplayFramebufferScale = Vector2.One;

        // Build font atlas (required by ImGui)
        io.Fonts.AddFontDefault();
        io.Fonts.Build();

        // Setup basic style
        ImGui.StyleColorsDark();

        // Create shader
        _shaderStages = device.CreateShaderStagesMetal(
            ImGuiMetalShader,
            "vertexShader",
            "fragmentShader"
        );

        // Create vertex input state for ImGui vertices
        var attributes = new[]
        {
            new VertexAttribute
            {
                BufferIndex = 0,
                Format = VertexFormat.Float2,
                Offset = 0,
                Name = "position",
                Location = 0
            },
            new VertexAttribute
            {
                BufferIndex = 0,
                Format = VertexFormat.Float2,
                Offset = 8,
                Name = "uv",
                Location = 1
            },
            new VertexAttribute
            {
                BufferIndex = 0,
                Format = VertexFormat.Float4,
                Offset = 16,
                Name = "color",
                Location = 2
            }
        };

        var bindings = new[]
        {
            new VertexBinding { Stride = 32 } // float2 + float2 + float4 = 8+8+16 = 32 bytes
        };

        _vertexInputState = device.CreateVertexInputState(attributes, bindings);

        // Create initial buffers
        CreateBuffers();
    }

    private void CreateBuffers()
    {
        // Create vertex buffer with our custom format (32 bytes per vertex)
        var vertexData = new byte[_vertexBufferSize * 32];
        _vertexBuffer = _device.CreateBuffer(BufferType.Vertex, vertexData);

        // Create index buffer
        var indexData = new byte[_indexBufferSize * 2]; // 2 bytes per index (ushort)
        _indexBuffer = _device.CreateBuffer(BufferType.Index, indexData);
    }

    public void NewFrame(double deltaTime)
    {
        var io = ImGui.GetIO();
        io.DeltaTime = (float)deltaTime;
        io.DisplaySize = new Vector2(_windowWidth, _windowHeight);

        ImGui.NewFrame();
    }

    public void UpdateWindowSize(int width, int height)
    {
        _windowWidth = width;
        _windowHeight = height;
    }

    public void Render(Texture colorTexture, Texture depthTexture)
    {
        ImGui.Render();
        var drawData = ImGui.GetDrawData();

        if (drawData.CmdListsCount == 0)
            return;

        // Create framebuffer
        var framebuffer = _device.CreateFramebuffer(colorTexture, depthTexture);

        // Create pipeline if needed
        if (_pipelineState == null)
        {
            var colorFormat = colorTexture.Format;
            var depthFormat = depthTexture.Format;

            _pipelineState = _device.CreateRenderPipeline(
                _vertexInputState!,
                _shaderStages!,
                colorFormat,
                depthFormat,
                CullMode.None,
                WindingMode.CounterClockwise
            );
        }

        // Create orthographic projection matrix
        float L = drawData.DisplayPos.X;
        float R = drawData.DisplayPos.X + drawData.DisplaySize.X;
        float T = drawData.DisplayPos.Y;
        float B = drawData.DisplayPos.Y + drawData.DisplaySize.Y;

        // Row-major orthographic projection
        var projection = new Matrix4x4(
            2.0f / (R - L), 0.0f, 0.0f, 0.0f,
            0.0f, 2.0f / (T - B), 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f, 0.0f,
            (R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f
        );

        // Create command buffer
        var commandBuffer = _commandQueue.CreateCommandBuffer();

        // Create render encoder with load (not clear) to preserve existing 3D content
        var colorAttachment = new ColorAttachment
        {
            LoadAction = LoadAction.Load,
            StoreAction = StoreAction.Store,
            ClearColor = new Color(0, 0, 0, 0)
        };

        var depthAttachment = new DepthAttachment
        {
            LoadAction = LoadAction.Load,
            ClearDepth = 1.0f
        };

        var encoder = commandBuffer.CreateRenderEncoder(
            framebuffer,
            colorAttachment,
            depthAttachment
        );

        encoder.BindPipeline(_pipelineState);

        // Render command lists
        for (int n = 0; n < drawData.CmdListsCount; n++)
        {
            var cmdList = drawData.CmdLists[n];

            // Upload vertex buffer
            if (cmdList.VtxBuffer.Size > 0)
            {
                int vertexCount = cmdList.VtxBuffer.Size;

                // Recreate buffer if needed
                if (vertexCount > _vertexBufferSize)
                {
                    _vertexBufferSize = vertexCount;
                }

                // Convert ImDrawVert (20 bytes: float2 pos, float2 uv, uint col)
                // to our format (32 bytes: float2 pos, float2 uv, float4 col)
                var convertedData = new byte[vertexCount * 32];
                unsafe
                {
                    byte* src = (byte*)cmdList.VtxBuffer.Data;
                    fixed (byte* dst = convertedData)
                    {
                        for (int i = 0; i < vertexCount; i++)
                        {
                            // Copy position (8 bytes)
                            *(float*)(dst + i * 32 + 0) = *(float*)(src + i * 20 + 0);
                            *(float*)(dst + i * 32 + 4) = *(float*)(src + i * 20 + 4);

                            // Copy UV (8 bytes)
                            *(float*)(dst + i * 32 + 8) = *(float*)(src + i * 20 + 8);
                            *(float*)(dst + i * 32 + 12) = *(float*)(src + i * 20 + 12);

                            // Convert color from ABGR uint to float4
                            uint col = *(uint*)(src + i * 20 + 16);
                            *(float*)(dst + i * 32 + 16) = ((col >> 0) & 0xFF) / 255.0f;  // R
                            *(float*)(dst + i * 32 + 20) = ((col >> 8) & 0xFF) / 255.0f;  // G
                            *(float*)(dst + i * 32 + 24) = ((col >> 16) & 0xFF) / 255.0f; // B
                            *(float*)(dst + i * 32 + 28) = ((col >> 24) & 0xFF) / 255.0f; // A
                        }
                    }
                }

                // Update buffer (recreate for simplicity)
                _vertexBuffer?.Dispose();
                _vertexBuffer = _device.CreateBuffer(BufferType.Vertex, convertedData);
            }

            // Upload index buffer
            if (cmdList.IdxBuffer.Size > 0)
            {
                var indexSize = cmdList.IdxBuffer.Size * 2; // 2 bytes per ushort

                // Recreate buffer if needed
                if (indexSize > _indexBufferSize * 2)
                {
                    _indexBufferSize = cmdList.IdxBuffer.Size;
                    _indexBuffer?.Dispose();
                    var newData = new byte[_indexBufferSize * 2];
                    _indexBuffer = _device.CreateBuffer(BufferType.Index, newData);
                }

                // Copy index data
                var indexData = new byte[indexSize];
                unsafe
                {
                    System.Buffer.MemoryCopy(
                        (void*)cmdList.IdxBuffer.Data,
                        System.Runtime.CompilerServices.Unsafe.AsPointer(ref indexData[0]),
                        indexSize,
                        indexSize
                    );
                }

                // Update buffer (recreate for simplicity)
                _indexBuffer?.Dispose();
                _indexBuffer = _device.CreateBuffer(BufferType.Index, indexData);
            }

            // Bind buffers
            encoder.BindVertexBuffer(0, _vertexBuffer!);
            encoder.BindIndexBuffer(_indexBuffer!, IndexFormat.UInt16);

            // Create uniform buffer for projection matrix
            var uniformData = new byte[64]; // sizeof(Matrix4x4)
            unsafe
            {
                fixed (byte* ptr = uniformData)
                {
                    *(Matrix4x4*)ptr = projection;
                }
            }
            var uniformBuffer = _device.CreateBuffer(BufferType.Uniform, uniformData);
            encoder.BindUniformBuffer(1, uniformBuffer);

            // Execute draw commands
            // Note: Since we can't use index offset in DrawIndexed, we draw all indices
            // This works fine for ImGui as each command list is self-contained
            if (cmdList.CmdBuffer.Size > 0)
            {
                var totalIndices = (uint)cmdList.IdxBuffer.Size;
                encoder.DrawIndexed(totalIndices);
            }

            uniformBuffer.Dispose();
        }

        encoder.EndEncoding();
        commandBuffer.Present(colorTexture);
        _commandQueue.Submit(commandBuffer);

        framebuffer.Dispose();
    }

    public void Dispose()
    {
        _pipelineState?.Dispose();
        _vertexInputState?.Dispose();
        _shaderStages?.Dispose();
        _indexBuffer?.Dispose();
        _vertexBuffer?.Dispose();
        _commandQueue.Dispose();

        ImGui.DestroyContext();
    }
}
