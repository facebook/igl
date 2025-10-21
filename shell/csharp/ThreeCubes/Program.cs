using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using IGL.Bindings;
using Silk.NET.Windowing;
using Silk.NET.Maths;

namespace ThreeCubes;

class Program
{
    static void Main(string[] args)
    {
        Console.WriteLine("Three Rotating Cubes - IGL + C# + ImGui Demo");
        Console.WriteLine("============================================");
        Console.WriteLine("🎯 Editor UI with ImGui overlay! 🎯\n");

        var options = WindowOptions.Default;
        options.Title = "Three Rotating Cubes - IGL + C# + ImGui Editor";
        options.Size = new Vector2D<int>(1280, 800);
        options.Position = new Vector2D<int>(100, 100);
        options.IsVisible = true;
        options.API = new GraphicsAPI(
            ContextAPI.None,  // We're using IGL/Metal directly
            ContextProfile.Core,
            ContextFlags.Default,
            new APIVersion(1, 0)
        );

        var window = Window.Create(options);

        Platform? platform = null;
        Device? device = null;
        ThreeCubesRenderSession? session = null;
        ImGuiRenderer? imguiRenderer = null;
        Stopwatch? frameTimer = null;

        window.Load += () =>
        {
            try
            {
                Console.WriteLine($"Window size: {window.Size.X}x{window.Size.Y}");

                // Get native window handle
                var windowHandle = GetNativeWindowHandle(window);
                Console.WriteLine("Got native window handle");

                // Create IGL platform
                Console.WriteLine("Creating IGL Metal platform...");
                platform = new Platform(windowHandle, window.Size.X, window.Size.Y);
                Console.WriteLine("Platform created successfully");

                // Get device
                Console.WriteLine("Getting graphics device...");
                device = platform.GetDevice();
                Console.WriteLine($"Device obtained: backend = {device.BackendType}");

                // Create C# render session
                Console.WriteLine("Creating Three Cubes render session...");
                session = new ThreeCubesRenderSession(device);
                Console.WriteLine("Render session created successfully!");

                // Create ImGui renderer
                Console.WriteLine("Creating ImGui renderer...");
                imguiRenderer = new ImGuiRenderer(device, window.Size.X, window.Size.Y);
                Console.WriteLine("ImGui renderer created successfully!\n");

                frameTimer = Stopwatch.StartNew();

                Console.WriteLine("Starting render loop...");
                Console.WriteLine("Close the window to exit\n");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error during initialization: {ex}");
                window.Close();
            }
        };

        double lastFrameTime = 0;
        window.Render += (delta) =>
        {
            if (platform == null || device == null || session == null || imguiRenderer == null || frameTimer == null)
                return;

            try
            {
                // Calculate delta time
                double currentTime = frameTimer.Elapsed.TotalSeconds;
                double deltaTime = currentTime - lastFrameTime;
                lastFrameTime = currentTime;

                // Get textures for this frame
                var (colorTexture, depthTexture) = platform.GetFrameTextures();

                // Render 3D scene first
                session.Render(device, colorTexture, depthTexture);

                // Start ImGui frame and render UI
                imguiRenderer.NewFrame(deltaTime);
                RenderUI(session);
                imguiRenderer.Render(colorTexture, depthTexture);

                // Present frame
                platform.PresentFrame();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Render error: {ex}");
                window.Close();
            }
        };

        window.Resize += (size) =>
        {
            imguiRenderer?.UpdateWindowSize(size.X, size.Y);
        };

        window.Closing += () =>
        {
            Console.WriteLine("\nWindow close requested, cleaning up...");
            imguiRenderer?.Dispose();
            session?.Dispose();
            platform?.Dispose();
            Console.WriteLine("Cleanup complete");
        };

        window.Run();

        Console.WriteLine("Application exited successfully");
    }

    static void RenderUI(ThreeCubesRenderSession session)
    {
        EditorUI.Render(session);
    }

    private static IntPtr GetNativeWindowHandle(IWindow window)
    {
        if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
        {
            // On macOS, we need to get the NSView handle
            var handle = window.Native!.Cocoa!.Value;
            return handle;
        }
        else
        {
            throw new PlatformNotSupportedException("Only macOS is currently supported");
        }
    }
}
