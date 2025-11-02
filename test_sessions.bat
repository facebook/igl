@echo off
cd build\vs\shell\Debug

echo === BasicFramebufferSession ===
timeout /t 3 /nobreak > nul 2>&1 & taskkill /f /im BasicFramebufferSession_d3d12.exe > nul 2>&1
start /b BasicFramebufferSession_d3d12.exe > nul 2>&1
timeout /t 2 /nobreak > nul
taskkill /f /im BasicFramebufferSession_d3d12.exe > nul 2>&1
echo.

echo === ColorSession ===
timeout /t 1 /nobreak > nul 2>&1
start /b ColorSession_d3d12.exe > nul 2>&1
timeout /t 2 /nobreak > nul
taskkill /f /im ColorSession_d3d12.exe > nul 2>&1
echo.

echo === MRTSession ===
start /b MRTSession_d3d12.exe > nul 2>&1
timeout /t 2 /nobreak > nul
taskkill /f /im MRTSession_d3d12.exe > nul 2>&1
echo.

echo === BindGroupSession ===
start /b BindGroupSession_d3d12.exe 2>&1 | find "Error"
timeout /t 2 /nobreak > nul
taskkill /f /im BindGroupSession_d3d12.exe > nul 2>&1
echo.

echo === ImguiSession ===
start /b ImguiSession_d3d12.exe 2>&1 | find "Error"
timeout /t 2 /nobreak > nul
taskkill /f /im ImguiSession_d3d12.exe > nul 2>&1
echo.

cd ..\..\..\..
