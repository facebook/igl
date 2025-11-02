#!/usr/bin/env python3
"""Compile D3D12 ImGui shaders"""

import os
import subprocess
import sys

FXC = r"C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\fxc.exe"

def compile_shader(shader_file, profile, output_cso):
    """Compile HLSL shader to CSO"""
    print(f"Compiling {shader_file}...")
    cmd = [FXC, "/T", profile, "/E", "main", "/Fo", output_cso, shader_file]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"ERROR: {result.stderr}")
        return False
    print(f"  SUCCESS: {output_cso}")
    return True

def cso_to_header(cso_file, header_file, var_name):
    """Convert CSO binary to C header"""
    print(f"Converting {cso_file} to {header_file}...")
    with open(cso_file, 'rb') as f:
        data = f.read()

    with open(header_file, 'w') as f:
        f.write(f"unsigned char {var_name}[] = {{\n")
        for i in range(0, len(data), 12):
            chunk = data[i:i+12]
            hex_bytes = ', '.join(f'0x{b:02x}' for b in chunk)
            f.write(f"  {hex_bytes},\n")
        f.write("};\n")
        f.write(f"unsigned int {var_name}_len = {len(data)};\n")

    print(f"  SUCCESS: {header_file} ({len(data)} bytes)")

def main():
    os.chdir(os.path.dirname(os.path.abspath(__file__)))

    # Compile vertex shader
    if not compile_shader("imgui_vs_d3d12.hlsl", "vs_5_0", "imgui_vs_d3d12_fxc.cso"):
        return 1

    # Compile pixel shader
    if not compile_shader("imgui_ps_d3d12.hlsl", "ps_5_0", "imgui_ps_d3d12_fxc.cso"):
        return 1

    # Convert to headers
    cso_to_header("imgui_vs_d3d12_fxc.cso", "imgui_vs_d3d12_fxc.h", "_tmp_imgui_vs_fxc_cso")
    cso_to_header("imgui_ps_d3d12_fxc.cso", "imgui_ps_d3d12_fxc.h", "_tmp_imgui_ps_fxc_cso")

    print("\nAll shaders compiled successfully!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
