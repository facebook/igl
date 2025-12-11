@echo off
REM Compile D3D12 ImGui shaders

set FXC="C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\fxc.exe"

echo Compiling vertex shader...
%FXC% /T vs_5_0 /E main /Fo imgui_vs_d3d12_fxc.cso imgui_vs_d3d12.hlsl
if %ERRORLEVEL% NEQ 0 (
    echo Vertex shader compilation failed!
    exit /b 1
)

echo Compiling pixel shader...
%FXC% /T ps_5_0 /E main /Fo imgui_ps_d3d12_fxc.cso imgui_ps_d3d12.hlsl
if %ERRORLEVEL% NEQ 0 (
    echo Pixel shader compilation failed!
    exit /b 1
)

echo Converting to C header files...
python -c "import sys; data = open('imgui_vs_d3d12_fxc.cso', 'rb').read(); print('unsigned char _tmp_imgui_vs_fxc_cso[] = {'); print(', '.join(f'0x{b:02x}' for b in data)); print('};'); print(f'unsigned int _tmp_imgui_vs_fxc_cso_len = {len(data)};')" > imgui_vs_d3d12_fxc.h

python -c "import sys; data = open('imgui_ps_d3d12_fxc.cso', 'rb').read(); print('unsigned char _tmp_imgui_ps_fxc_cso[] = {'); print(', '.join(f'0x{b:02x}' for b in data)); print('};'); print(f'unsigned int _tmp_imgui_ps_fxc_cso_len = {len(data)};')" > imgui_ps_d3d12_fxc.h

echo Done!
