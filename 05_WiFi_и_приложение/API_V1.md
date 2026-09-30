# ARDU HTTP API v1 — предварительный контракт

Статус: ЧАСТИЧНО РЕАЛИЗОВАН  
Дата актуализации: 2026-09-30

## 1. Общие правила

- только домашняя локальная Wi-Fi сеть, без SoftAP и без облака; доступ в Интернет не требуется;
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

Допустимые разделы v1:

- `light`
- `music`
- `ambient`
- `night`
- `alarm`
- `off`

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
