# ARDU — ESP8266 HTTP BRIDGE R3 + OTA test

Дата: 2026-09-30

## Назначение

R3 — обслуживаемая ESP8266-ревизия для уже компактно спаянного ARDU.

Она сохраняет HTTP/UART transport R1, L01 semantic endpoints R2 и добавляет password-protected ArduinoOTA.

Главная цель: один последний wired upload R3, после которого дальнейшие ESP firmware updates выполнять по домашнему Wi-Fi без перекоммутации UART/GPIO0/RESET.

## 1. Перед первой wired прошивкой

Открыть:

`05_WiFi_и_приложение/esp8266_http_bridge_r3/esp8266_http_bridge_r3.ino`

В самом начале скетча вручную заменить:

```cpp
const char* WIFI_SSID = "PUT_YOUR_WIFI_SSID_HERE";
const char* WIFI_PASSWORD = "PUT_YOUR_WIFI_PASSWORD_HERE";
const char* OTA_PASSWORD = "PUT_A_STRONG_OTA_PASSWORD_HERE";
```

на реальные локальные значения.

Требование R3: OTA password минимум 8 символов.

Не коммитить реальные значения в публичный GitHub.

## 2. Compile gate

Arduino IDE:
- Board: Generic ESP8266 Module;
- flash: фактические 4 MB;
- Upload Speed для wired upload: 115200.

Сначала Verify.

Если compile не проходит — физический ESP не трогать; прислать полный compiler error.

## 3. Первый и последний обязательный wired upload

Использовать уже hardware-tested programmer procedure проекта:
- локальный 100 µF / 10 V на ESP VCC/GND оставить;
- Nano RESET → GND, чтобы ATmega не мешал USB↔UART;
- GPIO0 ESP удерживать LOW;
- programmer UART собрать как ранее подтверждено;
- при `Connecting...` кратко RST ESP → GND и отпустить;
- Upload Speed 115200.

Норма:
- ESP8266EX найден;
- flash 4 MB;
- Writing до 100%;
- Hash of data verified.

После upload обязательно вернуть normal boot и рабочий UART.

## 4. Normal boot gate

Вернуть:
- GPIO0 через 4.7 kΩ к 3.3 V;
- RST через 4.7 kΩ к 3.3 V;
- Nano RESET больше не на GND;
- ESP TX0/GPIO1 → Nano D0/RX;
- Nano D1/TX → рабочий divider 1.5 kΩ / 3.0 kΩ → ESP RX0/GPIO3;
- common GND.

Перезапустить питание.

## 5. HTTP/Wi-Fi gate

На ПК:

```powershell
curl.exe http://ardu.local/api/ping
```

Если mDNS в Windows не резолвится, использовать IP ESP из роутера.

Норма:
- `ok=true`;
- `fw=HTTP_BRIDGE_R3`;
- `wifi_connected=true`;
- `ota_ready=true`;
- `ota_hostname=ardu`;
- `ota_port=8266`.

Также проверить:
- `GET /api/status`;
- `GET /api/time`;
- Developer `PING→PONG`.

## 6. OTA discovery in Arduino IDE

После успешного normal boot компьютер и ESP должны быть в одной домашней сети.

В Arduino IDE открыть Tools → Port.

Ожидается сетевой порт ARDU/ESP8266 с hostname `ardu` и IP устройства.

Если порт не появляется:
1. подождать 10–20 s;
2. убедиться, что `/api/ping` отвечает и `ota_ready=true`;
3. проверить firewall Windows/mDNS;
4. не разбирать UART — сначала диагностировать сеть.

## 7. Первый OTA proof

Чтобы доказать OTA без функционального изменения:
1. в локальной копии скетча изменить только комментарий или строку версии тестовой ревизии после отдельного commit/revision;
2. выбрать сетевой Port `ardu`;
3. нажать Upload;
4. Arduino IDE запросит OTA password — ввести `OTA_PASSWORD`.

Норма:
- upload идёт по сети;
- после успешного OTA ESP автоматически перезагружается;
- wired programmer UART не трогаем;
- после reboot `/api/ping` снова отвечает;
- STATUS/TIME/PING до Nano остаются рабочими.

## 8. OTA safety regression

После OTA:
- приложение снова Online;
- `GET /api/status` → FW10 CORE R1;
- `GET /api/time` → валидное растущее DS3231 time;
- Developer `PING` → `PONG`;
- L01 ON/OFF не повреждены;
- Nano `UPTIME_MS` не должен сбрасываться только из-за OTA ESP.

## PASS

R3 PASS если:
1. wired upload R3 выполнен один раз;
2. Wi-Fi/HTTP работает с ручными credentials из скетча;
3. ping показывает `HTTP_BRIDGE_R3` и `ota_ready=true`;
4. Arduino IDE видит network OTA port;
5. тестовая следующая ESP revision загружается по Wi-Fi;
6. после OTA transport ESP↔Nano и Android app остаются рабочими.

После этого физический programmer wiring ESP не требуется для обычных firmware revisions, пока OTA itself исправен.
