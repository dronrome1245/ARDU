param(
    [Parameter(Mandatory=$true)]
    [string]$HexPath,

    [string]$DeviceIp = "192.168.0.4",
    [string]$OtaPassword,
    [string]$AvrdudePath,
    [string]$AvrdudeConfPath
)

$ErrorActionPreference = "Stop"

function Find-Avrdude {
    $root = Join-Path $env:LOCALAPPDATA "Arduino15\packages\arduino\tools\avrdude"
    if (-not (Test-Path $root)) {
        throw "Arduino AVRDUDE folder not found: $root"
    }
    $exe = Get-ChildItem -Path $root -Recurse -Filter avrdude.exe -File |
        Sort-Object FullName -Descending |
        Select-Object -First 1
    if (-not $exe) { throw "avrdude.exe not found under $root" }
    return $exe.FullName
}

$hex = (Resolve-Path $HexPath).Path
if (-not $hex.ToLowerInvariant().EndsWith(".hex")) {
    throw "Expected an Arduino Intel HEX file (*.hex): $hex"
}

if (-not $AvrdudePath) { $AvrdudePath = Find-Avrdude }
$AvrdudePath = (Resolve-Path $AvrdudePath).Path

if (-not $AvrdudeConfPath) {
    $binDir = Split-Path $AvrdudePath -Parent
    $toolRoot = Split-Path $binDir -Parent
    $AvrdudeConfPath = Join-Path $toolRoot "etc\avrdude.conf"
}
$AvrdudeConfPath = (Resolve-Path $AvrdudeConfPath).Path

if (-not $OtaPassword) {
    $secure = Read-Host "Current ARDU ESP OTA password" -AsSecureString
    $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    try {
        $OtaPassword = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
    } finally {
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
    }
}

Write-Host "Checking ARDU ESP V4 remote programmer..."
$ping = Invoke-RestMethod "http://$DeviceIp/api/ping" -TimeoutSec 5
if (-not $ping.ok -or -not $ping.nano_remote_flash) {
    throw "ESP at $DeviceIp does not report nano_remote_flash=true"
}

$payload = @{ password = $OtaPassword } | ConvertTo-Json -Compress
$arm = Invoke-RestMethod -Uri "http://$DeviceIp/api/system/nano-program/arm" `
    -Method Post `
    -ContentType "application/json" `
    -Body $payload `
    -TimeoutSec 5

# Do not keep the plaintext password around longer than needed.
$OtaPassword = $null
$payload = $null

if (-not $arm.ok -or -not $arm.armed) {
    throw "ESP did not arm Nano programming mode"
}

$netPort = "net:$DeviceIp`:$($arm.port)"
Write-Host "Programming Nano through $netPort at $($arm.baud) baud..."
Write-Host "HEX: $hex"

$args = @(
    "-C$AvrdudeConfPath",
    "-v",
    "-p", "atmega328p",
    "-c", "arduino",
    "-P", $netPort,
    "-b", "$($arm.baud)",
    "-D",
    "-U", "flash:w:$hex`:i"
)

& $AvrdudePath @args
$code = $LASTEXITCODE
if ($code -ne 0) {
    throw "AVRDUDE failed with exit code $code. The ESP reset bridge remains the recovery path; arm and retry."
}

Write-Host "AVRDUDE write + verify completed. Waiting for ARDU UART to return..."
$deadline = (Get-Date).AddSeconds(12)
$status = $null
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 600
    try {
        $status = Invoke-RestMethod "http://$DeviceIp/api/status" -TimeoutSec 3
        if ($status.ok) { break }
    } catch {
        # Old Nano bootloader can take a few seconds to fall through after reset.
    }
}

if ($status -and $status.ok) {
    Write-Host "PASS: Nano is responding again. mode=$($status.mode), rtc_valid=$($status.rtc_valid)"
} else {
    Write-Warning "Programming verified, but /api/status did not return within 12 s. Check /api/system/nano-program/status and retry the Nano reset/program cycle if needed."
}
