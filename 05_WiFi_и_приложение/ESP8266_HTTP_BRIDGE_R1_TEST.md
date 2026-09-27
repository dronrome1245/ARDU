# ARDU — ESP8266 HTTP BRIDGE R1 test

Дата: 2026-09-27

## Назначение

Этот слой даёт Android-приложению транспорт до Nano до реализации полного semantic API.

Схема:

телефон → домашний Wi-Fi → ESP8266 HTTP → UART → Nano FW-10

R1 не реализует SoftAP и не требует облака.

## Файлы

- \`05_WiFi_и_приложение/esp8266_http_bridge_r1/esp8266_http_bridge_r1.ino\`
- \`05_WiFi_и_приложение/esp8266_http_bridge_r1/wifi_secrets.example.h\`

## 1. Wi-Fi credentials

Скопировать:

\`wifi_secrets.example.h\` → \`wifi_secrets.h\`

и заполнить:

\`\`\`cpp
#define ARDU_WIFI_SSID "..."
#define ARDU_WIFI_PASSWORD "..."
\`\`\`

\`wifi_secrets.h\` не коммитить.

## 2. ESP hardware

Фактическая плата проекта:
- ESP8266MOD AI-THINKER на белой adapter board;
- VCC = 3.3 V от YP-8;
- GND общий с Nano;
- локальный 100 µF между ESP VCC/GND оставить.

Рабочий UART после прошивки:
- ESP TX0 / GPIO1 → Nano D0/RX;
- Nano D1/TX → level divider → ESP RX0/GPIO3;
- общий GND обязателен.

Nano TX нельзя подключать прямо к ESP RX как постоянное решение.

Во время прошивки ESP использовать уже подтверждённую процедуру проекта и при необходимости временно отключить рабочий UART Nano.

## 3. Compile/Upload

Использовать уже установленный ESP8266 Arduino core и ранее рабочую конфигурацию Generic ESP8266 Module / 4 MB flash.

После upload:
- GPIO0 вернуть в normal boot HIGH;
- восстановить рабочий UART к Nano;
- перезапустить ESP.

## 4. Найти устройство

ESP использует DHCP и hostname \`ardu\`.

Варианты:
- посмотреть адрес \`ardu\` / ESP8266 в DHCP clients домашнего роутера;
- попробовать \`ardu.local\` в сети с mDNS support.

Приложение будет хранить base URL отдельно; IP не показывается в обычном пользовательском режиме.

## 5. HTTP smoke test

С телефона/ПК в той же домашней сети:

\`GET http://<ESP_IP>/api/ping\`

Ожидается JSON с:
- \`ok=true\`;
- \`device=ARDU-ESP8266\`;
- \`fw=HTTP_BRIDGE_R1\`;
- IP/RSSI.

## 6. Nano transport

Nano должен быть загружен FW-10 CORE R1.

\`GET /api/status\`

должен вернуть raw Nano STATUS внутри JSON.

\`GET /api/time\`

должен вернуть raw Nano TIME.

## 7. Developer raw command

\`POST /api/dev/nano\`

Body = plain text, например:

\`\`\`
PING
\`\`\`

Ожидается Nano response \`PONG\`.

Проверить минимум:

\`\`\`
STATUS
LIGHT STATUS
CLAP STATUS
TIME
NIGHT STATUS
ALARM STATUS
DAWN STATUS
PING
\`\`\`

Это тот же набор, который позже будет доступен в Developer Mode Android-приложения.

## 8. Async events

\`GET /api/events\`

возвращает последние UART lines, пришедшие от Nano вне синхронного HTTP request.

Это R1 polling interface. Более поздний app layer может регулярно опрашивать его.

## PASS

R1 проходит, если:
1. ESP стабильно подключается к домашнему Wi-Fi;
2. \`/api/ping\` отвечает;
3. \`/api/status\` получает STATUS Nano;
4. raw POST \`PING\` получает PONG;
5. несколько Nano команд подряд не повреждают UART;
6. ESP не перезапускается под активным Wi-Fi;
7. питание ESP остаётся стабильным.

После этого создаётся Android Studio/Kotlin app R1, сначала с Connection + Developer Mode, затем с пользовательскими экранами.
