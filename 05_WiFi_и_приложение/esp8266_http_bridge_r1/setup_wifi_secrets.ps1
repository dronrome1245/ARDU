$ErrorActionPreference = "Stop"

$dir = Split-Path -Parent $MyInvocation.MyCommand.Path
$example = Join-Path $dir "wifi_secrets.example.h"
$target = Join-Path $dir "wifi_secrets.h"

if (-not (Test-Path $example)) {
    Write-Error "wifi_secrets.example.h not found"
    exit 1
}

if (Test-Path $target) {
    Write-Host "wifi_secrets.h already exists. Nothing changed."
    exit 0
}

Copy-Item $example $target
Write-Host "Created wifi_secrets.h"
Write-Host "Edit it and replace YOUR_WIFI_SSID / YOUR_WIFI_PASSWORD."
