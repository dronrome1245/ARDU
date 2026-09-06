# ARDU HTTP API v1 — предварительный контракт

Статус: ПРОЕКТ, до реализации ESP8266  
Дата: 2026-09-06

## 1. Общие правила

- локальная сеть, без облака;
- JSON UTF-8;
- ESP8266 — HTTP-шлюз;
- Nano — источник истины по состоянию света;
- изменение состояния считается применённым после подтверждения Nano;
- ошибки возвращаются явно, приложение не должно угадывать состояние.

Базовый префикс: `/api`.

## 2. Состояние

### `GET /api/status`

Предварительный ответ:

```json
{
  "online": true,
  "mode": "music",
  "power": true,
  "music_mode": "M01",
  "alarm_enabled": true,
  "dawn_active": false,
  "rtc_ok": true
}
```

### `GET /api/settings`

Возвращает актуальные настройки, считанные из Nano/синхронизированного кэша ESP.

### `GET /api/time`

Возвращает текущее время RTC.

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

- `music`
- `ambient`
- `night`
- `alarm`
- `off`

## 4. Светомузыка

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

## 5. Фон

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
  "speed":30
}
```

## 6. Ночник

### `POST /api/night/settings`

```json
{
  "enabled":true,
  "hue":24,
  "saturation":180,
  "brightness":18
}
```

Расписание добавить после фиксации модели расписаний.

## 7. Будильник

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

## 8. Что ещё не фиксировано

До прошивки ESP8266 нужно отдельно определить:

- коды HTTP ошибок;
- точные диапазоны каждого поля;
- формат версии/диагностики;
- модель первичной настройки Wi‑Fi;
- нужно ли автообнаружение устройства в LAN;
- модель сохранения настроек и версионирование структуры.
