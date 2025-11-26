@echo off
setlocal EnableExtensions EnableDelayedExpansion

set SESSIONS=BasicFramebufferSession ColorSession DrawInstancedSession EmptySession HelloTriangleSession HelloWorldSession MRTSession Textured3DCubeSession TextureViewSession ThreeCubesRenderSession TinyMeshSession TinyMeshBindGroupSession TQSession BindGroupSession ImguiSession TQMultiRenderPassSession ComputeSession

set PASS_COUNT=0
set FAIL_COUNT=0
set TIMEOUT_SECONDS=30

echo.
echo ========================================
echo Building D3D12 render sessions (Debug)
echo ========================================

REM Build only the D3D12 shell targets, not Metal/Vulkan/unit tests
for %%S in (%SESSIONS%) do (
    cmake --build build/vs --config Debug --target %%S_d3d12 --parallel 8 >> artifacts\render_sessions_build.log 2>&1
    if !ERRORLEVEL! neq 0 (
        echo BUILD FAILED for %%S_d3d12 - see artifacts\render_sessions_build.log
        type artifacts\render_sessions_build.log | findstr /C:"error"
        exit /b 1
    )
)
echo BUILD SUCCESSFUL

cd build\vs\shell\Debug

for %%S in (%SESSIONS%) do (
    set "EXEC=%%S_d3d12.exe"
    set "LOGFILE=temp_%%S.log"

    echo.
    echo ========================================
    echo Launching !EXEC! ^(parallel^) with timeout %TIMEOUT_SECONDS% seconds
    echo ========================================

    start "" /B cmd /c ".\!EXEC! > !LOGFILE! 2>&1"
)

echo.
echo Waiting %TIMEOUT_SECONDS% seconds for sessions to run...
powershell -NoProfile -Command "Start-Sleep -Seconds %TIMEOUT_SECONDS%" >nul
echo Forcing shutdown of sessions...
for %%S in (%SESSIONS%) do (
    taskkill /F /IM "%%S_d3d12.exe" >nul 2>&1
)
powershell -NoProfile -Command "Start-Sleep -Seconds 3" >nul

for %%S in (%SESSIONS%) do (
    set "EXEC=%%S_d3d12.exe"
    set "LOGFILE=temp_%%S.log"

    echo.
    echo ========================================
    echo Collecting results for !EXEC!
    echo ========================================

    if exist "!LOGFILE!" (
        findstr /C:"Frame 0" "!LOGFILE!" >nul
        if !ERRORLEVEL! equ 0 (
            findstr /C:"Signaled fence for frame" "!LOGFILE!" >nul
            if !ERRORLEVEL! equ 0 (
                echo RESULT: PASS - Rendered frames successfully
                set /A PASS_COUNT+=1
            ) else (
                echo RESULT: FAIL - Started but did not complete frames
                type "!LOGFILE!" | findstr /C:"Error" /C:"Assert" /C:"Exception"
                set /A FAIL_COUNT+=1
            )
        ) else (
            echo RESULT: FAIL - Did not start properly
            type "!LOGFILE!" | findstr /C:"Error" /C:"Assert" /C:"Exception"
            set /A FAIL_COUNT+=1
        )
    ) else (
        echo RESULT: FAIL - No log produced
        set /A FAIL_COUNT+=1
    )

    if exist "!LOGFILE!" del "!LOGFILE!"
)

echo.
echo ========================================
echo FINAL RESULTS
echo ========================================
echo PASSED: !PASS_COUNT! sessions
echo FAILED: !FAIL_COUNT! sessions
echo.

cd ..\..\..\..
