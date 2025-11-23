@echo off
setlocal enableextensions enabledelayedexpansion

set LOG_DIR=artifacts\mandatory
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

rem Enable D3D12 debug validation and capture for all sub-suites.
set IGL_D3D12_DEBUG=1
set IGL_D3D12_GPU_VALIDATION=1
set IGL_D3D12_DRED=1
set IGL_DXGI_DEBUG=1
set IGL_D3D12_CAPTURE_VALIDATION=1

echo Running render session suite...
call test_all_sessions.bat > "%LOG_DIR%\render_sessions.log" 2>&1
set SESS_RESULT=%ERRORLEVEL%

echo Running unit test suite...
call test_all_unittests.bat > "%LOG_DIR%\unit_tests.log" 2>&1
set UNIT_RESULT=%ERRORLEVEL%

echo.
echo ========================================
echo COMBINED TEST SUMMARY
echo ========================================
if %SESS_RESULT%==0 (
  echo Render Sessions: PASS
) else (
  echo Render Sessions: FAIL (see %LOG_DIR%\render_sessions.log)
)
if %UNIT_RESULT%==0 (
  echo Unit Tests: PASS
) else (
  echo Unit Tests: FAIL (see %LOG_DIR%\unit_tests.log)
)
echo Logs located in %LOG_DIR%
echo.

set EXIT_CODE=0
if %SESS_RESULT% neq 0 set EXIT_CODE=1
if %UNIT_RESULT% neq 0 set EXIT_CODE=1

endlocal & exit /b %EXIT_CODE%
