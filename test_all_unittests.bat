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

echo --- [%NAME%] Build ---
cmake --build build/vs --config Release --target IGLTests -j 8 >> "%LOG_DIR%\build_release.log" 2>&1
set CMD_ERROR=!ERRORLEVEL!
if not "!CMD_ERROR!"=="0" (
  echo [%NAME%] CRITICAL: Build failed ^(exit !CMD_ERROR!^), see %LOG_DIR%\build_release.log
  set /A FAIL_COUNT+=1
  set FAILURES=%FAILURES% %NAME%-BUILD-CRITICAL
  goto :eof
)
echo [%NAME%] Build OK

set DXIL_SOURCE=build\vs\src\igl\d3d12\Release\dxil.dll
set DXIL_DEST=build\vs\src\igl\tests\Release\dxil.dll
if exist "%DXIL_SOURCE%" (
  copy /Y "%DXIL_SOURCE%" "%DXIL_DEST%" >nul 2>&1
  if errorlevel 1 (
    echo [%NAME%] WARNING: Failed to copy dxil.dll to test output directory.
  ) else (
    echo [%NAME%] Copied dxil.dll to test output.
  )
) else (
  echo [%NAME%] WARNING: dxil.dll not found at %DXIL_SOURCE%.
)

set TEST_EXE=build\vs\src\igl\tests\Release\IGLTests.exe
if not exist "%TEST_EXE%" (
  echo [%NAME%] CRITICAL: Test executable not found: %TEST_EXE%
  set /A FAIL_COUNT+=1
  set FAILURES=%FAILURES% %NAME%-MISSING-EXE-CRITICAL
  goto :eof
)

echo --- [%NAME%] Run ---
"%TEST_EXE%" --gtest_color=no --gtest_brief=1 --gtest_output=xml:%LOG_DIR%\IGLTests.xml > "%LOG_DIR%\IGLTests.log" 2>&1
set TEST_EXIT=!ERRORLEVEL!
set TESTS_EXECUTED=1
if not "!TEST_EXIT!"=="0" (
  echo [%NAME%] UNIT TESTS FAILED, exit !TEST_EXIT!
  findstr /C:"[  FAILED  ]" "%LOG_DIR%\IGLTests.log" > "%LOG_DIR%\failed_tests.txt" 2>&1
  if exist "%LOG_DIR%\failed_tests.txt" (
    echo Failed tests:
    type "%LOG_DIR%\failed_tests.txt"
  )
  set /A FAIL_COUNT+=1
  set FAILURES=%FAILURES% %NAME%-TESTS
) else (
  echo [%NAME%] UNIT TESTS PASSED
  set /A PASS_COUNT+=1
)

set TEST_FAILURES=
if exist "%LOG_DIR%\IGLTests.log" (
  for /f %%I in ('findstr /C:"[  FAILED  ]" "%LOG_DIR%\IGLTests.log" ^| find /c /v ""') do set TEST_FAILURES=%%I
)
if not defined TEST_FAILURES set TEST_FAILURES=0
echo [%NAME%] Detected failing tests: !TEST_FAILURES!
for /f "delims=" %%F in ("!TEST_FAILURES!") do set /A TOTAL_FAILED_TESTS+=%%F

goto :eof
