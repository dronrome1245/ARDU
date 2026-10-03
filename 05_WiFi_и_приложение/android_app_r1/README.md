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


## Functional v1 release candidate — 2026-10-02

Версия приложения: `0.9-functional-v1-rc1`.

Приложение переведено с временного HTTP_BRIDGE_R7 contract на финальный `ARDU_ESP_V1` API.

Реализовано:
- startup sync через `/api/ping` + `/api/status` + `/api/settings` + `/api/time`;
- сохранение последнего рабочего адреса и ручной адрес ARDU; `ardu.local` остаётся fallback, но приложение от него не зависит;
- Обычный свет: ON/OFF, brightness, Kelvin 1800..6500, presets, RGB, startup profile;
- clap enable + полный family calibration wizard: quiet baseline → 9 samples → finish → save/cancel;
- Music: M01/M02/M03/M04/M05/M08/M09, применимые dynamic controls, microphone calibration;
- Ambient: F01/F02/F03, HSV/brightness/speed/rainbow, auto-cycle/period;
- Night: manual power, hue/saturation/brightness, schedule enable + HH:MM window;
- Alarm/Dawn: enable, time, fade duration, max brightness, start/end hue, RTC sync, STOP Dawn;
- service current limit;
- semantic async events;
- Developer Mode использует только frozen numeric UART v1, textual `PING/STATUS` больше не является release interface.

CI:
- Android debug APK compile PASS;
- JVM final API contract tests PASS;
- contract tests используют local mock HTTP server и проверяют реальные release JSON shapes и POST payloads без физического ARDU.

Физическая end-to-end проверка этой версии ожидает новый Nano + финальные Nano/ESP v1 uploads.


## Design v1 release candidate — 2026-10-02

Текущая версия: `0.10-design-v1-rc1`.

После mock-phone functional PASS интерфейс переведён из технического prototype в пользовательский UI:
- dark low-glare theme;
- fixed 5-section bottom navigation;
- Settings in the header;
- semantic cards instead of one long flat form;
- visible active section;
- color previews;
- Developer Mode isolated from normal controls.

Final GitHub Actions gate:
- `testDebugUnitTest` PASS;
- `assembleDebug` PASS.

Следующая проверка до replacement Nano может быть выполнена на том же stateful ESP mock: `git pull` → reinstall/run Android app → подключиться к `<PC_IP>:8080` и проверить все 5 вкладок визуально.


## Smart-home Light pilot — 2026-10-02

Version: `0.11-home-ui-rc1`.

Owner feedback from the v0.10 physical-phone mock review was applied:
- compact header;
- Refresh/Settings are small header actions;
- firmware/RSSI removed from everyday header;
- Light quick scenes;
- brightness percentage and +/- controls;
- native touch color wheel;
- exact RGB sliders collapsed by default.

The old slider APIs are still used where useful for precision, but they are no longer the only/primary interaction.

No firmware/API changes.

CI at final source commit:
- API contract tests PASS;
- debug APK build PASS.


## Owner-approved final UI RC — 2026-10-03

Current version: `0.14-ios-homehub-focus-rc1`.

Final direction selected by owner from the UI concept boards:
**Home Hub + Focus Dial**, mint-dark.

New native UI components:
- `ui/FocusDialView.kt`;
- `ui/RoomHeroView.kt`.

Light tab now opens a Home Hub overview and then a Focus Dial detail view. Other normal user tabs receive contextual room hero cards while preserving the complete v1 functionality implemented earlier.

No firmware/API changes and no third-party UI dependency.

Final CI:
- static layout IDs: clean;
- `testDebugUnitTest`: PASS;
- `assembleDebug`: PASS;
- Actions run: `37062432020`.

Phone/mock check:
`git pull` → run stateful ESP mock → reinstall/run Android → visually review Home Hub overview, Focus Dial detail and contextual heroes.


## Premium Light visual RC — 2026-10-03

Current Android version: `0.15-premium-light-rc1`.

Added:
- `ui/SceneTileView.kt`;
- deeper native Home Hub room rendering;
- gradient/glow/tick Focus Dial polish.

No external image package and no third-party visual dependency are required for this pass.

Final Actions:
- run `37102197040`;
- contract tests PASS;
- debug APK PASS.

Next manual check: Home Hub Light overview + Focus Dial detail on physical phone via stateful ESP mock.
