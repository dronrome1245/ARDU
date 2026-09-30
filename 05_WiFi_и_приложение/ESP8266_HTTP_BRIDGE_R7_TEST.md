# ARDU — ESP8266 HTTP BRIDGE R7 + complete L01 profile test

Дата: 2026-09-30

## Цель

Закрыть весь оставшийся пользовательский профиль L01 одним пакетом:
- arbitrary Kelvin 1800..6500;
- RGB;
- explicit startup-profile save;
- power-cycle persistence;
- сохранить уже работающие ON/OFF, brightness, presets, OTA и Developer transport.

## OTA

1. `git pull`.
2. Открыть `esp8266_http_bridge_r7/esp8266_http_bridge_r7.ino`.
3. Локально вписать актуальные Wi-Fi/OTA credentials. Секреты не коммитить.
4. Verify.
5. Network Port `ardu 192.168.0.4` → Upload.
6. После reboot `/api/ping` → `HTTP_BRIDGE_R7`.

## Windows semantic smoke

Для JSON использовать PowerShell `Invoke-RestMethod`, чтобы не зависеть от quoting `curl.exe`:

```powershell
$base = 'http://192.168.0.4'
Invoke-RestMethod -Method Post -Uri "$base/api/light/settings" -ContentType 'application/json' -Body '{"kelvin":3500}'
Invoke-RestMethod -Method Post -Uri "$base/api/light/settings" -ContentType 'application/json' -Body '{"rgb":{"r":255,"g":0,"b":0}}'
Invoke-RestMethod -Method Post -Uri "$base/api/light/settings" -ContentType 'application/json' -Body '{"persist_startup_profile":true}'
Invoke-RestMethod -Method Get -Uri "$base/api/light/status"
```

## Android batch regression

После Sync/Run `0.5-l01-profile-r1`:
1. L01 ON.
2. Kelvin slider: 2200 → 3500 → 5200.
3. RGB: 255,0,0 → 0,255,0 → 0,0,255.
4. Вернуть Kelvin 4000, brightness 100.
5. Нажать `Сохранить как свет после включения питания`.
6. Убедиться, что UI показывает saved state / `dirty=false`.
7. Полностью снять питание ARDU, затем включить.
8. После cold boot приложение должно показать L01 ON, 4000 K, brightness 100.
9. Вернуть baseline 2700 K / brightness 64 и снова сохранить.
10. OFF→ON, Refresh, Developer PING→PONG.

## PASS

R7 PASS если весь пакет проходит без timeout/reset, power-cycle восстанавливает сохранённый профиль, а после теста baseline 2700 K / 64 снова сохранён.
