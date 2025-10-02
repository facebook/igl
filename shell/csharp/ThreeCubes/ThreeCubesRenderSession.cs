using System;
using System.Numerics;
using System.Runtime.InteropServices;
using IGL.Bindings;

namespace ThreeCubes;

/// <summary>
/// Three Rotating Cubes Render Session - C# Implementation
/// </summary>
public class ThreeCubesRenderSession : IDisposable
{
    // Vertex structure matching C++ version
    [StructLayout(LayoutKind.Sequential)]
    private struct VertexPosColor
    {
        public Vector3 Position;
        public Vector3 Color;
    }

    // Uniform structure for MVP matrix
    [StructLayout(LayoutKind.Sequential)]
    private struct VertexUniforms
    {
        public Matrix4x4 MvpMatrix;
    }

    // Cube transform data
    private class CubeTransform
    {
        public Vector3 Position;
        public Vector3 RotationAxis;
        public float RotationSpeed;
        public float CurrentAngle;
        public Vector3 Color;
    }

    // Metal shader source matching Rust version
    private const string MetalShaderSource = @"
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
    ";

    private readonly CommandQueue _commandQueue;
    private readonly IGL.Bindings.Buffer _vertexBuffer;
    private readonly IGL.Bindings.Buffer _indexBuffer;
    private readonly VertexInputState _vertexInputState;
    private readonly ShaderStages _shaderStages;
    private readonly CubeTransform[] _cubes;
    private readonly DateTime _startTime;
    private DateTime _lastFrameTime;
    private int frameCount_ = 0;

    private Framebuffer? _framebuffer;
    private RenderPipelineState? _pipelineState;

    public ThreeCubesRenderSession(Device device)
    {
        Console.WriteLine("Creating Three Cubes Render Session in C#...");

        // Create cube geometry
        var vertices = CreateCubeVertices();
        var indices = CreateCubeIndices();

        // Create buffers
        var vertexBytes = MemoryMarshal.AsBytes(vertices.AsSpan());
        var indexBytes = MemoryMarshal.AsBytes(indices.AsSpan());

        _vertexBuffer = device.CreateBuffer(BufferType.Vertex, vertexBytes);
        _indexBuffer = device.CreateBuffer(BufferType.Index, indexBytes);

        Console.WriteLine($"Created vertex buffer ({vertexBytes.Length} bytes, {vertices.Length} vertices) and index buffer ({indexBytes.Length} bytes, {indices.Length} indices)");
        Console.WriteLine($"First vertex: pos=({vertices[0].Position.X}, {vertices[0].Position.Y}, {vertices[0].Position.Z}), color=({vertices[0].Color.X}, {vertices[0].Color.Y}, {vertices[0].Color.Z})");

        // Create vertex input state
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
                Name = "color_in",
                Location = 1
            }
        };

        var bindings = new VertexBinding[]
        {
            new() { Stride = (uint)Marshal.SizeOf<VertexPosColor>() }
        };

        _vertexInputState = device.CreateVertexInputState(attributes, bindings);

        // Create shaders
        _shaderStages = device.CreateShaderStagesMetal(MetalShaderSource, "vertexShader", "fragmentShader");

        // Create command queue
        _commandQueue = device.CreateCommandQueue();

        // Initialize cube transforms
        _cubes = new[]
        {
            // Cube 1: Left, rotating on Y axis, red
            new CubeTransform
            {
                Position = new Vector3(-3.0f, 0.0f, 0.0f),
                RotationAxis = Vector3.Normalize(new Vector3(0.0f, 1.0f, 0.0f)),
                RotationSpeed = 1.0f,
                CurrentAngle = 0.0f,
                Color = new Vector3(1.0f, 0.3f, 0.3f)
            },
            // Cube 2: Center, rotating on diagonal axis, green
            new CubeTransform
            {
                Position = new Vector3(0.0f, 0.0f, 0.0f),
                RotationAxis = Vector3.Normalize(new Vector3(1.0f, 1.0f, 0.0f)),
                RotationSpeed = 1.5f,
                CurrentAngle = 0.0f,
                Color = new Vector3(0.3f, 1.0f, 0.3f)
            },
            // Cube 3: Right, rotating on XZ diagonal, blue
            new CubeTransform
            {
                Position = new Vector3(3.0f, 0.0f, 0.0f),
                RotationAxis = Vector3.Normalize(new Vector3(1.0f, 0.0f, 1.0f)),
                RotationSpeed = 0.75f,
                CurrentAngle = 0.0f,
                Color = new Vector3(0.3f, 0.3f, 1.0f)
            }
        };

        _startTime = DateTime.Now;
        _lastFrameTime = _startTime;

        Console.WriteLine("C# Render Session created successfully!");
    }

    public void Render(Device device, Texture colorTexture, Texture depthTexture)
    {
        // Update cube rotations
        var now = DateTime.Now;
        var deltaTime = (float)(now - _lastFrameTime).TotalSeconds;
        _lastFrameTime = now;

        foreach (var cube in _cubes)
        {
            cube.CurrentAngle += cube.RotationSpeed * deltaTime;
        }

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
                CullMode.Back,
                WindingMode.Clockwise);
        }

        // Create command buffer
        var commandBuffer = _commandQueue.CreateCommandBuffer();

        // Create render encoder
        var colorAttachment = new ColorAttachment
        {
            LoadAction = LoadAction.Clear,
            StoreAction = StoreAction.Store,
            ClearColor = new Color(0.1f, 0.1f, 0.15f, 1.0f)
        };

        var depthAttachment = new DepthAttachment
        {
            LoadAction = LoadAction.Clear,
            ClearDepth = 1.0f
        };

        frameCount_++;

        var encoder = commandBuffer.CreateRenderEncoder(_framebuffer, colorAttachment, depthAttachment);

        // Bind vertex and index buffers
        encoder.BindVertexBuffer(0, _vertexBuffer);
        encoder.BindIndexBuffer(_indexBuffer, IndexFormat.UInt16);
        encoder.BindPipeline(_pipelineState);

        // Render each cube
        var aspectRatio = colorTexture.AspectRatio;

        for (int i = 0; i < _cubes.Length; i++)
        {
            var cube = _cubes[i];

            // Create MVP matrix
            var mvp = CreateMvpMatrix(cube, aspectRatio);

            if (frameCount_ == 1 && i == 0)
            {
                // Also compute and display the row-major version BEFORE transpose
                var projection = CreatePerspectiveLH(45.0f * MathF.PI / 180.0f, aspectRatio, 0.1f, 100.0f);
                var positionWithDepth = cube.Position + new Vector3(0.0f, 0.0f, 8.0f);
                var translation = Matrix4x4.CreateTranslation(positionWithDepth);
                var rotation = Matrix4x4.CreateFromAxisAngle(cube.RotationAxis, cube.CurrentAngle);
                var model = translation * rotation;
                var mvpRowMajor = model * projection;  // Reversed order!

                Console.WriteLine($"[C# MVP Matrix (Row-Major, BEFORE transpose)]:");
                Console.WriteLine($"  Row0: [{mvpRowMajor.M11,8:F4} {mvpRowMajor.M12,8:F4} {mvpRowMajor.M13,8:F4} {mvpRowMajor.M14,8:F4}]");
                Console.WriteLine($"  Row1: [{mvpRowMajor.M21,8:F4} {mvpRowMajor.M22,8:F4} {mvpRowMajor.M23,8:F4} {mvpRowMajor.M24,8:F4}]");
                Console.WriteLine($"  Row2: [{mvpRowMajor.M31,8:F4} {mvpRowMajor.M32,8:F4} {mvpRowMajor.M33,8:F4} {mvpRowMajor.M34,8:F4}]");
                Console.WriteLine($"  Row3: [{mvpRowMajor.M41,8:F4} {mvpRowMajor.M42,8:F4} {mvpRowMajor.M43,8:F4} {mvpRowMajor.M44,8:F4}]");
                Console.WriteLine($"[C# MVP Matrix (Column-Major, AFTER transpose, sent to Metal)]:");
                Console.WriteLine($"  Row0: [{mvp.M11,8:F4} {mvp.M12,8:F4} {mvp.M13,8:F4} {mvp.M14,8:F4}]");
                Console.WriteLine($"  Row1: [{mvp.M21,8:F4} {mvp.M22,8:F4} {mvp.M23,8:F4} {mvp.M24,8:F4}]");
                Console.WriteLine($"  Row2: [{mvp.M31,8:F4} {mvp.M32,8:F4} {mvp.M33,8:F4} {mvp.M34,8:F4}]");
                Console.WriteLine($"  Row3: [{mvp.M41,8:F4} {mvp.M42,8:F4} {mvp.M43,8:F4} {mvp.M44,8:F4}]");
                Console.WriteLine($"  aspectRatio={aspectRatio:F4}, cube.Position=({cube.Position.X}, {cube.Position.Y}, {cube.Position.Z})");
            }

            var uniforms = new VertexUniforms { MvpMatrix = mvp };

            // Create uniform buffer for this cube
            var uniformBytes = MemoryMarshal.AsBytes(new[] { uniforms }.AsSpan());

            if (frameCount_ == 1 && i == 0)
            {
                Console.WriteLine("[C# Uniform bytes (first 64 bytes, as floats)]:");
                var floats = MemoryMarshal.Cast<byte, float>(uniformBytes);
                for (int j = 0; j < 16; j += 4)
                {
                    Console.WriteLine($"  [{floats[j],8:F4} {floats[j+1],8:F4} {floats[j+2],8:F4} {floats[j+3],8:F4}]");
                }
                Console.WriteLine("[What Rust sends (for comparison)]:");
                Console.WriteLine("  [ 1.8096  0.0000 -0.0349 -0.0349]");
                Console.WriteLine("  [ 0.0000  2.4142  0.0000  0.0000]");
                Console.WriteLine("  [ 0.0632  0.0000  1.0004  0.9994]");
                Console.WriteLine("  [-5.4320  0.0000  7.9079  8.0000]");
            }

            using var uniformBuffer = device.CreateBuffer(BufferType.Uniform, uniformBytes);

            // Bind uniforms and draw
            encoder.BindUniformBuffer(1, uniformBuffer);
            encoder.DrawIndexed(36);
        }

        // End encoding
        encoder.EndEncoding();

        // Present the drawable
        commandBuffer.Present(colorTexture);

        // Submit
        _commandQueue.Submit(commandBuffer);

        commandBuffer.Dispose();
    }

    private Matrix4x4 CreateMvpMatrix(CubeTransform cube, float aspectRatio)
    {
        // Create left-handed perspective projection (in row-major)
        var fov = 45.0f * MathF.PI / 180.0f;
        var projection = CreatePerspectiveLH(fov, aspectRatio, 0.1f, 100.0f);

        // Model transform: translate cube forward in Z (8.0), then to position, then rotate
        var positionWithDepth = cube.Position + new Vector3(0.0f, 0.0f, 8.0f);
        var translation = Matrix4x4.CreateTranslation(positionWithDepth);
        var rotation = Matrix4x4.CreateFromAxisAngle(cube.RotationAxis, cube.CurrentAngle);
        var model =  rotation * translation;

        // IMPORTANT: To match column-major multiplication (projection_col * model_col),
        // we need to reverse the order in row-major and then transpose:
        // (A_col * B_col) = transpose(B_row * A_row)
        var mvp = model * projection;

        // Transpose to column-major for Metal
        return mvp; //Matrix4x4.Transpose(mvp);
    }

    // Build left-handed perspective matrix matching glam::Mat4::perspective_lh
    // This creates the matrix in row-major format (C# default)
    private static Matrix4x4 CreatePerspectiveLH(float fovY, float aspect, float nearZ, float farZ)
    {
        var tanHalfFovy = MathF.Tan(fovY / 2.0f);
        var f = 1.0f / tanHalfFovy;

        // glam's perspective_lh formula (converted to row-major):
        // [f/aspect    0         0              0        ]
        // [0           f         0              0        ]
        // [0           0         far/(far-near) 1        ]
        // [0           0        -near*far/(far-near)  0  ]

        var m = new Matrix4x4();
        m.M11 = f / aspect;
        m.M12 = 0;
        m.M13 = 0;
        m.M14 = 0;

        m.M21 = 0;
        m.M22 = f;
        m.M23 = 0;
        m.M24 = 0;

        m.M31 = 0;
        m.M32 = 0;
        m.M33 = farZ / (farZ - nearZ);
        m.M34 = 1.0f;

        m.M41 = 0;
        m.M42 = 0;
        m.M43 = -(nearZ * farZ) / (farZ - nearZ);
        m.M44 = 0;

        return m;
    }

    // Helper to create left-handed look-at matrix
    private static Matrix4x4 CreateLookAtLH(Vector3 eye, Vector3 target, Vector3 up)
    {
        var zaxis = Vector3.Normalize(target - eye);  // Forward
        var xaxis = Vector3.Normalize(Vector3.Cross(up, zaxis));  // Right
        var yaxis = Vector3.Cross(zaxis, xaxis);  // Up

        var result = new Matrix4x4();
        result.M11 = xaxis.X;
        result.M12 = yaxis.X;
        result.M13 = zaxis.X;
        result.M14 = 0.0f;

        result.M21 = xaxis.Y;
        result.M22 = yaxis.Y;
        result.M23 = zaxis.Y;
        result.M24 = 0.0f;

        result.M31 = xaxis.Z;
        result.M32 = yaxis.Z;
        result.M33 = zaxis.Z;
        result.M34 = 0.0f;

        result.M41 = -Vector3.Dot(xaxis, eye);
        result.M42 = -Vector3.Dot(yaxis, eye);
        result.M43 = -Vector3.Dot(zaxis, eye);
        result.M44 = 1.0f;

        return result;
    }

    private static VertexPosColor[] CreateCubeVertices()
    {
        // Simple 8-vertex cube matching Rust version
        float half = 1.0f;
        return new[]
        {
            // Front face (red tint)
            new VertexPosColor { Position = new(-half, half, -half), Color = new(1.0f, 0.3f, 0.3f) },
            new VertexPosColor { Position = new(half, half, -half), Color = new(1.0f, 0.3f, 0.3f) },
            new VertexPosColor { Position = new(-half, -half, -half), Color = new(0.8f, 0.2f, 0.2f) },
            new VertexPosColor { Position = new(half, -half, -half), Color = new(0.8f, 0.2f, 0.2f) },
            // Back face (blue tint)
            new VertexPosColor { Position = new(half, half, half), Color = new(0.3f, 0.3f, 1.0f) },
            new VertexPosColor { Position = new(-half, half, half), Color = new(0.3f, 0.3f, 1.0f) },
            new VertexPosColor { Position = new(half, -half, half), Color = new(0.2f, 0.2f, 0.8f) },
            new VertexPosColor { Position = new(-half, -half, half), Color = new(0.2f, 0.2f, 0.8f) },
        };
    }

    private static ushort[] CreateCubeIndices()
    {
        // Index data matching Rust version (8-vertex cube)
        return new ushort[]
        {
            0, 1, 2, 1, 3, 2, // front
            1, 4, 3, 4, 6, 3, // right
            4, 5, 6, 5, 7, 6, // back
            5, 0, 7, 0, 2, 7, // left
            5, 4, 0, 4, 1, 0, // top
            2, 3, 7, 3, 6, 7, // bottom
        };
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
