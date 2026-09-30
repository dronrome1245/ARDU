# ARDU — ESP8266 HTTP BRIDGE R6 + Android L01 brightness/Kelvin test

Дата: 2026-09-30

## Цель

Расширить уже hardware-tested semantic L01 ON/OFF одним слоем:
- brightness `0..255`;
- Kelvin presets `2700/4000/6000`;
- без RGB, произвольного Kelvin slider и persistence.

## Перед OTA

1. `git pull`.
2. Открыть `esp8266_http_bridge_r6/esp8266_http_bridge_r6.ino`.
3. Локально скопировать те же `WIFI_SSID`, `WIFI_PASSWORD`, `OTA_PASSWORD`, которые использует рабочий R5. Не коммитить секреты.
4. Board: Generic ESP8266 Module; flash: фактические 4 MB.
5. Выполнить Verify.

## OTA R6

1. Tools → Port → `ardu 192.168.0.4`.
2. Upload через ArduinoOTA; проводку не менять.
3. После reboot:
   - `GET /api/ping` → `fw=HTTP_BRIDGE_R6`;
   - `/api/status`, `/api/time`, Developer PING остаются рабочими.

## Semantic API smoke

Включить L01 перед визуальными изменениями.

```powershell
curl.exe -X POST http://192.168.0.4/api/light/settings -H "Content-Type: application/json" --data-binary "{\"brightness\":128}"
curl.exe -X POST http://192.168.0.4/api/light/settings -H "Content-Type: application/json" --data-binary "{\"kelvin\":4000}"
curl.exe http://192.168.0.4/api/light/status
```

PASS:
- brightness response: `ok=true`, `brightness=128`, `applied=true`;
- Kelvin response: `ok=true`, `kelvin=4000`, `color_mode=kelvin`, `applied=true`;
- readback показывает brightness 128 и Kelvin 4000.

## Android batch test

После `git pull`/Sync/Run:
1. верх: `Онлайн • HTTP_BRIDGE_R6`;
2. L01 ON;
3. brightness 32 → 128 → 220;
4. presets 2700 → 4000 → 6000;
5. вернуть 2700 / 64;
6. Refresh;
7. ON→OFF→ON;
8. Developer PING→PONG.

## PASS

R6 stage PASS если OTA, direct semantic API и Android batch test проходят без timeout/reset и фактический state после каждого write перечитывается из Nano.
