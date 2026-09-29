# ARDU — ESP8266 HTTP BRIDGE R2 / L01 stage 1 test

Дата: 2026-09-30

## Назначение

R2 сохраняет весь уже пройденный transport R1 и добавляет первый пользовательский semantic слой L01.

Новые маршруты:
- `GET /api/light/status`;
- `POST /api/light/settings` — в этой итерации только `{"enabled":true|false}`.

Raw `/api/dev/nano` остаётся только Developer Mode.

## 1. Wi-Fi credentials

В каталоге `esp8266_http_bridge_r2`:

1. запустить `setup_wifi_secrets.ps1` или `setup_wifi_secrets.bat`;
2. открыть созданный `wifi_secrets.h`;
3. вписать те же SSID/password домашней сети, что использовались для R1.

`wifi_secrets.h` игнорируется Git и не коммитится.

## 2. Compile + upload

Использовать ту же конфигурацию ESP8266, которая уже прошла для R1.

После upload вернуть ESP в normal boot и восстановить рабочий UART:
- ESP TX0/GPIO1 → Nano D0/RX;
- Nano D1/TX → делитель 1.5 kΩ / 3.0 kΩ → ESP RX0/GPIO3;
- общая GND.

R2 также содержит startup UART framing fix, который был внесён в source R1 после первого физического upload.

## 3. Gate 1 — ping

`GET /api/ping`

Норма: `fw=HTTP_BRIDGE_R2`.

## 4. Gate 2 — read L01 state

`GET /api/light/status`

Норма: HTTP 200 и JSON с:
- `ok=true`;
- `enabled`;
- `color_mode`;
- `kelvin`;
- `rgb`;
- `brightness`;
- `saved`;
- `dirty`.

При текущем Nano MODE=OFF ожидается `enabled=false`.

## 5. Gate 3 — app ON/OFF

Пересобрать/запустить текущий Android project.

На экране `Обычный свет L01`:
1. нажать `Включить` — кольцо D6 должно включиться текущим L01 profile, основной STATUS должен показать `MODE=L01`;
2. нажать `Выключить` — кольцо должно погаснуть, STATUS должен показать `MODE=OFF`;
3. повторить ON/OFF несколько раз.

## 6. Regression transport

После L01 ON/OFF:
- `Обновить` по-прежнему получает STATUS/TIME;
- Developer `PING` по-прежнему возвращает `PONG`;
- ESP/Nano не перезапускаются самопроизвольно.

## PASS

Stage 1 PASS, если semantic read + ON/OFF работают из обычного UI и прежний transport regression остаётся чистым.

После PASS следующий малый слой: brightness + Kelvin presets 2700/4000/6000 с явным Nano ACK, затем persistence.
