param()

$ErrorActionPreference = 'Stop'
Write-Output "Detecting ESP32 serial port..."
$cliOutput = & arduino-cli board list 2>&1
Write-Output "arduino-cli output:\n$cliOutput"
$port = $null
# Allow environment override (useful for forcing COM3)
if ($env:ESP_PORT) {
  $port = $env:ESP_PORT
  Write-Output "Using override port from ESP_PORT=$port"
} elseif ($cliOutput -match 'COM3') {
  # prefer COM3 if present (user requested)
  $port = 'COM3'
  Write-Output "Preferring COM3 since it appears in board list"
} else {
  $m = [regex]::Match($cliOutput, 'COM\d+')
  if ($m.Success) { $port = $m.Value }
  else {
    $m = [regex]::Match($cliOutput, '/dev/tty\S+')
    if ($m.Success) { $port = $m.Value }
  }
}
if (-not $port) {
  Write-Error 'No serial port detected by arduino-cli. See output above.'
  exit 1
}
Write-Output "Detected port: $port"

Push-Location -Path "${PSScriptRoot}/..\.." | Out-Null
Set-Location -Path "${PSScriptRoot}/..\..\egg_incubator_v2"

Write-Output "Compiling (fqbn: esp32:esp32:esp32)..."
arduino-cli compile --fqbn esp32:esp32:esp32 .
if ($LASTEXITCODE -ne 0) { Write-Error 'Compile failed'; exit $LASTEXITCODE }

Write-Output "Uploading to $port..."
arduino-cli upload -p $port --fqbn esp32:esp32:esp32 .
if ($LASTEXITCODE -ne 0) { Write-Error 'Upload failed'; exit $LASTEXITCODE }

Write-Output "Opening serial monitor (baud=115200)..."
arduino-cli monitor -p $port -c baudrate=115200

Pop-Location | Out-Null
