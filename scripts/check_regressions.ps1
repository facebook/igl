<#
.SYNOPSIS
  Compares current failures against baseline and reports NEW failing unit tests and render sessions.

.USAGE
  powershell -ExecutionPolicy Bypass -File scripts/check_regressions.ps1

.RETURNS
  Exit code 0 if no new failures; 1 if regressions detected.
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Read-ListOrEmpty($Path) {
  if (Test-Path $Path) {
    return Get-Content $Path | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' } | Sort-Object -Unique
  } else { return ,@() }
}

# Baseline files
$baselineRoot = 'artifacts/baseline'
$baselineTestsFile = Join-Path $baselineRoot 'failed_tests.txt'
$baselineSessionsFile = Join-Path $baselineRoot 'failed_sessions.txt'
$baselineTests = Read-ListOrEmpty $baselineTestsFile
$baselineSessions = Read-ListOrEmpty $baselineSessionsFile

# Current unit test failures
$currentTests = @()
$failedTestsSrc = 'artifacts/unit_tests/D3D12/failed_tests.txt'
if (Test-Path $failedTestsSrc) {
  $currentTests = Read-ListOrEmpty $failedTestsSrc
} elseif (Test-Path 'artifacts/unit_tests/D3D12/IGLTests.xml') {
  try {
    [xml]$xml = Get-Content 'artifacts/unit_tests/D3D12/IGLTests.xml'
    foreach ($suite in $xml.testsuites.testsuite) {
      foreach ($tc in $suite.testcase) {
        if ($tc.failure) { $currentTests += "$($suite.name).$($tc.name)" }
      }
    }
    $currentTests = @($currentTests | Sort-Object -Unique)
  } catch {
    $currentTests = ,@()
  }
} else {
  $currentTests = ,@()
}

# Current render session failures (from mandatory summary)
$currentSessions = ,@()
$renderSummary = 'artifacts/mandatory/render_sessions.log'
if (Test-Path $renderSummary) {
  Select-String -Path $renderSummary -Pattern '^(?i)FAIL\s+' | ForEach-Object {
    ($_.Line -replace '^(?i)FAIL\s+', '').Trim()
  } | ForEach-Object { if ($_ -ne '') { $currentSessions += $_ } }
  $currentSessions = @($currentSessions | Sort-Object -Unique)
}

function Diff-New($current, $baseline) {
  $bset = @{}; foreach ($b in $baseline) { $bset[$b.ToLower()] = $true }
  $new = @()
  foreach ($c in $current) { if (-not $bset.ContainsKey($c.ToLower())) { $new += $c } }
  return $new | Sort-Object -Unique
}

$newTests = Diff-New $currentTests $baselineTests
$newSessions = Diff-New $currentSessions $baselineSessions

Write-Host "[regressions] Baseline tests: $($baselineTests.Count) | Current tests: $($currentTests.Count) | NEW: $($newTests.Count)"
Write-Host "[regressions] Baseline sessions: $($baselineSessions.Count) | Current sessions: $($currentSessions.Count) | NEW: $($newSessions.Count)"

if ($newTests.Count -gt 0 -or $newSessions.Count -gt 0) {
  if ($newTests.Count -gt 0) {
    Write-Host "[regressions] New failing tests:" -ForegroundColor Red
    $newTests | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
  }
  if ($newSessions.Count -gt 0) {
    Write-Host "[regressions] New failing render sessions:" -ForegroundColor Red
    $newSessions | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
  }
  exit 1
} else {
  Write-Host "[regressions] No new failures detected." -ForegroundColor Green
  exit 0
}
