# ARDU ESP8266 v2 — ARDU-DIRECT + Ambient P01..P12

**Статус:** исходный файл скомпилирован в GitHub CI, на железо ещё не загружен. Физическую OTA выполняет владелец через Arduino IDE по [единой инструкции](../../ARDU_AMBIENT12_RELEASE_CANDIDATE.md).

## Исходники и пароли

Arduino IDE открывает `esp8266_ardu_v2.ino`. Для реальной прошивки создать в **этой же папке** свой `wifi_secrets.h`: скопировать `wifi_secrets.example.h`, заполнить:
- `ARDU_WIFI_SSID` — действующее домашнее имя сети;
- `ARDU_WIFI_PASSWORD` — домашний пароль;
- `ARDU_OTA_PASSWORD` — OTA пароль, который будет использовать новая ESP; для обслуживания лучше оставить прежний;
- `ARDU_SOFTAP_PASSWORD` — новый сильный пароль для защищённой `ARDU-DIRECT`, **не менее 8 символов**.

После заполнения заменить `ARDU_CREDENTIALS_CONFIGURED 0` на `ARDU_CREDENTIALS_CONFIGURED 1`. Без файла или с example passwords реальная сборка **намеренно выдаёт compile error**, чтобы случайно не потерять OTA-доступ. `wifi_secrets.h` игнорируется глобальным Git rule `**/wifi_secrets.h`, секреты не публиковать.

`ARDU_CI_BUILD` — только для автоматического теста пустых credentials, вручную никогда не устанавливать его перед OTA. Встроенные локальные типы позволяют проверять один `.ino` без второго project-header; локальный secret header **обязателен** на устройстве.

## OTA

ESP v2 следует обновить **первым** через уже работающий домашний Station/Arduino OTA port `ardu` (порт 8266), сохранив роутер включённым. При запросе OTA-пароля во время текущей загрузки нужен пароль из **старой, установленной ESP прошивки**. Затем проверить `/api/ping`: `fw:"ARDU_ESP_V2_AMBIENT12"`, `network_mode:"station"`, `uart_protocol:1`, `ota_ready:true`.

До Nano v2 чтение 12 сцен возвращает `supported:false`, это ожидаемо. После полной Nano v2 становится доступна цепочка `GET /api/ambient/presets`, `POST /api/ambient/preset`, `POST /api/ambient/preset/settings`, расширение `GET /api/settings`. `F02/F03` не пользовательские эффекты, зарезервированы. Только после этого проверить отключение роутера и подключение телефона к WPA2 `ARDU-DIRECT` на `192.168.4.1`. OTA в SoftAP-режиме не предполагается.

**[GitHub CI 37820404281](https://github.com/dronrome1245/ARDU/actions/runs/37820404281) PASS:** полный ESP v2, one-file CI source, обычная сборка с непубличным тестовым `wifi_secrets.h`, проверка compile guard, исходный ESP v1. После физического стендового теста отдельно подтверждаются автономность Wi-Fi, возврат Station и действующее OTA.
