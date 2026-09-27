@echo off
setlocal
set "DIR=%~dp0"
if exist "%DIR%wifi_secrets.h" (
  echo wifi_secrets.h already exists. Nothing changed.
  exit /b 0
)
copy "%DIR%wifi_secrets.example.h" "%DIR%wifi_secrets.h" >nul
if errorlevel 1 (
  echo Failed to create wifi_secrets.h
  exit /b 1
)
echo Created wifi_secrets.h
echo Edit it and replace YOUR_WIFI_SSID / YOUR_WIFI_PASSWORD.
