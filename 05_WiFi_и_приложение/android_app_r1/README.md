# ARDU Android app — текущая рабочая копия

Первый native Android/Kotlin слой для проверки уже закрытого транспорта ARDU:

- `GET /api/ping`;
- `GET /api/status` — показывает фактический raw `STATUS` Nano;
- `GET /api/time` — показывает фактическое время DS3231;
- Developer Mode: raw `POST /api/dev/nano` и полный ответ Nano.

Nano остаётся источником истины. R1 не пытается превращать текущий raw transport bridge в окончательный semantic API.

## Подключение

R1 сначала пробует локальное имя `ardu.local`, затем текущий проверенный адрес стенда как fallback. Адрес не показывается в обычном пользовательском интерфейсе. Это временный transport-layer механизм; позже его заменит нормальное обнаружение/настройка устройства.

Обычный экран показывает только:

- `Онлайн / Нет связи`;
- версию ESP bridge;
- `STATUS` Nano;
- время DS3231.

Техническая raw-команда скрыта в Developer Mode.

## Открытие

1. В Android Studio выбрать **Open**.
2. Открыть каталог `05_WiFi_и_приложение/android_app_r1`.
3. Дождаться Gradle Sync. Проект использует AGP 9.4.1, Gradle 9.6, JDK 17 и `compileSdk 36`.
4. Запустить на Android-телефоне, подключённом к той же домашней Wi-Fi сети, что и ARDU.

Подробный hardware/app smoke-test: `../ANDROID_APP_R1_TEST.md`.

## Следующий слой

После физического app transport PASS:

1. добавить обычный свет L01;
2. выполнить сквозной regression L01 через приложение;
3. затем продолжать по D-071 малыми интеграционными слоями.

`/api/events` намеренно оставлен на следующую малую итерацию Developer Mode.


## Transport R1 — PASS 2026-09-30

На физическом Android-телефоне пройдены:
- Gradle Sync;
- build/install/launch;
- `/api/ping` → `Онлайн • HTTP_BRIDGE_R1`;
- `/api/status` → FW10 CORE R1;
- `/api/time` → растущее время DS3231;
- Developer `PING` → `PONG`.

## L01 stage 1 — semantic API на HTTP_BRIDGE_R5

Физический ESP теперь работает на `HTTP_BRIDGE_R5`; OTA и post-OTA transport regression пройдены.

Обычный пользовательский L01 UI больше не использует временный raw compatibility adapter:
- read L01: `GET /api/light/status`;
- ON/OFF: `POST /api/light/settings {"enabled":true|false}`;
- после write приложение перечитывает фактическое состояние L01;
- Nano остаётся источником истины;
- raw `POST /api/dev/nano` остаётся только в Developer Mode.

Версия приложения для этого слоя: `0.3-l01-semantic-r1`.

До физического PASS L01 ON/OFF brightness/Kelvin/RGB/persistence в UI не добавляются.

## L01 stage 2 — brightness + Kelvin presets

Версия приложения: `0.4-l01-bright-kelvin-r1`.

После физического ON/OFF PASS обычный L01 UI расширен:
- brightness slider `0..255`; один semantic write выполняется при отпускании slider;
- preset buttons `2700 K / 4000 K / 6000 K`;
- после каждого write приложение перечитывает `GET /api/light/status`;
- ON/OFF остаётся semantic;
- raw endpoint остаётся только Developer Mode.

Физический ESP для этого слоя должен быть `HTTP_BRIDGE_R6`, который добавляет atomic `brightness` и Kelvin preset writes. RGB, произвольный Kelvin slider и persistence пока не входят в этот gate.

## L01 stage 3 — complete profile

Версия приложения: `0.5-l01-profile-r1`.

Добавлено:
- arbitrary Kelvin slider `1800..6500`;
- RGB controls `R/G/B 0..255` + `Применить RGB`;
- индикатор saved/dirty;
- кнопка `Сохранить как свет после включения питания`;
- persistence write выполняется только по явной кнопке, не при каждом движении slider.

Backend: `HTTP_BRIDGE_R7`.
