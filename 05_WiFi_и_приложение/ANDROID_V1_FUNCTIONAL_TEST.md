# ARDU Android v1 — functional acceptance

Дата: 2026-10-02  
Версия: `0.9-functional-v1-rc1`  
Статус: SOURCE/CI PASS, PHYSICAL END-TO-END PENDING

Исполнять после:
1. final Nano v1 upload;
2. final ESP8266 v1 OTA;
3. firmware acceptance `RELEASE_V1_UPLOAD_ACCEPTANCE.md`.

## 1. Установка

1. `git pull`.
2. Android Studio → открыть `05_WiFi_и_приложение/android_app_r1`.
3. Build/Run на телефон в той же Wi-Fi сети.
4. При первом запуске приложение пробует сохранённый адрес, затем `ardu.local`, затем текущий стендовый fallback.
5. Если DHCP-адрес изменился: `Настройки → Адрес ARDU`, ввести IP без обязательного `http://`.

PASS:
- приложение запускается;
- показывает `Онлайн • ARDU_ESP_V1`;
- текущее время читается с DS3231;
- режим соответствует Nano.

## 2. Startup sync

Нажать `Обновить состояние`.

PASS:
- нет ошибок;
- все разделы получают значения из Nano;
- после перезапуска приложения UI снова восстанавливается из `/api/settings`, а не из старого локального состояния.

## 3. Обычный свет

Проверить:
- Включить/Выключить;
- brightness;
- 2700/4000/6000 K;
- произвольный Kelvin;
- RGB;
- `Сохранить как свет после включения питания`;
- clap ON/OFF.

После каждого действия UI должен перечитать фактическое состояние Nano.

## 4. Clap wizard

1. `Калибровать хлопки • 9 пар`.
2. Во время quiet baseline сохранять тишину.
3. Для каждой пары нажимать sample и делать double-clap.
4. Добрать 9 accepted samples.
5. `Рассчитать порог`.
6. Проверить double-clap.
7. Если работает — `Сохранить`; иначе `Отмена`.

PASS:
- прогресс соответствует Nano;
- приложение не вычисляет threshold само;
- cancel восстанавливает прошлую настройку.

## 5. Светомузыка

Проверить ON/OFF и каждый ID:
- M01;
- M02;
- M03;
- M04;
- M05;
- M08;
- M09.

Проверить, что UI скрывает неприменимые controls.

Проверить:
- active/background brightness;
- smoothing;
- sensitivity;
- M05/M08 submode;
- M08 speed;
- M02 rainbow speed;
- M09 hue start/hue step;
- `Калибровать микрофон`.

PASS: каждое подтверждённое изменение после reread совпадает с Nano.

## 6. Фон

Проверить:
- ON/OFF;
- F01/F02/F03;
- hue;
- saturation там, где применима;
- brightness;
- speed F02/F03;
- rainbow step F03;
- auto-cycle;
- auto period.

## 7. Ночник

Проверить:
- manual ON/OFF;
- hue/saturation/brightness;
- schedule ON/OFF;
- schedule_on / schedule_off;
- интервал через полночь.

## 8. Будильник/рассвет

Проверить:
- alarm enable;
- HH:MM;
- fade duration;
- max brightness;
- start/end hue;
- RTC sync с телефоном;
- реальный alarm trigger;
- `Остановить рассвет`.

## 9. Настройки/сервис

Проверить:
- текущий Nano firmware = `ARDU_V1`;
- UART = v1;
- RTC status;
- audio calibration status;
- current limit;
- `Прочитать события`.

После clap/alarm/dawn события должны отображаться semantic именами и raw `V ...`.

## 10. Developer Mode

Открыть Developer Mode.

Release interface — только numeric UART:
- `1` → `O 1`;
- `2` → `D 2 ...`;
- `4` → `D 4 ...`;
- `3` → settings data + `O 3`.

Текстовые `PING`, `STATUS`, `TIME` в release v1 не использовать.

## 11. Connection persistence

1. Ввести рабочий IP вручную.
2. Переподключиться.
3. Полностью закрыть приложение.
4. Запустить снова.

PASS: приложение сначала использует сохранённый рабочий адрес и не требует `ardu.local`.

## 12. Final PASS

Android functionality считается закрытой, когда все разделы проходят end-to-end на final Nano/ESP firmware без:
- HTTP timeout;
- stale state после write;
- crash;
- неверного отображения режима;
- записи неподходящих параметров в другой mode;
- необходимости использовать Developer Mode для обычной функции.

После этого следующий Android этап — визуальный design/UX polish, без изменения firmware/API contract.


## Mock-phone smoke result — 2026-10-02

Статус: **PASS** для mock path.

На физическом Android-телефоне:
- приложение подключилось к `ARDU_ESP_V1_MOCK`;
- connection header показал Online/firmware/RSSI;
- semantic mode и RTC time отобразились;
- section navigation работает;
- Alarm/Dawn settings отрисованы из mock `/api/settings`.

Это не заменяет финальный hardware acceptance, но подтверждает реальный Android→LAN→HTTP mock path до получения replacement Nano.
