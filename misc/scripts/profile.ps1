param(
    [Parameter(Mandatory)]
    [string] $Target,

    [Parameter(ValueFromRemainingArguments)]
    [string[]] $BuildArguments
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ModeArguments = switch ($Target) {
    "--build" { @("fast_unsafe=yes") }
    "--build-safe" { @("fast_unsafe=no") }
    default {
        [Console]::Error.WriteLine("Unknown profile target '$Target'. Expected --build or --build-safe.")
        exit 2
    }
}

$BuildArguments = @($BuildArguments | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
$EffectiveBuildArguments = @($ModeArguments) + $BuildArguments
$timings = @{}
$timingPattern = '^Total (?<name>build|SConscript file execution|SCons execution|command execution) time: (?<seconds>[0-9.]+) seconds$'
$sconscriptPattern = '^SConscript:.+ took [0-9.]+ ms$'
$sconsignPattern = '^Total SConsign sync time: [0-9.]+ seconds$'
$sconsArguments = @("platform=windows", "target=editor", "arch=x86_64", "--debug=time") + $EffectiveBuildArguments

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
& uv tool run scons @sconsArguments 2>&1 |
    ForEach-Object {
        $line = $_.ToString()
        if ($line -match $timingPattern) {
            $timings[$Matches.name] = [double] $Matches.seconds
        } elseif ($line -notmatch $sconscriptPattern -and $line -notmatch $sconsignPattern) {
            Write-Output $line
        }
    }
$buildExitCode = $LASTEXITCODE
$stopwatch.Stop()

$requiredTimings = @("build", "SConscript file execution", "SCons execution", "command execution")
$missingTimings = @($requiredTimings | Where-Object { -not $timings.ContainsKey($_) })
if ($missingTimings.Count -gt 0) {
    [Console]::Error.WriteLine("SCons did not report timing data for: $($missingTimings -join ', ').")
    exit $(if ($buildExitCode -eq 0) { 1 } else { $buildExitCode })
}

$total = $stopwatch.Elapsed.TotalSeconds
$launcher = [Math]::Max(0.0, $total - $timings["build"])
$rows = @(
    [pscustomobject]@{ Phase = "Total"; Seconds = $total; Percent = 100.0 }
    [pscustomobject]@{ Phase = "Configuration"; Seconds = $timings["SConscript file execution"]; Percent = 100.0 * $timings["SConscript file execution"] / $total }
    [pscustomobject]@{ Phase = "SCons overhead"; Seconds = $timings["SCons execution"]; Percent = 100.0 * $timings["SCons execution"] / $total }
    [pscustomobject]@{ Phase = "Commands"; Seconds = $timings["command execution"]; Percent = 100.0 * $timings["command execution"] / $total }
    [pscustomobject]@{ Phase = "Launcher"; Seconds = $launcher; Percent = 100.0 * $launcher / $total }
)
$primaryCost = $rows | Select-Object -Skip 1 | Sort-Object Seconds -Descending | Select-Object -First 1

Write-Output ""
$profileName = "Build profile ($($EffectiveBuildArguments -join ' '))"
Write-Output $profileName
Write-Output ("{0,-18} {1,10} {2,9}" -f "Phase", "Time", "Share")
Write-Output ("{0,-18} {1,10} {2,9}" -f "------------------", "----------", "---------")
foreach ($row in $rows) {
    Write-Output ("{0,-18} {1,9:N2}s {2,8:N1}%" -f $row.Phase, $row.Seconds, $row.Percent)
}
Write-Output ""
Write-Output ("Primary cost: {0} ({1:N1}%)" -f $primaryCost.Phase, $primaryCost.Percent)

exit $buildExitCode
