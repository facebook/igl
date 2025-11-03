param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
    [Parameter(Mandatory = $true)][long]$ThresholdBytes,
    [Parameter(Mandatory = $true)][string]$LogFile,
    [Parameter(Mandatory = $true)][string]$MemFile
)

if (Test-Path $LogFile) { Remove-Item $LogFile -Force }
if (Test-Path $MemFile) { Remove-Item $MemFile -Force }
if (Test-Path variable:\script:exitCode) {
    Remove-Variable -Name exitCode -Scope Script -ErrorAction SilentlyContinue
}
$script:exitCode = $null

$logWriter = $null
$process = $null
$handlersRegistered = $false
$samples = New-Object System.Collections.Generic.List[int64]

try {
    $logDir = Split-Path -Path $LogFile -Parent
    if ($logDir -and -not (Test-Path $logDir)) {
        New-Item -ItemType Directory -Path $logDir | Out-Null
    }

    $logWriter = [System.IO.StreamWriter]::new($LogFile, $false, [System.Text.Encoding]::UTF8)

    $resolvedExe = $Exe
    try {
        $resolvedExe = (Resolve-Path -LiteralPath $Exe -ErrorAction Stop).ProviderPath
    } catch {
        # keep original path; Start-Process will surface error
    }

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $resolvedExe
    $startInfo.WorkingDirectory = Split-Path -Path $resolvedExe -Parent
    if ([string]::IsNullOrEmpty($startInfo.WorkingDirectory)) {
        $startInfo.WorkingDirectory = (Get-Location).Path
    }
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.CreateNoWindow = $true

    $exeNameWithoutExt = [System.IO.Path]::GetFileNameWithoutExtension($resolvedExe)
    if ([string]::IsNullOrEmpty($exeNameWithoutExt)) {
        $exeNameWithoutExt = [System.IO.Path]::GetFileNameWithoutExtension($Exe)
    }

    $exeImage = "$exeNameWithoutExt.exe"
    try {
        $null = Start-Process -FilePath "taskkill.exe" -ArgumentList "/F","/T","/IM",$exeImage -NoNewWindow -Wait -ErrorAction SilentlyContinue
    } catch {
        if ($logWriter) { $logWriter.WriteLine("Warning: pre-run taskkill failed: $_") }
    }

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    $process.EnableRaisingEvents = $true

    $script:logWriter = $logWriter
    $outputHandler = [System.Diagnostics.DataReceivedEventHandler]{
        param($sender, $args)
        if ($args.Data) {
            $script:logWriter.WriteLine($args.Data)
        }
    }
    $errorHandler = [System.Diagnostics.DataReceivedEventHandler]{
        param($sender, $args)
        if ($args.Data) {
            $script:logWriter.WriteLine($args.Data)
        }
    }

    $launchFailed = $false
    try {
        if (-not $process.Start()) {
            throw "Failed to start process."
        }
    } catch {
        $launchFailed = $true
        $launchException = $_
    }

    if ($launchFailed) {
        if ($logWriter) {
            $logWriter.WriteLine("Failed to launch process: $Exe")
            if ($launchException) {
                $logWriter.WriteLine($launchException)
            }
            $logWriter.Flush()
            $logWriter.Dispose()
            $logWriter = $null
        }
        $script:exitCode = 10
        throw [System.Exception]::new("LaunchFailed")
    }

    $process.add_OutputDataReceived($outputHandler)
    $process.add_ErrorDataReceived($errorHandler)
    $process.BeginOutputReadLine()
    $process.BeginErrorReadLine()
    $handlersRegistered = $true

    $pushedLocation = $false
    if (-not [string]::IsNullOrEmpty($startInfo.WorkingDirectory)) {
        try {
            Push-Location $startInfo.WorkingDirectory
            $pushedLocation = $true
        } catch {
            $logWriter.WriteLine("Warning: failed to set working directory '$($startInfo.WorkingDirectory)': $_")
        }
    }

    try {
        $process.Refresh()
        $samples.Add($process.WorkingSet64)
    } catch {
        if ($logWriter) { $logWriter.WriteLine("Warning: failed to sample memory immediately after launch: $_") }
    }

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($stopwatch.Elapsed.TotalSeconds -lt $TimeoutSeconds -and -not $process.HasExited) {
        Start-Sleep -Seconds 5
        try {
            $process.Refresh()
            $samples.Add($process.WorkingSet64)
        } catch {
            if ($logWriter) { $logWriter.WriteLine("Warning: failed to sample memory: $_") }
        }
    }

    if (-not $process.HasExited) {
        $logWriter.WriteLine("Timeout reached ($TimeoutSeconds seconds). Requesting graceful shutdown.")
        try {
            if ($process.CloseMainWindow()) {
                Start-Sleep -Seconds 2
            }
        } catch {
            $logWriter.WriteLine("Warning: CloseMainWindow failed: $_")
        }

        $forceTermination = $false
        if (-not $process.HasExited) {
            $logWriter.WriteLine("Forcing process termination.")
            try {
                $null = Start-Process -FilePath "taskkill.exe" -ArgumentList "/F","/T","/PID",$process.Id -NoNewWindow -Wait -ErrorAction Stop
                $forceTermination = $true
            } catch {
                $logWriter.WriteLine("Error: taskkill failed: $_")
            }
        }

        try {
            if (-not $process.WaitForExit(5000)) {
                $logWriter.WriteLine("Error: process still alive after forced termination.")
                $script:exitCode = 11
            }
        } catch {
            $logWriter.WriteLine("Warning: WaitForExit after termination failed: $_")
        }
        if ($forceTermination -or $script:exitCode -eq 11) {
            try {
                try {
                    $null = Start-Process -FilePath "taskkill.exe" -ArgumentList "/F","/T","/IM",$exeImage -NoNewWindow -Wait -ErrorAction SilentlyContinue
                } catch {
                    $logWriter.WriteLine("Warning: post-run taskkill failed: $_")
                }
            } catch {
                $logWriter.WriteLine("Warning: linger cleanup failed: $_")
            }
        }
    } else {
        try {
            $process.WaitForExit()
        } catch {
            $logWriter.WriteLine("Warning: WaitForExit failed: $_")
        }
    }

    if ($samples.Count -eq 0) {
        try {
            $process.Refresh()
            $samples.Add($process.WorkingSet64)
        } catch {
            if ($logWriter) { $logWriter.WriteLine("Warning: unable to record memory sample: $_") }
            $samples.Add(0)
        }
    }

    $logWriter.Flush()
    $logWriter.Dispose()
    $logWriter = $null
} catch {
    if ($logWriter) {
        $logWriter.WriteLine("Unhandled error: $_")
        $logWriter.Flush()
    }
    if (-not $script:exitCode) {
        $script:exitCode = 1
    }
} finally {
    if ($process -and $handlersRegistered) {
        try { $process.CancelOutputRead() } catch { }
        try { $process.CancelErrorRead() } catch { }
        try { $process.remove_OutputDataReceived($outputHandler) } catch { }
        try { $process.remove_ErrorDataReceived($errorHandler) } catch { }
    }
    if ($logWriter) {
        $logWriter.Flush()
        $logWriter.Dispose()
        $logWriter = $null
    }
    if ($process) {
        $process.Dispose()
    }
    if ($pushedLocation) {
        try { Pop-Location } catch { }
    }
}

$exitCode = 0
if ($script:exitCode -ne $null) {
    $exitCode = $script:exitCode
} elseif ($process -eq $null) {
    $exitCode = 10
} elseif (-not $process.HasExited) {
    $exitCode = 11
}

if ($exitCode -eq 0) {
    $min = ($samples | Measure-Object -Minimum).Minimum
    $max = ($samples | Measure-Object -Maximum).Maximum
    if ($null -eq $min) { $min = 0 }
    if ($null -eq $max) { $max = 0 }
    $diff = $max - $min
    $diffMB = [Math]::Round($diff / 1MB, 2)
    "$min $max $diff $diffMB" | Out-File -FilePath $MemFile -Encoding ASCII

    if ($diff -gt $ThresholdBytes) {
        $exitCode = 2
    }
}

exit $exitCode
