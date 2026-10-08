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


## Real-asset Light RC — 2026-10-03

Current Android version: `0.16-real-assets-light-rc1`.

New resources:
`app/src/main/res/drawable-nodpi/`
- `ardu_light_hero.webp`
- `ardu_scene_evening.webp`
- `ardu_scene_warm.webp`
- `ardu_scene_day.webp`
- `ardu_scene_cool.webp`

Total optimized asset size is about 24 KB.

Light Home Hub now renders real room imagery. The Light-specific header lives over the hero image; the old global header is hidden on the Light tab. Scene tiles use real photos.

Final CI: Actions `37105664138` — contract tests PASS, debug APK PASS.


## Light layout polish — v0.17

Current version: `0.17-light-layout-polish-rc1`.

Phone-review fixes:
- right-biased focal crop keeps the warm floor lamp visible in the real hero photo;
- Brightness and Temperature summary cards are now identical in height, padding and value typography.

Implementation:
- `ui/FocalCropImageView.kt`;
- no AppCompat/third-party dependency.

Final Actions run `37110520082`: contract tests PASS, debug APK PASS.


## Music framing/icon hotfix — v0.20.2

Current Android version: `0.20.2-music-framing-icon-hotfix-rc1`.

Owner follow-up fixes:
- Music hero no longer renders any `centerCrop` copy of the dedicated photo; the 250dp hero uses one complete `fitCenter` image over the dark hero surface;
- M05 «Частота» uses a distinct radio-frequency/beacon illustration instead of a second waveform similar to M01 «Градиент».

No API/UART/firmware changes.

GitHub Actions Android Verify run `37204187940`: contract tests PASS + debug APK PASS.

Phone check: `git pull` → Android Studio Run → visually confirm full Music hero framing and clear M01/M05 distinction.

## Ambient Home Hub — v0.21

Current Android version: `0.21-ambient-homehub-rc1`.

Ambient/«Фон» now follows the premium Home Hub visual language:
- contextual 250dp hero with connection/status/actions inside;
- current effect summary inside the hero;
- illustrated F01/F02/F03 tiles via `ui/AmbientEffectTileView.kt`;
- mint selected-state border/glow;
- full-width existing ON/OFF actions.

No API/UART/firmware behavior changed.

Phone check:
1. `git pull`;
2. Android Studio → Run;
3. open **Фон**;
4. confirm there is no separate top status rectangle above the Ambient hero;
5. confirm ARDU/status/refresh/settings are inside the hero;
6. confirm Цвет / Смена / Радуга have distinct illustrations and selected mint state follows the selected effect;
7. re-check ON/OFF, sliders and auto-cycle for obvious regressions.

## Home Hub completion — v0.22

Current Android version: `0.22-night-alarm-homehub-rc1`.

All five normal user tabs now use the approved premium language.

### Phone check
1. `git pull`.
2. Android Studio → Run.
3. Open **Ночь**:
   - ARDU/status/refresh/settings must be inside the hero;
   - no separate global status rectangle;
   - summary must show Night state + brightness + schedule/manual state;
   - verify color/saturation/brightness and schedule save still work visually.
4. Open **Будильник**:
   - ARDU/status/refresh/settings must be inside the dawn hero;
   - summary must show alarm time/state and RTC status;
   - HH:MM editor should be large and balanced;
   - fade/brightness/start/end color controls must remain intact;
   - RTC sync and Stop Dawn buttons remain available.
5. Confirm bottom dock still switches all five user tabs normally.

No API/UART/firmware changes are part of v0.22.

## Concept parity RC — v0.23

Current version: `0.23-concept-parity-rc1`.

Rollback/reference:
`archive/android-v0.22-homehub-2026-10-04`.

Phone check:
1. `git pull`.
2. Android Studio → Run.
3. Keep the phone on the real ARDU Wi‑Fi network.
4. Compare Light / Music / Ambient / Night / Alarm with the concept screenshots.
5. Verify real control smoke:
   - Light overview brightness and Kelvin;
   - one Music mode;
   - one Ambient effect;
   - Night +/- brightness;
   - Alarm enable/time display.
6. If a v0.23 layout is worse, use the archived v0.22 branch as the exact previous baseline.

## ESP8266 v1 local credentials

Final ESP source no longer requires editing the tracked `esp8266_ardu_v1.ino`.

For the next ESP compile/OTA only, keep real Wi-Fi/OTA/ARDU-DIRECT values in:

`05_WiFi_и_приложение/esp8266_ardu_v1/wifi_secrets.h`

Create it locally from `wifi_secrets.example.h`. The real file is ignored by Git. Therefore `git pull` and branch changes do not overwrite credentials and the tracked sketch remains clean.

## Restored visual baseline — v0.22.1

Current Android version: `0.22.1-homehub-restored-rc1`.

Owner rejected v0.23 and selected the previous Home Hub implementation.

The canonical `main` now contains the restored v0.22 visual UI while retaining newer ARDU-DIRECT network behavior.

If currently on the archive branch:
```powershell
git switch main
git pull
```

Then Android Studio → Run.

Expected appearance = the previous v0.22 version the owner returned to manually.

## Owner polish — v0.22.2 emulator review

Current source version: `0.22.2-homehub-owner-polish-rc1`.

For this gate do **not** upload Nano/ESP yet.

1. `git pull`.
2. Android Studio → select Pixel emulator → Run.
3. Review visually:
   - bottom menu labels;
   - Свет overview ON/OFF + scenes;
   - Light detail active ON/OFF state;
   - Settings Back / ARDU-DIRECT / reset-defaults card;
   - Music active-state behavior;
   - Фон dedicated hero + F01/F02/F03 + four presets;
   - Ночь dedicated hero;
   - Будильник dedicated hero.
4. Report visual issues page by page.

Firmware source also contains reset-defaults + Ambient speed fix, but these are intentionally not a requirement for emulator visual review.

## Hero scale hotfix — v0.22.3

Ambient / Night / Alarm hero photos are intentionally shown farther away than v0.22.2 while preserving the same 250dp card and all overlay controls.

Phone/emulator check: `git pull` → Run → compare only Фон / Ночь / Будильник first.

## Generated heroes — v0.22.4

Version: `0.22.4-generated-heroes-rc1`.

Owner-approved new scenes are installed for:
- Фон;
- Ночь;
- Будильник.

Check:
1. `git pull`;
2. Android Studio → Pixel emulator → Run;
3. open only **Фон → Ночь → Будильник** first;
4. verify the room compositions feel natural behind the existing text/status overlay.

Ambient preset behavior is a separate design task; the current four choices are not final.

## Ambient hero asset hotfix — v0.22.5

After owner emulator screenshot showed visual corruption, only the Ambient hero WebP was re-encoded from approved room art.

`git pull` → Android Studio → **Run** in the Pixel emulator → open **Фон** and confirm the TV with cyan/magenta ambient backlight is visible. The Night/Alarm backgrounds and preset behavior are unchanged.

Current version: `0.22.5-ambient-hero-hotfix-rc1`, versionCode 28.
