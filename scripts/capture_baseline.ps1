<#
.SYNOPSIS
  Runs the full harness and captures a baseline of failing unit tests and render sessions.

.USAGE
  powershell -ExecutionPolicy Bypass -File scripts/capture_baseline.ps1
  powershell -ExecutionPolicy Bypass -File scripts/capture_baseline.ps1 -NoRunHarness

.NOTES
  - Baseline files are written to:
      artifacts/baseline/mandatory/render_sessions.log
      artifacts/baseline/mandatory/unit_tests.log
      artifacts/baseline/failed_tests.txt
      artifacts/baseline/failed_sessions.txt
#>
param(
  [switch]$NoRunHarness
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Ensure-Dir($Path) {
  if (-not (Test-Path $Path)) { New-Item -ItemType Directory -Path $Path | Out-Null }
}

Write-Host "[baseline] Starting baseline capture..."

if (-not $NoRunHarness) {
  Write-Host "[baseline] Running combined harness: mandatory_all_tests.bat"
  cmd /c mandatory_all_tests.bat | Write-Host
}

$baselineRoot = Join-Path artifacts baseline
$baselineMandatory = Join-Path $baselineRoot mandatory
Ensure-Dir $baselineRoot
Ensure-Dir $baselineMandatory

# Copy summary logs
$srcRenderSummary = "artifacts/mandatory/render_sessions.log"
$srcUnitSummary   = "artifacts/mandatory/unit_tests.log"
if (Test-Path $srcRenderSummary) { Copy-Item $srcRenderSummary (Join-Path $baselineMandatory 'render_sessions.log') -Force }
if (Test-Path $srcUnitSummary)   { Copy-Item $srcUnitSummary   (Join-Path $baselineMandatory 'unit_tests.log') -Force }

# Build baseline failed tests list
$baselineTestsFile = Join-Path $baselineRoot 'failed_tests.txt'
$failedTestsSrc = "artifacts/unit_tests/D3D12/failed_tests.txt"
if (Test-Path $failedTestsSrc) {
  Get-Content $failedTestsSrc | Sort-Object -Unique | Set-Content $baselineTestsFile
} else {
  $xmlPath = "artifacts/unit_tests/D3D12/IGLTests.xml"
  if (Test-Path $xmlPath) {
    try {
      [xml]$xml = Get-Content $xmlPath
      $failed = @()
      foreach ($suite in $xml.testsuites.testsuite) {
        foreach ($tc in $suite.testcase) {
          if ($tc.failure) { $failed += "$($suite.name).$($tc.name)" }
        }
      }
      $failed | Sort-Object -Unique | Set-Content $baselineTestsFile
    } catch {
      Write-Warning "[baseline] Could not parse IGLTests.xml; writing empty failed_tests.txt"
      '' | Set-Content $baselineTestsFile
    }
  } else {
    Write-Warning "[baseline] No unit test failure sources found; writing empty failed_tests.txt"
    '' | Set-Content $baselineTestsFile
  }
}

# Build baseline failed render sessions list
$baselineSessionsFile = Join-Path $baselineRoot 'failed_sessions.txt'
if (Test-Path $srcRenderSummary) {
  $failedSessions = @()
  Select-String -Path $srcRenderSummary -Pattern '^(?i)FAIL\s+' | ForEach-Object {
    ($_.Line -replace '^(?i)FAIL\s+', '').Trim()
  } | ForEach-Object { if ($_ -ne '') { $failedSessions += $_ } }
  $failedSessions | Sort-Object -Unique | Set-Content $baselineSessionsFile
} else {
  Write-Warning "[baseline] Render summary not found; writing empty failed_sessions.txt"
  '' | Set-Content $baselineSessionsFile
}

# Summary
$bt = 0
if (Test-Path $baselineTestsFile) {
  $bt = (Get-Content $baselineTestsFile | Where-Object { $_.Trim() -ne '' }).Count
}
$bs = 0
if (Test-Path $baselineSessionsFile) {
  $bs = (Get-Content $baselineSessionsFile | Where-Object { $_.Trim() -ne '' }).Count
}
Write-Host "[baseline] Done. Failed tests: $bt; Failed sessions: $bs"
