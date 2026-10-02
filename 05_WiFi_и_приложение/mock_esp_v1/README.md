# ARDU ESP v1 mock

Stateful development server for Android testing while physical Nano/ESP release upload is unavailable.

## Windows

From PowerShell:

```powershell
cd C:\ARDU
python .\05_WiFi_и_приложение\mock_esp_v1\mock_esp_v1.py
```

Expected:

```text
ARDU ESP v1 mock listening on http://0.0.0.0:8080
On the phone enter: <THIS_PC_LAN_IP>:8080
```

Find the PC IPv4 address:

```powershell
ipconfig
```

Use the IPv4 address of the adapter connected to the same home Wi-Fi/LAN as the phone, for example:

```text
192.168.0.10:8080
```

In Android ARDU:

`Настройки → Адрес ARDU → <PC_IP>:8080 → Сохранить адрес и переподключиться`.

Windows Firewall may ask whether Python can accept private-network connections. For this LAN-only test, allow it on the private home network.

## What is simulated

- `/api/ping`, status, settings and RTC;
- Light state/profile/clap;
- 9-pair clap calibration flow;
- all Music mode state/settings;
- microphone calibration result;
- Ambient F01/F02/F03;
- Night settings/schedule;
- Alarm/Dawn settings;
- current limit;
- async event list;
- numeric Developer UART examples.

The mock is for Android UI/API verification only. It does not simulate LED timing, MAX9814/FHT, RTC hardware or electrical behavior.

## Stop

Press `Ctrl+C` in the PowerShell window.

## CI

`Mock ESP Verify` checks:
- Python syntax;
- `GET /api/ping`;
- `GET /api/settings`;
- a stateful Music POST followed by settings reread.
