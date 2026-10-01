# ARDU v1 — финальная загрузка и acceptance

Дата: 2026-10-01  
Статус: ГОТОВО К ВЫПОЛНЕНИЮ  
Цель: один финальный firmware session без промежуточных ревизий.

## 1. Release candidates

### Nano

`04_Прошивка/nano_ardu_v1/nano_ardu_v1.ino`

CI:
- flash: 28024 / 30720 = 91%;
- globals: 1070 / 2048 = 52%;
- free SRAM: 978 bytes.

### ESP8266

`05_WiFi_и_приложение/esp8266_ardu_v1/esp8266_ardu_v1.ino`

CI:
- RAM: 31988 / 80192 = 39%;
- IRAM: 60823 / 65536 = 92%;
- IROM code: 324376 bytes.

UART: `06_Интерфейс_управления/UART_V1.md`.

## 2. До начала

1. `git pull`.
2. Закрыть Android-приложение ARDU и все curl/HTTP polling.
3. Открыть локальную копию `esp8266_ardu_v1.ino`.
4. Перенести в неё из рабочей локальной R7 те же:
   - `WIFI_SSID`;
   - `WIFI_PASSWORD`;
   - `OTA_PASSWORD`.
5. Реальные секреты не коммитить.
6. Сначала нажать Verify ESP v1 локально. Upload пока не выполнять.
7. Убедиться, что физический ESP R7 сейчас подключается к Wi-Fi и OTA network port доступен.

## 3. Финальная загрузка Nano v1

### 3.1 Изолировать RX Nano

При выключенном питании временно разорвать только:

`ESP TX0 / GPIO1 → Nano D0 / RX`.

Причина: прошлая попытка Nano upload с подключённым ESP TX дала `avrdude resp=0x00`.

Линию:

`Nano D1/TX → divider → ESP RX`

можно оставить: ESP RX является входом и не должен конкурировать с USB-UART.

D7 может оставаться физически неподключённым.

### 3.2 Arduino IDE

- Board: `Arduino Nano`;
- Processor: `ATmega328P (Old Bootloader)`;
- актуальный COM Nano;
- открыть `nano_ardu_v1.ino`;
- Upload.

Ожидается успешный avrdude write/verify.

### 3.3 Прямой smoke через USB Serial

Пока ESP TX→Nano D0 ещё разорван:

Serial Monitor = 115200, newline.

Отправить:

```text
1
```

Ожидание:

```text
O 1
```

Затем:

```text
2
```

Ожидание: одна строка `D 2 ...`.

Затем:

```text
4
```

Ожидание: `D 4 1 ...` с валидным DS3231 временем.

Затем:

```text
3
```

Ожидание:
- несколько `D 3 ...`;
- в конце `O 3`.

Если эти четыре команды проходят — Nano release transport gate закрыт.

## 4. Восстановить UART

1. Закрыть Serial Monitor.
2. Обесточить стенд перед перестановкой провода.
3. Вернуть `ESP TX0/GPIO1 → Nano D0/RX`.
4. Проверить, что штатный divider Nano D1/TX→ESP RX не изменён.
5. Общая GND остаётся общей.
6. Включить стенд.

С этого момента старый физический ESP R7 уже не совместим с numeric Nano UART. Это ожидаемое короткое переходное состояние. Не выполнять functional HTTP test до OTA ESP v1.

## 5. Финальная OTA-загрузка ESP8266 v1

1. В Arduino IDE открыть локальный `esp8266_ardu_v1.ino` с заполненными credentials.
2. Выбрать уже проверенный OTA network port `ardu` / IP текущего ESP.
3. Upload по OTA.
4. Проводку GPIO0/RST/UART не менять.
5. Дождаться reboot ESP.

Wired ESP programmer после этого остаётся recovery-only.

## 6. Первый HTTP gate

PowerShell:

```powershell
$base = "http://192.168.0.4"
Invoke-RestMethod "$base/api/ping"
```

Ожидание:
- `ok=true`;
- `fw=ARDU_ESP_V1`;
- `uart_protocol=1`;
- `wifi_connected=true`;
- `ota_ready=true`.

Затем:

```powershell
Invoke-RestMethod "$base/api/status"
Invoke-RestMethod "$base/api/settings"
Invoke-RestMethod "$base/api/time"
```

Критерии:
- status отвечает без timeout;
- `nano_fw=ARDU_V1`;
- RTC valid;
- settings содержит Light/Clap/Night/Alarm/Ambient/Music/System.

Developer transport:

```powershell
Invoke-RestMethod "$base/api/dev/nano" -Method Post -ContentType "text/plain" -Body "1"
```

Ожидание: `nano = "O 1"`.

## 7. L01 regression

### ON

```powershell
Invoke-RestMethod "$base/api/light/settings" -Method Post -ContentType "application/json" -Body '{"enabled":true}'
```

### Baseline profile

```powershell
Invoke-RestMethod "$base/api/light/settings" -Method Post -ContentType "application/json" -Body '{"kelvin":2700}'
Invoke-RestMethod "$base/api/light/settings" -Method Post -ContentType "application/json" -Body '{"brightness":64}'
Invoke-RestMethod "$base/api/light/settings" -Method Post -ContentType "application/json" -Body '{"persist_startup_profile":true}'
Invoke-RestMethod "$base/api/light/status"
```

Критерий: физический Ring A работает как раньше; state reread соответствует Nano.

## 8. Clap

Проверить toggle enable/disable:

```powershell
Invoke-RestMethod "$base/api/light/settings" -Method Post -ContentType "application/json" -Body '{"clap_enabled":false}'
Invoke-RestMethod "$base/api/light/settings" -Method Post -ContentType "application/json" -Body '{"clap_enabled":true}'
```

Runtime double-clap должен переключать L01/OFF с текущим сохранённым threshold.

### Family calibration acceptance

Start:

```powershell
Invoke-RestMethod "$base/api/light/clap/calibration/start" -Method Post -ContentType "application/json" -Body '{"pairs":9}'
```

Для каждой пары хлопков выполнить:

```powershell
Invoke-RestMethod "$base/api/light/clap/calibration/sample" -Method Post
```

После 9 accepted samples:

```powershell
Invoke-RestMethod "$base/api/light/clap/calibration/finish" -Method Post
Invoke-RestMethod "$base/api/light/clap/calibration" 
```

Проверить double-clap с suggested threshold.

Если устраивает:

```powershell
Invoke-RestMethod "$base/api/light/clap/calibration/save" -Method Post
```

Если нет:

```powershell
Invoke-RestMethod "$base/api/light/clap/calibration/cancel" -Method Post
```

## 9. Music acceptance

Калибровка MAX9814:

```powershell
Invoke-RestMethod "$base/api/music/calibrate" -Method Post
```

По очереди выбрать:
- M01
- M02
- M03
- M04
- M05
- M08
- M09

Пример:

```powershell
Invoke-RestMethod "$base/api/music/mode" -Method Post -ContentType "application/json" -Body '{"id":"M01"}'
```

Для M05/M08 проверить submode:

```powershell
Invoke-RestMethod "$base/api/music/settings" -Method Post -ContentType "application/json" -Body '{"submode":"low","persist":false}'
Invoke-RestMethod "$base/api/music/settings" -Method Post -ContentType "application/json" -Body '{"submode":"three","persist":true}'
```

Критерий: каждый ID визуально активируется, UART/HTTP не зависает.

## 10. Ambient acceptance

```powershell
Invoke-RestMethod "$base/api/ambient/effect" -Method Post -ContentType "application/json" -Body '{"id":"F01"}'
Invoke-RestMethod "$base/api/ambient/effect" -Method Post -ContentType "application/json" -Body '{"id":"F02"}'
Invoke-RestMethod "$base/api/ambient/effect" -Method Post -ContentType "application/json" -Body '{"id":"F03"}'
```

F03 parameter smoke:

```powershell
Invoke-RestMethod "$base/api/ambient/settings" -Method Post -ContentType "application/json" -Body '{"speed":1,"rainbow_step":0.5,"persist":true}'
```

## 11. Night acceptance

Manual:

```powershell
Invoke-RestMethod "$base/api/night/settings" -Method Post -ContentType "application/json" -Body '{"enabled":true,"hue":24,"saturation":180,"brightness":18,"persist":true}'
```

Затем OFF и обратно.

Schedule можно проверить коротким окном через `schedule_on/schedule_off`; старый NIGHT R2 subsystem уже hardware-tested, поэтому повторный многоцикловый test не требуется.

## 12. Alarm/Dawn acceptance

1. `GET /api/time`.
2. Установить alarm на ближайшие 1–2 минуты.
3. Для быстрого acceptance использовать:
   - fade_minutes = 1;
   - max_brightness в безопасном диапазоне.
4. Дождаться trigger.
5. Проверить начало Dawn.
6. Во время Dawn:

```powershell
Invoke-RestMethod "$base/api/alarm/stop-dawn" -Method Post
```

Критерий: Dawn прекращается.

После теста вернуть пользовательские настройки Alarm.

## 13. Events

```powershell
Invoke-RestMethod "$base/api/events"
```

После clap/alarm/dawn должны присутствовать semantic event objects с raw `V ...`.

## 14. Power-cycle persistence

После установки желаемых финальных значений:

1. полностью снять питание ARDU;
2. снова подать питание;
3. L01 должен стартовать с сохранённым profile;
4. `GET /api/settings` должен вернуть сохранённые значения;
5. clap threshold должен сохраниться;
6. current limit должен сохраниться.

## 15. Current limit

До подключения второго кольца оставить default release limit:

`3000 mA total`.

Не поднимать до 4500 mA на одном кольце ради теста.

После физического подключения Ring B и финального Class II PSU значение подбирается через:

`POST /api/system/current-limit`

без новой прошивки.

## 16. Критерий firmware freeze

Firmware freeze разрешён, если:

- Nano direct smoke 1/2/3/4 PASS;
- ESP ping/status/settings/time PASS;
- L01 PASS;
- clap runtime + calibration PASS;
- Music IDs PASS;
- Ambient PASS;
- Night PASS;
- Alarm/Dawn/STOP PASS;
- events PASS;
- power-cycle persistence PASS;
- нет reset, UART corruption или LED glitches.

После этого:
- Nano/ESP v1 больше не меняются без release-blocker;
- следующий этап = Android functional completion + design.
