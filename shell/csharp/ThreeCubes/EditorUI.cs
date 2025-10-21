using System;
using System.Numerics;
using ImGuiNET;

namespace ThreeCubes;

/// <summary>
/// Unity-like editor UI with hierarchy, inspector, and viewport controls
/// </summary>
public static class EditorUI
{
    private static int selectedCubeIndex = -1;
    private static bool showHierarchy = true;
    private static bool showInspector = true;
    private static bool showStats = true;
    private static bool showDemo = false;

    public static void Render(ThreeCubesRenderSession session)
    {
        // Main menu bar
        if (ImGui.BeginMainMenuBar())
        {
            if (ImGui.BeginMenu("View"))
            {
                ImGui.MenuItem("Hierarchy", null, ref showHierarchy);
                ImGui.MenuItem("Inspector", null, ref showInspector);
                ImGui.MenuItem("Stats", null, ref showStats);
                ImGui.Separator();
                ImGui.MenuItem("ImGui Demo", null, ref showDemo);
                ImGui.EndMenu();
            }

            if (ImGui.BeginMenu("Help"))
            {
                if (ImGui.MenuItem("About"))
                {
                    Console.WriteLine("Three Cubes Editor - Built with IGL + C# + ImGui");
                }
                ImGui.EndMenu();
            }

            ImGui.EndMainMenuBar();
        }

        // Hierarchy Panel
        if (showHierarchy)
        {
            RenderHierarchy(session);
        }

        // Inspector Panel
        if (showInspector)
        {
            RenderInspector(session);
        }

        // Stats Panel
        if (showStats)
        {
            RenderStats();
        }

        // ImGui Demo Window
        if (showDemo)
        {
            ImGui.ShowDemoWindow(ref showDemo);
        }
    }

    private static void RenderHierarchy(ThreeCubesRenderSession session)
    {
        ImGui.SetNextWindowPos(new Vector2(10, 30), ImGuiCond.FirstUseEver);
        ImGui.SetNextWindowSize(new Vector2(250, 400), ImGuiCond.FirstUseEver);

        if (ImGui.Begin("Hierarchy", ref showHierarchy))
        {
            ImGui.Text("Scene Objects");
            ImGui.Separator();

            // Scene root
            if (ImGui.TreeNode("Scene"))
            {
                // Render cube list
                for (int i = 0; i < 3; i++)
                {
                    var cube = session.GetCube(i);
                    var flags = ImGuiTreeNodeFlags.Leaf | ImGuiTreeNodeFlags.NoTreePushOnOpen;
                    if (selectedCubeIndex == i)
                    {
                        flags |= ImGuiTreeNodeFlags.Selected;
                    }

                    ImGui.TreeNodeEx($"Cube {i} ({GetCubeName(i)})", flags);

                    if (ImGui.IsItemClicked())
                    {
                        selectedCubeIndex = i;
                    }
                }

                ImGui.TreePop();
            }

            ImGui.Spacing();
            ImGui.Separator();

            if (ImGui.Button("+ Add Object", new Vector2(-1, 0)))
            {
                Console.WriteLine("Add Object clicked (not implemented yet)");
            }
        }
        ImGui.End();
    }

    private static void RenderInspector(ThreeCubesRenderSession session)
    {
        ImGui.SetNextWindowPos(new Vector2(1020, 30), ImGuiCond.FirstUseEver);
        ImGui.SetNextWindowSize(new Vector2(250, 600), ImGuiCond.FirstUseEver);

        if (ImGui.Begin("Inspector", ref showInspector))
        {
            if (selectedCubeIndex >= 0 && selectedCubeIndex < 3)
            {
                var cube = session.GetCube(selectedCubeIndex);

                ImGui.Text($"Cube {selectedCubeIndex}: {GetCubeName(selectedCubeIndex)}");
                ImGui.Separator();

                // Transform Component
                if (ImGui.CollapsingHeader("Transform", ImGuiTreeNodeFlags.DefaultOpen))
                {
                    var pos = cube.Position;
                    if (ImGui.DragFloat3("Position", ref pos, 0.1f))
                    {
                        session.SetCubePosition(selectedCubeIndex, pos);
                    }

                    var rotAxis = cube.RotationAxis;
                    if (ImGui.DragFloat3("Rotation Axis", ref rotAxis, 0.01f))
                    {
                        session.SetCubeRotationAxis(selectedCubeIndex, Vector3.Normalize(rotAxis));
                    }

                    var rotSpeed = cube.RotationSpeed;
                    if (ImGui.DragFloat("Rotation Speed", ref rotSpeed, 0.01f, 0.0f, 10.0f))
                    {
                        session.SetCubeRotationSpeed(selectedCubeIndex, rotSpeed);
                    }

                    var angle = cube.CurrentAngle * (180.0f / MathF.PI); // Convert to degrees
                    ImGui.Text($"Current Angle: {angle:F1}°");
                }

                // Mesh Renderer Component
                if (ImGui.CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags.DefaultOpen))
                {
                    var color = cube.Color;
                    if (ImGui.ColorEdit3("Color", ref color))
                    {
                        session.SetCubeColor(selectedCubeIndex, color);
                    }

                    ImGui.Text("Mesh: Cube");
                    ImGui.Text("Material: Basic Color");
                }

                // Add Component Section
                ImGui.Spacing();
                ImGui.Separator();
                if (ImGui.Button("+ Add Component", new Vector2(-1, 0)))
                {
                    Console.WriteLine("Add Component clicked (not implemented yet)");
                }
            }
            else
            {
                ImGui.TextDisabled("No object selected");
                ImGui.TextWrapped("Select an object from the Hierarchy to view its properties.");
            }
        }
        ImGui.End();
    }

    private static void RenderStats()
    {
        ImGui.SetNextWindowPos(new Vector2(10, 440), ImGuiCond.FirstUseEver);
        ImGui.SetNextWindowSize(new Vector2(250, 120), ImGuiCond.FirstUseEver);

        if (ImGui.Begin("Stats", ref showStats))
        {
            ImGui.Text($"FPS: {ImGui.GetIO().Framerate:F1}");
            ImGui.Text($"Frame Time: {1000.0f / ImGui.GetIO().Framerate:F2} ms");
            ImGui.Separator();
            ImGui.Text($"Triangles: {36}"); // 12 triangles per cube * 3 cubes
            ImGui.Text($"Vertices: {24}");  // 8 vertices per cube * 3 cubes
        }
        ImGui.End();
    }

    private static string GetCubeName(int index)
    {
        return index switch
        {
            0 => "Red Left",
            1 => "Green Center",
            2 => "Blue Right",
            _ => "Unknown"
        };
    }
}
