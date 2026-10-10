# ESP8266 V3 — warm dawn, последний стабилизатор ARDU-DIRECT

**Единый файл:** `esp8266_warm_dawn_v3.ino`. [ESP8266 Verify PASS](https://github.com/dronrome1245/ARDU/actions/runs/38050334769). Отдельного `wifi_secrets.h` нет. Реальный OTA и аппаратный Direct ещё не выполнены владельцем.

## Конфигурация
Четыре `#define` с шаблонными строками в начале скетча: `ARDU_WIFI_SSID`, `ARDU_WIFI_PASSWORD`, `ARDU_OTA_PASSWORD`, `ARDU_SOFTAP_PASSWORD`. Скачай Raw в личную папку **вне C:\ARDU**, скопируй действующие значения из своей последней локальной ESP V2 и не отправляй их в GitHub/чат. С шаблонами firmware специально не компилируется. Для OTA нужны домашний Station Wi-Fi и **старый** OTA password текущей установленной ESP V2.

## Функции
- HTTP v1 как в ESP V2 (P01..P12, L01, Music, Night, Alarm, time, OTA).
- Блокировать STA radio-scan при `WiFi.softAPgetStationNum()>0`, что минимизирует обрывы ARDU-DIRECT из-за попыток найти домашний роутер каждые 10 сек. После ухода клиентов Station discovery возобновляется.
- `/api/settings` `alarm.dawn_kelvin_supported` есть только при Nano R5; `POST /api/alarm/settings` c start/end Kelvin проверяет оба поля, поддержку и атомарно передаёт `78`, затем `77` сохраняет настройки. На старой Nano вернуть 409.
- `/api/ping` `fw:"ARDU_ESP_V3_WARM_DAWN"`, диагностическое `softap_clients`; пароли через API не возвращаются.

## Загрузка и тест
Используй проверенные Generic ESP8266 Module / Arduino IDE / работающий сетевой OTA port в домашнем Wi-Fi. После OTA `curl.exe http://192.168.0.4/api/ping` (или другой действующий IP) должен показать V3 и `network_mode:station`. `ota_ready` при SoftAP может быть false: обновляй через Station, не рассчитывай на OTA из Direct. Сохранённый полный ESP V2 source остаётся rollback. Затем прошей Nano R5 для активации warm dawn; если Nano R4 осталась, новый ESP будет честно сообщать `dawn_kelvin_supported:false`.

Детали и hardware gate: `../../RELEASE_WARM_DAWN_DIRECT_ACCEPTANCE.md`.
