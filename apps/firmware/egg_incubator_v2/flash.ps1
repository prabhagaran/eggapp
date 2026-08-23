<#
.SYNOPSIS
Build and flash egg_incubator_v2 to one specific board.

.DESCRIPTION
Regenerates secrets.h from secrets.d/<device>.h, then compiles and uploads.

secrets.h is a build artifact here, never something to hand-edit: it is
overwritten on every run, so whatever was left in it by the previous flash
cannot leak into this one. That is the whole point. Two boards were once
flashed as INCUBATOR_01 by editing it manually and forgetting to revert —
identical MQTT client ids, so each connection kicked the other off the broker.

.PARAMETER Device
Board profile name, matching a file in secrets.d/ (e.g. incubator_01).

.PARAMETER Port
Serial port to upload to (e.g. COM3). Omit to build without uploading.

.EXAMPLE
./flash.ps1 -Device incubator_01 -Port COM3

.EXAMPLE
./flash.ps1 -Device incubator_02        # compile only, no board attached
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Device,
    [Parameter(Mandatory = $false)][string]$Port
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$fqbn = "esp32:esp32:esp32:PartitionScheme=min_spiffs"

$profilePath = Join-Path $here "secrets.d/$Device.h"
if (-not (Test-Path $profilePath)) {
    Write-Host "No profile 'secrets.d/$Device.h'." -ForegroundColor Red
    Write-Host "Available:" -ForegroundColor Yellow
    Get-ChildItem (Join-Path $here "secrets.d") -Filter *.h -ErrorAction SilentlyContinue |
        ForEach-Object { "  $($_.BaseName)" }
    exit 1
}

# Read the identity out of the profile purely so it can be echoed back and,
# after upload, checked against what the board actually reports. A flash that
# silently lands the wrong identity is the failure mode worth catching.
$declaredId = (Select-String -Path $profilePath -Pattern '^\s*#define\s+DEVICE_ID\s+"([^"]+)"' |
    Select-Object -First 1).Matches.Groups[1].Value
if (-not $declaredId) {
    Write-Host "Profile '$Device' defines no DEVICE_ID." -ForegroundColor Red
    exit 1
}

Write-Host "Device : $declaredId  (secrets.d/$Device.h)" -ForegroundColor Cyan
if ($Port) { Write-Host "Port   : $Port" -ForegroundColor Cyan }

# Generated, not authored — regenerated every run so stale identity cannot leak.
$banner = @"
// GENERATED FILE - DO NOT EDIT.
// Written by flash.ps1 from secrets.d/$Device.h on $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss').
// Any hand-edits are lost on the next flash. Edit secrets.d/$Device.h instead.

"@
Set-Content -Path (Join-Path $here "secrets.h") -Value ($banner + (Get-Content $profilePath -Raw)) -Encoding utf8

Write-Host "`nCompiling..." -ForegroundColor Cyan
arduino-cli compile --fqbn $fqbn $here
if ($LASTEXITCODE -ne 0) { Write-Host "Compile failed." -ForegroundColor Red; exit 1 }

if (-not $Port) {
    Write-Host "`nCompiled. No -Port given, so nothing was uploaded." -ForegroundColor Green
    exit 0
}

Write-Host "`nUploading to $Port..." -ForegroundColor Cyan
arduino-cli upload --fqbn $fqbn --port $Port $here
if ($LASTEXITCODE -ne 0) { Write-Host "Upload failed." -ForegroundColor Red; exit 1 }

# Confirm the board reports the identity we intended, rather than trusting that
# the right profile reached the right port. Best-effort: a busy or unreadable
# port is reported, not treated as failure.
Write-Host "`nVerifying reported identity..." -ForegroundColor Cyan
Start-Sleep -Seconds 3
try {
    $sp = New-Object System.IO.Ports.SerialPort
    $sp.PortName = $Port; $sp.BaudRate = 115200
    $sp.Parity = [System.IO.Ports.Parity]::None; $sp.DataBits = 8
    $sp.StopBits = [System.IO.Ports.StopBits]::One
    $sp.Handshake = [System.IO.Ports.Handshake]::None
    $sp.Encoding = [System.Text.Encoding]::ASCII
    $sp.ReadTimeout = 500
    $sp.Open(); $sp.DiscardInBuffer()
    $sb = New-Object System.Text.StringBuilder
    $deadline = (Get-Date).AddSeconds(20)
    while ((Get-Date) -lt $deadline) {
        try { $c = $sp.ReadExisting(); if ($c) { [void]$sb.Append($c) } } catch {}
        if ($sb.ToString() -match [regex]::Escape($declaredId)) { break }
        Start-Sleep -Milliseconds 200
    }
    $sp.Close()

    $seen = [regex]::Matches($sb.ToString(), 'INCUBATOR_[A-Z0-9_]+') |
        ForEach-Object { $_.Value } | Select-Object -Unique
    if ($seen -contains $declaredId) {
        Write-Host "OK: board reports $declaredId" -ForegroundColor Green
    } elseif ($seen) {
        Write-Host "MISMATCH: expected $declaredId, board reports $($seen -join ', ')" -ForegroundColor Red
        exit 1
    } else {
        Write-Host "Could not read an id from serial (board may still be booting)." -ForegroundColor Yellow
    }
} catch {
    Write-Host "Could not open $Port to verify: $($_.Exception.Message)" -ForegroundColor Yellow
}
