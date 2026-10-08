# ARDU HTTP API v1 — предварительный контракт

Статус: RELEASE v1 РЕАЛИЗОВАН В SOURCE / CI PASS / PHYSICAL UPLOAD PENDING  
Дата актуализации: 2026-10-01

## 1. Общие правила

- домашний Station — основной transport; при недоступном роутере доступен защищённый SoftAP `ARDU-DIRECT` на `192.168.4.1`; облако/Интернет не требуются;
- JSON UTF-8;
- ESP8266 — HTTP-шлюз;
- Nano — источник истины по состоянию света;
- изменение состояния считается применённым после подтверждения Nano;
- ошибки возвращаются явно, приложение не должно угадывать состояние.

Базовый префикс: `/api`.

## 2. Состояние

### OTA status fields (HTTP_BRIDGE_R4)

В R3 `GET /api/ping` дополнительно возвращает:
- `ota_ready` — OTA service запущен после успешного Wi-Fi connect;
- `network_mode` — `station`, `softap` или `offline`;
- `softap_active` — активен ли direct fallback;
- в SoftAP mode поле `ip` = `192.168.4.1`, дополнительно возвращается `softap_ssid`;
- `wifi_connected` сохраняет прежний смысл: это именно состояние домашнего Station, поэтому в рабочем direct mode может быть `false`;
- `ota_hostname` — `ardu`;
- `ota_port` — `8266`.

OTA password никогда не возвращается через HTTP.


### `GET /api/status`

Предварительный ответ:

```json
{
  "online": true,
  "mode": "music",
  "rtc_time": "12:34:56",
  "power": true,
  "music_mode": "M01",
  "alarm_enabled": true,
  "dawn_active": false,
  "rtc_ok": true
}
```

### `GET /api/settings`

Возвращает полный актуальный сохранённый набор настроек всех пользовательских режимов, считанный из Nano. ESP cache не является источником истины.

Ответ должен включать как минимум настройки `light`, всех Music ID, Ambient/F01-F03, Night и Alarm/Dawn, а также версию структуры настроек.

### `GET /api/time`

Возвращает фактическое время DS3231. Это источник времени, которое приложение показывает как время ARDU; часы телефона не подменяют его.

Предварительный ответ:

```json
{
  "valid": true,
  "date": "2026-09-27",
  "time": "12:34:56"
}
```

### Семантика persistent save

Обычные пользовательские POST-настройки должны различать live application и финальный persistent commit так, чтобы slider не писал EEPROM на каждом промежуточном шаге.

Требование к финальному протоколу:

- live preview может применяться в RAM;
- завершённое изменение должно быть записано Nano энергонезависимо;
- HTTP-ответ, означающий «сохранено», возвращается только после подтверждения Nano;
- API должен позволять приложению отличить `applied` от `persisted` либо обеспечивать эквивалентную гарантию.

## 3. Общий режим

### `POST /api/power`

```json
{"on":true}
```

Это программное выключение. Оно не заменяет физическую вторую клавишу 230 В.

### `POST /api/mode`

```json
{"mode":"music"}
```

Допустимые runtime-режимы v1:

- `light`
- `music`
- `ambient`
- `night`
- `off`

`alarm` не является отдельным постоянно активным top-level режимом: вкладка будильника настраивает Alarm/Dawn, а сам Dawn запускается по RTC. При переходе приложения на вкладку «Будильник» текущий декоративный/световой режим переводится в `off`, но будильник остаётся настроенным.

## 4. Обычный свет

### `GET /api/light/status`

**Реализовано в ESP8266 HTTP_BRIDGE_R2.**

ESP выполняет `STATUS` + `LIGHT STATUS` на Nano и возвращает семантическое состояние L01:

```json
{
  "ok": true,
  "enabled": false,
  "color_mode": "kelvin",
  "kelvin": 2700,
  "rgb": {"r":255,"g":170,"b":87},
  "brightness": 64,
  "saved": true,
  "dirty": false
}
```

`enabled` определяется по верхнему режиму Nano (`MODE=L01`). Значения цвета/яркости читаются из Nano, а не из cache ESP/телефона.

### `POST /api/light/settings`

**Первый реализованный срез HTTP_BRIDGE_R2:** только атомарное включение/выключение L01:

```json
{"enabled":true}
```

или

```json
{"enabled":false}
```

ESP преобразует это только в `LIGHT ON` / `LIGHT OFF` и возвращает успех после ACK Nano. Любые другие поля в R2 отклоняются с `R2_SUPPORTS_ENABLED_ONLY`; это защищает от молчаливого игнорирования ещё не реализованных настроек.

Полный целевой payload следующего слоя:

```json
{
  "enabled": true,
  "brightness": 128,
  "color_mode": "kelvin",
  "kelvin": 2700,
  "rgb": {"r":255,"g":196,"b":137},
  "clap_enabled": true,
  "persist_startup_profile": true
}
```

**Реализовано в HTTP_BRIDGE_R6 — второй атомарный срез:**

```json
{"brightness":128}
```

и Kelvin presets:

```json
{"kelvin":2700}
{"kelvin":4000}
{"kelvin":6000}
```

R6 принимает по одному полю за запрос. Mapping:
- `brightness` → `LIGHT BRIGHT <0..255>`;
- `kelvin=2700|4000|6000` → `LIGHT PRESET <value>`.

После успешного write приложение перечитывает `GET /api/light/status`; ESP и телефон не становятся источником истины. Произвольный Kelvin slider, RGB, clap и persistence остаются следующими слоями.

`enabled` включает/выключает L01 программно. `brightness` ограничивается общим лимитом тока Nano. `color_mode` = `kelvin` или `rgb`. Для `kelvin` используется приблизительная RGB-имитация температуры; приложение предоставляет presets 2700/4000/6000 и slider. Для `rgb` используется произвольный color picker. `persist_startup_profile=true` означает записать текущий L01 profile как тот, который будет применён после cold power-on. `clap_enabled` включает/выключает локальный хлопковый выключатель.

Точный clap detector остаётся внутренней логикой Nano и не переносится в приложение.

## 5. Светомузыка

### `POST /api/music/mode`

```json
{"id":"M03"}
```

### `POST /api/music/settings`

Пример:

```json
{
  "active_brightness":128,
  "background_brightness":20,
  "smoothing":40,
  "sensitivity":55,
  "submode":"low"
}
```

Передавать только применимые к текущему режиму поля.

### `POST /api/music/calibrate`

Запускает калибровку шума MAX9814. Ответ должен сообщить успешное завершение либо ошибку/таймаут.

## 6. Фон

### `POST /api/ambient/effect`

```json
{"id":"F02"}
```

### `POST /api/ambient/settings`

Пример:

```json
{
  "hue":32,
  "saturation":255,
  "brightness":80,
  "speed":30,
  "rainbow_step":0.5,
  "auto_cycle":false,
  "auto_period_s":10
}
```

## 7. Ночник

### `POST /api/night/settings`

```json
{
  "enabled":true,
  "hue":24,
  "saturation":180,
  "brightness":18,
  "schedule_enabled":true,
  "schedule_on":"22:00",
  "schedule_off":"07:00"
}
```

Расписание добавить после фиксации модели расписаний.

## 8. Будильник

### `POST /api/alarm/settings`

Предварительно:

```json
{
  "enabled":true,
  "hour":7,
  "minute":0,
  "fade_minutes":30,
  "max_brightness":120,
  "start_hue":8,
  "end_hue":32
}
```

### `POST /api/alarm/stop-dawn`

Немедленно прекращает активный рассвет.

### `POST /api/time/sync`

Синхронизирует DS3231 со временем телефона.

## 9. Developer Mode

Для режима разработчика потребуется отдельный сервисный HTTP-интерфейс поверх UART Nano. Минимально:

- запрос расширенного статуса/диагностики;
- получение live diagnostic values;
- отправка сервисной текстовой команды Nano и возврат полного ответа;
- передача асинхронных EVENT в приложение.

Точные URI и ограничения команд фиксируются после полного regression единой Nano-прошивки и заморозки UART v1. Обычные пользовательские настройки режимов не должны зависеть от raw-command endpoint.

## 10. Что ещё не фиксировано

До прошивки ESP8266 нужно отдельно определить:

- коды HTTP ошибок;
- точные диапазоны каждого поля;
- формат версии/диагностики;
- модель первичной настройки Wi‑Fi;
- нужно ли автообнаружение устройства в LAN;
- точный JSON-формат полного набора сохранённых настроек и version/schema migration; базовый принцип persistence уже зафиксирован в `04_Прошивка/PERSISTENCE_V1.md`.


### Clap calibration

Предварительный HTTP mapping к Nano calibration state machine:

- `POST /api/light/clap/calibration/start`

Пример:
```json
{"pairs":9}
```

- `POST /api/light/clap/calibration/sample` — открыть одно окно для double-clap sample;
- `POST /api/light/clap/calibration/finish` — вычислить и временно применить suggested threshold;
- `POST /api/light/clap/calibration/save` — persist после пользовательского теста;
- `POST /api/light/clap/calibration/cancel` — вернуть предыдущую сохранённую настройку;
- `GET /api/light/clap/calibration` — progress/state/result.

Пример status:
```json
{
  "active": true,
  "target_pairs": 9,
  "good_pairs": 5,
  "quiet_p99_derivative": 18,
  "suggested_threshold": null,
  "saved": true
}
```

ESP не рассчитывает threshold: он только маршрутизирует команды и состояние Nano.

### HTTP_BRIDGE_R7 — полный L01 profile write slice

`POST /api/light/settings` дополнительно поддерживает атомарные payload:

```json
{"kelvin":3500}
```

Диапазон arbitrary Kelvin: `1800..6500`.

```json
{"rgb":{"r":255,"g":40,"b":10}}
```

RGB диапазон каждого канала: `0..255`.

```json
{"persist_startup_profile":true}
```

Mapping к Nano:
- `kelvin` → `LIGHT KELVIN <1800..6500>`;
- `rgb` → `LIGHT RGB <r> <g> <b>`;
- `persist_startup_profile=true` → `LIGHT SAVE`.

R7 сохраняет уже реализованные atomic `enabled` и `brightness`. После любого write приложение перечитывает `GET /api/light/status`; Nano остаётся источником истины.


## 11. Финальный release-контракт ESP8266 v1 — 2026-10-01

Реализация: `05_WiFi_и_приложение/esp8266_ardu_v1/esp8266_ardu_v1.ino`.

Физическая загрузка ещё не выполнена, но source compile gate пройден.

### Реализованные endpoints

- `GET /api/ping`
- `GET /api/status`
- `GET /api/settings`
- `GET /api/time`
- `POST /api/time/sync`
- `POST /api/power`
- `POST /api/mode`
- `GET /api/light/status`
- `POST /api/light/settings`
- `GET /api/light/clap/calibration`
- `POST /api/light/clap/calibration/start`
- `POST /api/light/clap/calibration/sample`
- `POST /api/light/clap/calibration/finish`
- `POST /api/light/clap/calibration/save`
- `POST /api/light/clap/calibration/cancel`
- `POST /api/music/mode`
- `POST /api/music/settings`
- `POST /api/music/calibrate`
- `POST /api/ambient/effect`
- `POST /api/ambient/settings`
- `POST /api/night/settings`
- `POST /api/alarm/settings`
- `POST /api/alarm/stop-dawn`
- `GET/POST /api/system/current-limit`
- `POST /api/system/reset-defaults`
- `GET /api/events`
- `POST /api/dev/nano`

### System reset defaults — extension 2026-10-07

`POST /api/system/reset-defaults`

Без обязательного payload. Успех:

```json
{"ok":true,"applied":true}
```

Семантика:
- Light → 4000 K / brightness 64 / startup profile defaults;
- clap threshold/timeout + enable → defaults;
- Music modes/settings → v1 defaults;
- Ambient effects/settings → v1 defaults;
- Night → defaults, schedule OFF;
- Alarm/Dawn → defaults, alarm OFF;
- current limit → 3000 mA;
- top-level runtime после reset → `light`;
- RTC/date/time **не сбрасываются**;
- аппаратная MAX9814 audio calibration сохраняется, потому что это calibration конкретного устройства, а не пользовательская настройка.

Mapping: numeric UART opcode `130`.

### Ambient app presets — 2026-10-07

«Северное сияние / Закат / Океан / Космос» — это Android presets поверх существующих F01/F02/F03 и их параметров. Новые firmware effect IDs не создаются.

### Persistence/live-preview

Для slider-like настроек:
- Music/Ambient/Night/Alarm-Dawn принимают optional `"persist":false` для live preview;
- default при отсутствии поля = `true`;
- приложение при движении slider отправляет throttled preview с `persist:false`;
- при отпускании slider отправляет финальное значение с `persist:true`.

L01 сохраняет startup profile только по явному `"persist_startup_profile":true`.

Ambient effect selection `POST /api/ambient/effect` сохраняется сразу как пользовательский выбор.

### Music final fields

`POST /api/music/settings` поддерживает применимые поля:
- `active_brightness`;
- `background_brightness`;
- `smoothing`;
- `sensitivity`;
- `submode`: `three|low|mid|high` для M05/M08;
- `speed` для M08;
- `rainbow_step10` для M02;
- `hue_start`, `hue_step` для M09;
- `persist`.

### Ambient final fields

- `auto_cycle`;
- `auto_period_s`;
- `hue`;
- `saturation` (F01/F02);
- `brightness`;
- `speed` (F02/F03): диапазон 1..255, **большее число всегда означает более быстрое движение**;
- `rainbow_step` 0.5..10.0 (F03);
- `persist`.

### Night final fields

- `enabled`;
- `hue`;
- `saturation`;
- `brightness`;
- `schedule_enabled`;
- `schedule_on` / `schedule_off` in `HH:MM`;
- `persist`.

### Alarm/Dawn final fields

- `enabled`;
- `hour`, `minute`;
- `fade_minutes`;
- `max_brightness`;
- `start_hue`, `end_hue`;
- `persist`.

Внутренний numeric UART описан отдельно: `06_Интерфейс_управления/UART_V1.md`.

### D-104: Ambient future target vs existing HTTP API (2026-10-08)

Owner decision: F02 and F03 are unnecessary in the **future user-facing** Ambient experience; F01 remains. All F02/F03 mappings above describe the **existing** v1 API and still apply to current Android/Nano/ESP source. D-104 does **not** remove these endpoints, retire any wire identifiers, or authorize a new protocol.

The twelve atmospheric preset candidates are documented in `04_Прошивка/AMBIENT_PRESETS_CONCEPT.md` as an **idea only**. A possible `POST /api/ambient/preset` is not implemented, approved or frozen.

## D-109 — ESP v2 Ambient12 semantic HTTP extension (2026-10-08)

**Live-source target:** `ARDU_ESP_V2_AMBIENT12`, existing numeric UART protocol v1 with additive scene opcodes 111..115. `ARDU-DIRECT` WPA2 hotspot `192.168.4.1` is used only when the home Station router is unavailable, with same endpoints. OTA is deliberately Station-only.

- `GET /api/ambient/presets` → `{"ok":true,"presets":{"supported":true,"selected":"P03","active":true,"scenes":{"P01":{"brightness":115,"dynamics":90},...}}}` with all 12 values and real Nano readback. When Nano old: `{"ok":true,"supported":false,"reason":"NANO_UPDATE_REQUIRED"}`.
- `POST /api/ambient/preset` body `{"id":"P03"}` → Nano `111 3` followed by EEPROM commit `114`; returns `{"ok":true,"selected":"P03","applied":true,"persisted":true}`. P01..P12 only; F02/F03 not aliases.
- `POST /api/ambient/preset/settings` body `{"brightness":102,"dynamics":120,"persist":true}` → 112/113 + optional 114; brightness/dynamics 0..255, either optional but ≥1 required, persist defaults true, false means live RAM preview.
- `GET /api/settings` retains v1 schema response and adds top-level `ambient_presets`: `{"supported":true,"selected":"P03","active":true,"scenes":{"P01":{"brightness":...,"dynamics":...},...}}` or `{"supported":false}` on older Nano. Android must read from device, not assume app cache.
- `GET /api/status` adds `ambient_presets_supported`, `ambient_preset_selected` and identifies `nano_fw:"ARDU_V2"` when readback115 succeeds.
- `POST /api/ambient/effect` accepts only `{"id":"F01"}` (manual HSV). Historical F02/F03 HTTP and four Android macros described above are **v1-only**, retired in the new user experience; numeric IDs reserved, not reassigned.

Private `wifi_secrets.h` (home SSID/password, OTA password, new WPA2 `ARDU-DIRECT` pass ≥8 chars) is **mandatory** for real IDE compilation; `ARDU_CREDENTIALS_CONFIGURED=1` only after replacement, and compile-time guards reject template passwords. [ESP CI](https://github.com/dronrome1245/ARDU/actions/runs/37820404281) PASS. Full physical Wi-Fi/OTA/protocol smoke still pending; see `ARDU_AMBIENT12_RELEASE_CANDIDATE.md`.
