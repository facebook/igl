@echo off
setlocal enableextensions enabledelayedexpansion

set LOG_ROOT=artifacts\unit_tests
if not exist "%LOG_ROOT%" mkdir "%LOG_ROOT%"

set PASS_COUNT=0
set FAIL_COUNT=0
set TOTAL_FAILED_TESTS=0
set TESTS_EXECUTED=0
set FAILURES=

call :run_d3d12

echo.
echo ========================================
echo UNIT TEST SUMMARY
echo ========================================
echo PASSED: %PASS_COUNT%
echo FAILED: %FAIL_COUNT%
if not "%FAILURES%"=="" (
  echo Failed targets:%FAILURES%
)
if %TESTS_EXECUTED% gtr 0 (
  echo Total failing tests detected: %TOTAL_FAILED_TESTS%
) else (
  echo Total failing tests detected: N/A ^(tests did not run^)
)
echo Logs stored under %LOG_ROOT%
echo.

if %FAIL_COUNT% gtr 0 (
  endlocal & exit /b 1
) else (
  endlocal & exit /b 0
)

:run_d3d12
set NAME=D3D12
set LOG_DIR=%LOG_ROOT%\%NAME%
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

echo.
echo --- [%NAME%] Configure ---
cmake -S . -B build/vs -G "Visual Studio 17 2022" -A x64 -DIGL_WITH_D3D12=ON -DIGL_WITH_OPENGL=OFF -DIGL_WITH_VULKAN=OFF -DIGL_WITH_TESTS=ON >> "%LOG_DIR%\configure.log" 2>&1
set CMD_ERROR=!ERRORLEVEL!
if not "!CMD_ERROR!"=="0" (
  echo [%NAME%] CRITICAL: Configure failed ^(exit !CMD_ERROR!^), see %LOG_DIR%\configure.log
  set /A FAIL_COUNT+=1
  set FAILURES=%FAILURES% %NAME%-CONFIGURE-CRITICAL
  goto :eof
)
echo [%NAME%] Configure OK

rem Test both Debug and Release configurations
for %%C in (Debug Release) do (
  echo.
  echo --- [%NAME%] Build %%C ---
  cmake --build build/vs --config %%C --target IGLTests -j 8 >> "%LOG_DIR%\build_%%C.log" 2>&1
  set CMD_ERROR=!ERRORLEVEL!
  if not "!CMD_ERROR!"=="0" (
    echo [%NAME%] CRITICAL: %%C build failed ^(exit !CMD_ERROR!^), see %LOG_DIR%\build_%%C.log
    set /A FAIL_COUNT+=1
    set FAILURES=%FAILURES% %NAME%-BUILD-%%C-CRITICAL
    goto :eof
  )
  echo [%NAME%] %%C build OK

  set DXIL_SOURCE=build\vs\src\igl\d3d12\%%C\dxil.dll
  set DXIL_DEST=build\vs\src\igl\tests\%%C\dxil.dll
  if exist "!DXIL_SOURCE!" (
    copy /Y "!DXIL_SOURCE!" "!DXIL_DEST!" >nul 2>&1
    if errorlevel 1 (
      echo [%NAME%] WARNING: Failed to copy dxil.dll to %%C test output directory.
    ) else (
      echo [%NAME%] Copied dxil.dll to %%C test output.
    )
  ) else (
    echo [%NAME%] WARNING: dxil.dll not found at !DXIL_SOURCE!.
  )

  set TEST_EXE=build\vs\src\igl\tests\%%C\IGLTests.exe
  if not exist "!TEST_EXE!" (
    echo [%NAME%] CRITICAL: %%C test executable not found: !TEST_EXE!
    set /A FAIL_COUNT+=1
    set FAILURES=%FAILURES% %NAME%-MISSING-EXE-%%C-CRITICAL
    goto :eof
  )

  echo --- [%NAME%] Run %%C ---
  "!TEST_EXE!" --gtest_color=no --gtest_brief=1 --gtest_output=xml:%LOG_DIR%\IGLTests_%%C.xml > "%LOG_DIR%\IGLTests_%%C.log" 2>&1
  set TEST_EXIT=!ERRORLEVEL!
  set TESTS_EXECUTED=1
  if not "!TEST_EXIT!"=="0" (
    echo [%NAME%] %%C UNIT TESTS FAILED, exit !TEST_EXIT!
    findstr /C:"[  FAILED  ]" "%LOG_DIR%\IGLTests_%%C.log" > "%LOG_DIR%\failed_tests_%%C.txt" 2>&1
    if exist "%LOG_DIR%\failed_tests_%%C.txt" (
      echo Failed tests:
      type "%LOG_DIR%\failed_tests_%%C.txt"
    )
    set /A FAIL_COUNT+=1
    set FAILURES=%FAILURES% %NAME%-TESTS-%%C
  ) else (
    echo [%NAME%] %%C UNIT TESTS PASSED
    set /A PASS_COUNT+=1
  )

  set TEST_FAILURES=0
  if exist "%LOG_DIR%\IGLTests_%%C.log" (
    findstr /C:"[  FAILED  ]" "%LOG_DIR%\IGLTests_%%C.log" | find /c /v "" > "%LOG_DIR%\temp_count_%%C.txt" 2>nul
    if exist "%LOG_DIR%\temp_count_%%C.txt" (
      set /p TEST_FAILURES=<"%LOG_DIR%\temp_count_%%C.txt"
      del "%LOG_DIR%\temp_count_%%C.txt"
    )
  )
  echo [%NAME%] %%C detected failing tests: !TEST_FAILURES!
  set /A TOTAL_FAILED_TESTS+=!TEST_FAILURES!
)

goto :eof
