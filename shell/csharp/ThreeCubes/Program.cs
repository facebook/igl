using System;
using System.Runtime.InteropServices;
using IGL.Bindings;
using Silk.NET.Windowing;
using Silk.NET.Maths;

namespace ThreeCubes;

class Program
{
    static void Main(string[] args)
    {
        Console.WriteLine("Three Rotating Cubes - IGL + C# Demo");
        Console.WriteLine("=====================================");
        Console.WriteLine("🎯 Render session implemented in C#! 🎯\n");

        var options = WindowOptions.Default;
        options.Title = "Three Rotating Cubes - IGL + C# (Native C# Renderer)";
        options.Size = new Vector2D<int>(1024, 768);
        options.Position = new Vector2D<int>(100, 100);  // Ensure visible position
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
                Console.WriteLine("Render session created successfully!\n");

                Console.WriteLine("Starting render loop...");
                Console.WriteLine("Close the window to exit\n");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error during initialization: {ex}");
                window.Close();
            }
        };

        window.Render += (delta) =>
        {
            if (platform == null || device == null || session == null)
                return;

            try
            {
                // Get textures for this frame
                var (colorTexture, depthTexture) = platform.GetFrameTextures();

                // Render using C# render session
                session.Render(device, colorTexture, depthTexture);

                // Present frame
                platform.PresentFrame();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Render error: {ex}");
                window.Close();
            }
        };

        window.Closing += () =>
        {
            Console.WriteLine("\nWindow close requested, cleaning up...");
            session?.Dispose();
            platform?.Dispose();
            Console.WriteLine("Cleanup complete");
        };

        window.Run();

        Console.WriteLine("Application exited successfully");
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
