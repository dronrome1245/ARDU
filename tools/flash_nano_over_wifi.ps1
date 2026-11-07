param(
    [Parameter(Mandatory=$true)]
    [string]$HexPath,

    [string]$DeviceIp = "192.168.0.4",
    [string]$OtaPassword,
    [string]$AvrdudePath,
    [string]$AvrdudeConfPath,

    # Read-only diagnostic mode: compare installed flash with a known HEX.
    [switch]$VerifyOnly
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
if ($VerifyOnly -and $ping.fw -ne "ARDU_ESP_V4_NANO_REMOTE_R3_DIAG") {
    Write-Warning "ESP fw=$($ping.fw): R3 diagnostics unavailable until its OTA update."
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
if ($VerifyOnly) {
    Write-Host "VERIFY-ONLY: read Nano flash without writing; Nano will be reset."
    $flashOp = "flash:v:$hex" + ":i"
} else {
    Write-Host "WRITE + VERIFY: Nano flash will be overwritten and Nano reset."
    $flashOp = "flash:w:$hex" + ":i"
}
Write-Host "Bridge: $netPort at $($arm.baud) baud"
Write-Host "HEX: $hex"

$args = @(
    "-C$AvrdudeConfPath",
    "-v",
    "-p", "atmega328p",
    "-c", "arduino",
    "-P", $netPort,
    "-b", "$($arm.baud)",
    "-D",
    "-U", $flashOp
)

& $AvrdudePath @args
$code = $LASTEXITCODE

# Collect bridge diagnostics also on AVRDUDE failure. Failed verify is never
# a PASS, even if the Nano application continues to boot.
$diag = $null
for ($attempt = 0; $attempt -lt 8; $attempt++) {
    Start-Sleep -Milliseconds 350
    try {
        $diag = Invoke-RestMethod "http://$DeviceIp/api/system/nano-program/status" -TimeoutSec 3
        if (-not $diag.active) { break }
    } catch {
        # Wait for ESP to restore HTTP service after programming mode.
    }
}
if ($diag) {
    Write-Host "Bridge last_result=$($diag.last_result); active=$($diag.active)"
    if ($null -ne $diag.uart_overrun) {
        Write-Host "UART overrun=$($diag.uart_overrun); rx_error=$($diag.uart_rx_error); RX buffer=$($diag.uart_rx_buffer_size)"
        Write-Host "Bytes TCP->UART=$($diag.tcp_to_uart_bytes); UART->TCP=$($diag.uart_to_tcp_bytes)"
        Write-Host "Max available TCP=$($diag.max_tcp_available); UART=$($diag.max_uart_available)"
    } else {
        Write-Warning "ESP does not expose R3 diagnostic counters yet."
    }
} else {
    Write-Warning "Could not read bridge diagnostics after the session."
}

if ($code -ne 0) {
    throw "AVRDUDE failed (exit code $code). NO PASS: save its output and bridge diagnostics. Do not blindly repeat a write."
}

if ($VerifyOnly) {
    Write-Host "PASS: byte-for-byte AVRDUDE verify succeeded; no flash write performed."
} else {
    Write-Host "PASS: AVRDUDE write + byte-for-byte verify succeeded."
}

# Confirm the application protocol is running after the bootloader session.
$deadline = (Get-Date).AddSeconds(12)
$status = $null
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 600
    try {
        $status = Invoke-RestMethod "http://$DeviceIp/api/status" -TimeoutSec 3
        if ($status.ok) { break }
    } catch {
        # Nano bootloader may take a moment to launch the application.
    }
}

if ($status -and $status.ok) {
    Write-Host "PASS: Nano application responds. mode=$($status.mode), rtc_valid=$($status.rtc_valid), current_limit_ma=$($status.current_limit_ma)"
} else {
    throw "AVRDUDE verified flash, but /api/status did not recover in 12 s. Application acceptance is NOT PASS."
}
